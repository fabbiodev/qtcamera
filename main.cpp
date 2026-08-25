#include <QApplication>
#include <QLabel>
#include <QTimer>

#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <linux/videodev2.h>
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

// Выполняем ioctl и повторяем его, если системный вызов временно прервался.
int xioctl(int fd, unsigned long request, void *arg)
{
    int rc = 0;                                      // результат выполнения ioctl
    do {
        rc = ::ioctl(fd, request, arg);              // отправляем команду драйверу камеры
    } while (rc == -1 && errno == EINTR);            // повторяем только после прерывания сигнала
    return rc;                                       // возвращаем результат вызывающей функции
}

// Возвращаем буфер обратно драйверу, чтобы камера могла записать в него следующий кадр.
void queueBuffer(int fd, size_t index)
{
    v4l2_buffer buffer {};                           // структура описания одного буфера V4L2
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;       // буфер предназначен для захвата видео
    buffer.memory = V4L2_MEMORY_MMAP;                // память буфера отображается через mmap
    buffer.index = static_cast<__u32>(index);        // номер буфера в очереди драйвера
    xioctl(fd, VIDIOC_QBUF, &buffer);                // ставим буфер в очередь на заполнение
}

// Получаем адрес одного буфера и подключаем память драйвера к нашей программе.
Buffer mapBuffer(int fd, size_t index)
{
    v4l2_buffer buffer {};                           // описание буфера, которое заполнит драйвер
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;       // тип операции - получение видеоданных
    buffer.memory = V4L2_MEMORY_MMAP;                // используем память, выделенную драйвером
    buffer.index = static_cast<__u32>(index);        // выбираем нужный буфер по номеру
    xioctl(fd, VIDIOC_QUERYBUF, &buffer);            // узнаём размер и смещение буфера

    Buffer mapped;                                   // наш объект с адресом и размером памяти
    mapped.size = buffer.length;                     // сохраняем размер, чтобы потом вызвать munmap
    mapped.data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, static_cast<off_t>(buffer.m.offset)); // подключаем память драйвера
    return mapped;                                   // возвращаем подключённый буфер
}

} // namespace

// Создаём Qt-приложение, открываем камеру и по таймеру просто забираем сырые кадры.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                    // создаём Qt-приложение

    QLabel label("...");                             // всё окно - это одна строка состояния
    label.setWindowTitle("Raw reader");              // задаём заголовок окна
    label.resize(360, 60);                           // задаём начальный размер окна
    label.show();                                    // показываем окно пользователю

    const int fd = ::open(kVideoDevice, O_RDWR | O_NONBLOCK); // открываем устройство без блокировки

    v4l2_format format {};                           // структура настроек формата кадра
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;       // выбираем видеозахват
    format.fmt.pix.width = kFrameWidth;              // просим ширину 256 пикселей
    format.fmt.pix.height = kFrameRows;              // просим высоту буфера 196 строк
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // формат транспортного USB-видеопотока
    format.fmt.pix.field = V4L2_FIELD_NONE;          // без чересстрочного поля
    xioctl(fd, VIDIOC_S_FMT, &format);               // передаём настройки драйверу

    v4l2_requestbuffers request {};                  // запрос на выделение буферов
    request.count = kBufferCount;                    // просим четыре буфера
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;      // буферы для видеозахвата
    request.memory = V4L2_MEMORY_MMAP;               // буферы будут доступны через mmap
    xioctl(fd, VIDIOC_REQBUFS, &request);            // просим драйвер выделить память

    vector<Buffer> buffers(request.count);           // создаём такой же список в программе
    for (size_t index = 0; index < buffers.size(); ++index) {
        buffers[index] = mapBuffer(fd, index);       // подключаем буфер к памяти программы
        queueBuffer(fd, index);                      // ставим его в очередь камеры
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // тип потока, который запускаем
    xioctl(fd, VIDIOC_STREAMON, &type);              // включаем передачу кадров

    QTimer timer;                                    // таймер регулярного опроса камеры
    QObject::connect(&timer, &QTimer::timeout, [&]() { // связываем таймер с чтением кадров
        v4l2_buffer buffer {};                       // сюда драйвер запишет данные о готовом кадре
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   // тип буфера захвата видео
        buffer.memory = V4L2_MEMORY_MMAP;            // способ работы с памятью
        while (xioctl(fd, VIDIOC_DQBUF, &buffer) >= 0) { // забираем все готовые кадры из очереди
            if (buffer.index < buffers.size()) {     // проверяем, что индекс существует
                // Весь сырой кадр целиком лежит в этом массиве байт.
                const uint8_t *frameData = static_cast<const uint8_t *>(buffers[buffer.index].data);
                (void)frameData;                     // пока просто получаем кадр, без обработки
                label.setText(QString("frame %1 bytes").arg(buffer.bytesused)); // показываем размер кадра
            }
            queueBuffer(fd, buffer.index);           // возвращаем буфер камере
        }
    });
    timer.start(kPollIntervalMs);                    // запускаем периодический вызов

    return app.exec();                               // запускаем цикл обработки событий Qt
}
