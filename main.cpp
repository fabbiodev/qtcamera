#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <limits>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

using namespace std;

namespace {

constexpr int kFrameWidth = 256;                    // ширина полезной части кадра в пикселях
constexpr int kFrameHeight = 192;                   // высота полезной части кадра в пикселях
constexpr int kFrameRows = 196;                     // полная высота буфера, которую отдаёт камера
constexpr int kBufferCount = 4;                     // количество буферов, которые просим у драйвера
constexpr int kPollIntervalMs = 10;                 // через сколько миллисекунд проверяем новый кадр
constexpr char kVideoDevice[] = "/dev/video0";      // файл устройства камеры в Linux

// Храним адрес и размер одного буфера, который драйвер выделил под кадр.
struct Buffer {
    void *data = nullptr;                            // адрес начала памяти буфера
    size_t size = 0;                                 // размер выделенной области памяти
};

// Храним результаты, которые посчитали по одному полезному кадру.
struct FrameStats {
    uint16_t minValue = 0;                           // самое маленькое значение в кадре
    uint16_t maxValue = 0;                           // самое большое значение в кадре
    double average = 0.0;                            // среднее значение по всем пикселям
    size_t expectedBytes = 0;                        // сколько байт занимает полезный кадр
};

// Выполняем ioctl и повторяем его, если системный вызов временно прервался.
int xioctl(int fd, unsigned long request, void *arg)
{
    int rc = 0;                                      // результат выполнения ioctl
    do {
        rc = ::ioctl(fd, request, arg);              // отправляем команду драйверу камеры
    } while (rc == -1 && errno == EINTR);            // повторяем только после прерывания сигнала
    return rc;                                       // возвращаем результат драйверу вызывающей функции
}

// Собираем одно значение пикселя из двух байт, в которых камера переносит свои RAW-данные.
uint16_t readSample(const uint8_t *frameData, int pixelIndex)
{
    const size_t offset = static_cast<size_t>(pixelIndex) * sizeof(uint16_t); // позиция двух байтов пикселя в массиве
    const uint16_t lowByte = frameData[offset];                              // первые 8 бит лежат без сдвига
    const uint16_t highByte = static_cast<uint16_t>(frameData[offset + 1]) << 8; // вторые 8 бит ставим в старшую часть
    return static_cast<uint16_t>(lowByte | highByte);                         // получаем контейнер uint16_t с RAW-значением
}

// Проходим по всему кадру и определяем минимум, максимум и среднее значение.
FrameStats calculateFrameStats(const uint8_t *frameData)
{
    FrameStats stats;                                                          // сюда записываем итоговую статистику
    stats.minValue = numeric_limits<uint16_t>::max();                         // начинаем поиск минимума с самого большого uint16_t
    stats.expectedBytes = static_cast<size_t>(kFrameWidth) * kFrameHeight * sizeof(uint16_t); // размер 256x192x2

    uint64_t sum = 0;                                                          // сумма нужна для вычисления среднего
    const int pixelCount = kFrameWidth * kFrameHeight;                         // общее количество значений в кадре
    for (int index = 0; index < pixelCount; ++index) {
        const uint16_t sample = readSample(frameData, index);                   // читаем значение текущего пикселя
        stats.minValue = min(stats.minValue, sample);                           // оставляем меньшее из двух значений
        stats.maxValue = max(stats.maxValue, sample);                           // оставляем большее из двух значений
        sum += sample;                                                           // добавляем значение в общую сумму
    }

    stats.average = static_cast<double>(sum) / pixelCount;                      // переводим сумму в double и делим на число пикселей
    return stats;                                                               // возвращаем готовую статистику кадра
}

// Окно само запускает чтение камеры, показывает статистику и освобождает ресурсы при закрытии.
class ReaderWindow final : public QWidget
{
public:
    // Создаём окно, добавляем строку состояния и запускаем чтение после показа окна.
    ReaderWindow()
    {
        auto *layout = new QVBoxLayout(this);                                   // вертикальный контейнер Qt
        statusLabel_ = new QLabel("Opening /dev/video0...", this);             // текст текущего состояния
        statusLabel_->setWordWrap(true);                                        // разрешаем перенос длинной строки
        layout->addWidget(statusLabel_);                                        // помещаем строку в окно

        resize(720, 160);                                                       // задаём начальный размер окна
        setWindowTitle("Raw reader");                                          // задаём начальный заголовок
        QTimer::singleShot(0, this, &ReaderWindow::startReader);                // запускаем камеру после создания окна
    }

    // Останавливаем поток и освобождаем память перед уничтожением окна.
    ~ReaderWindow() override
    {
        stopReader();                                                           // закрываем камеру и mmap-буферы
    }

private:
    QLabel *statusLabel_ = nullptr;                                             // указатель на текст состояния
    QTimer *pollTimer_ = nullptr;                                               // таймер регулярного опроса камеры
    int videoFd_ = -1;                                                          // файловый дескриптор /dev/video0
    bool streaming_ = false;                                                    // флаг включённого видеопотока
    vector<Buffer> buffers_;                                                     // список отображённых в память буферов

    // Возвращаем буфер обратно драйверу, чтобы камера могла записать в него следующий кадр.
    void queueBuffer(size_t index)
    {
        v4l2_buffer buffer {};                                                   // структура описания одного буфера V4L2
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                              // буфер предназначен для захвата видео
        buffer.memory = V4L2_MEMORY_MMAP;                                       // память буфера отображается через mmap
        buffer.index = static_cast<__u32>(index);                               // номер буфера в очереди драйвера
        xioctl(videoFd_, VIDIOC_QBUF, &buffer);                                 // ставим буфер в очередь на заполнение
    }

    // Получаем адрес одного буфера и подключаем память драйвера к нашей программе.
    Buffer mapBuffer(size_t index)
    {
        v4l2_buffer buffer {};                                                   // описание буфера, которое заполняет драйвер
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                              // тип операции - получение видеоданных
        buffer.memory = V4L2_MEMORY_MMAP;                                       // используем память, выделенную драйвером
        buffer.index = static_cast<__u32>(index);                               // выбираем нужный буфер по номеру
        xioctl(videoFd_, VIDIOC_QUERYBUF, &buffer);                             // узнаём размер и смещение буфера

        Buffer mappedBuffer;                                                     // наш объект с адресом и размером памяти
        mappedBuffer.size = buffer.length;                                      // сохраняем размер, чтобы потом вызвать munmap
        mappedBuffer.data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, // подключаем память к адресному пространству
                                   MAP_SHARED, videoFd_, static_cast<off_t>(buffer.m.offset)); // используем смещение от драйвера
        return mappedBuffer;                                                     // возвращаем подключённый буфер
    }

    // Читаем кадр из буфера, считаем значения и показываем результат в окне.
    void showFrameStats(const v4l2_buffer &buffer)
    {
        if (buffer.index >= buffers_.size()) {                                  // проверяем, что индекс существует
            return;                                                             // неизвестный буфер не обрабатываем
        }

        const auto *frameData = static_cast<const uint8_t *>(buffers_[buffer.index].data); // адрес полученного кадра
        const FrameStats stats = calculateFrameStats(frameData);                 // считаем min, max и average
        setWindowTitle(QString("Raw reader | avg %1 | min %2 | max %3")        // формируем заголовок окна
                           .arg(stats.average, 0, 'f', 1)                       // добавляем среднее с одним знаком после точки
                           .arg(stats.minValue)                                 // добавляем минимум
                           .arg(stats.maxValue));                               // добавляем максимум
        statusLabel_->setText(QString("bytesused=%1 expected=%2 tail=%3")       // показываем размеры полученного кадра
                                  .arg(static_cast<qulonglong>(buffer.bytesused)) // сколько байт реально сообщил драйвер
                                  .arg(static_cast<qulonglong>(stats.expectedBytes)) // сколько байт занимает полезные данные
                                  .arg(static_cast<qulonglong>(buffer.bytesused - stats.expectedBytes))); // хвост после полезного кадра
    }

    // Забираем один готовый буфер у драйвера и сразу возвращаем его после обработки.
    bool readNextFrame()
    {
        v4l2_buffer buffer {};                                                   // сюда драйвер запишет информацию о готовом кадре
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                              // тип буфера захвата видео
        buffer.memory = V4L2_MEMORY_MMAP;                                       // способ работы с памятью
        if (xioctl(videoFd_, VIDIOC_DQBUF, &buffer) < 0) {                      // забираем готовый буфер из очереди
            return false;                                                        // готового кадра пока нет
        }

        showFrameStats(buffer);                                                  // читаем и анализируем данные кадра
        queueBuffer(buffer.index);                                               // возвращаем буфер камере
        return true;                                                             // сообщаем, что кадр обработан
    }

    // Обрабатываем все кадры, которые успели накопиться к моменту срабатывания таймера.
    void pollFrames()
    {
        while (readNextFrame()) {                                                // читаем, пока драйвер отдаёт готовые буферы
        }
    }

    // Открываем камеру, настраиваем формат, создаём буферы и запускаем поток.
    void startReader()
    {
        videoFd_ = ::open(kVideoDevice, O_RDWR | O_NONBLOCK);                    // открываем устройство без блокировки

        v4l2_format format {};                                                    // структура настроек формата кадра
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                               // выбираем видеозахват
        format.fmt.pix.width = kFrameWidth;                                      // просим ширину 256 пикселей
        format.fmt.pix.height = kFrameRows;                                      // просим высоту буфера 196 строк
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;                          // формат транспортного USB-видеопотока
        format.fmt.pix.field = V4L2_FIELD_NONE;                                  // без чересстрочного поля
        xioctl(videoFd_, VIDIOC_S_FMT, &format);                                // передаём настройки драйверу

        v4l2_requestbuffers request {};                                          // запрос на выделение буферов
        request.count = kBufferCount;                                            // просим четыре буфера
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                              // буферы для видеозахвата
        request.memory = V4L2_MEMORY_MMAP;                                       // буферы будут доступны через mmap
        xioctl(videoFd_, VIDIOC_REQBUFS, &request);                              // просим драйвер выделить память

        buffers_.resize(request.count);                                          // создаём такой же список в программе
        for (size_t index = 0; index < buffers_.size(); ++index) {
            buffers_[index] = mapBuffer(index);                                  // подключаем буфер к памяти программы
            queueBuffer(index);                                                  // ставим его в очередь камеры
        }

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                                  // тип потока, который запускаем
        xioctl(videoFd_, VIDIOC_STREAMON, &type);                                // включаем передачу кадров
        streaming_ = true;                                                       // запоминаем, что поток включён

        pollTimer_ = new QTimer(this);                                           // создаём таймер Qt
        connect(pollTimer_, &QTimer::timeout, this, &ReaderWindow::pollFrames);   // связываем таймер с чтением
        pollTimer_->start(kPollIntervalMs);                                      // запускаем периодический вызов
        statusLabel_->setText("Reading frames...");                             // сообщаем, что поток читается
    }

    // Останавливаем таймер и поток, освобождаем mmap и закрываем устройство.
    void stopReader()
    {
        if (pollTimer_ != nullptr) {
            pollTimer_->stop();                                                  // прекращаем опрос камеры
            pollTimer_ = nullptr;                                                // указатель больше не используем
        }

        if (streaming_ && videoFd_ >= 0) {
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;                              // тип останавливаемого потока
            xioctl(videoFd_, VIDIOC_STREAMOFF, &type);                           // выключаем передачу кадров
            streaming_ = false;                                                  // отмечаем, что поток выключен
        }

        for (const auto &buffer : buffers_) {
            if (buffer.data != nullptr && buffer.data != MAP_FAILED) {           // проверяем, что память была подключена
                ::munmap(buffer.data, buffer.size);                              // отключаем память буфера
            }
        }
        buffers_.clear();                                                         // удаляем описания буферов

        if (videoFd_ >= 0) {
            ::close(videoFd_);                                                    // закрываем файл устройства
            videoFd_ = -1;                                                        // сбрасываем дескриптор
        }
    }
};

} // namespace

// Создаём Qt-приложение, показываем окно и передаём управление циклу событий.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                                                 // создаём Qt-приложение
    ReaderWindow window;                                                          // создаём окно чтения камеры
    window.show();                                                                // показываем окно пользователю
    return app.exec();                                                            // запускаем цикл обработки событий Qt
}
