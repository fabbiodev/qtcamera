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

namespace {

constexpr int kFrameWidth = 256;          // ширина полезного кадра
constexpr int kFrameHeight = 192;         // высота полезного кадра
constexpr int kFrameRows = 196;           // высота буфера в драйвере
constexpr int kBufferCount = 4;           // сколько буферов держим в очереди
constexpr int kPollIntervalMs = 10;       // частота опроса V4L2
constexpr char kVideoDevice[] = "/dev/video0"; // куда открываем поток

struct Buffer {
    void *data = nullptr;  // адрес mmap
    size_t size = 0;       // размер mmap
};

struct FrameStats {
    std::uint16_t minValue = 0; // минимум по кадру
    std::uint16_t maxValue = 0; // максимум по кадру
    double average = 0.0;       // среднее значение
    size_t expectedBytes = 0;   // сколько байт ждём
};

// ioctl с повтором при EINTR
int xioctl(int fd, unsigned long request, void *arg)
{
    int rc = 0;
    do { rc = ::ioctl(fd, request, arg); } while (rc == -1 && errno == EINTR);
    return rc;
}

// Читаем один 16-битный сэмпл из сырого кадра
std::uint16_t readSample(const std::uint8_t *frameData, int pixelIndex)
{
    const size_t offset = static_cast<size_t>(pixelIndex) * sizeof(std::uint16_t);
    return static_cast<std::uint16_t>(frameData[offset] | (frameData[offset + 1] << 8));
}

// Считаем базовые метрики кадра
FrameStats calculateFrameStats(const std::uint8_t *frameData)
{
    FrameStats stats{std::numeric_limits<std::uint16_t>::max(), 0, 0.0, static_cast<size_t>(kFrameWidth) * static_cast<size_t>(kFrameHeight) * sizeof(std::uint16_t)};
    std::uint64_t sum = 0;
    for (int index = 0, pixelCount = kFrameWidth * kFrameHeight; index < pixelCount; ++index) {
        const std::uint16_t sample = readSample(frameData, index);
        stats.minValue = std::min(stats.minValue, sample);
        stats.maxValue = std::max(stats.maxValue, sample);
        sum += sample;
    }
    stats.average = static_cast<double>(sum) / static_cast<double>(kFrameWidth * kFrameHeight);
    return stats;
}

} // namespace

class ReaderWindow final : public QWidget
{
public:
    // Создаём окно и сразу запускаем чтение
    ReaderWindow()
    {
        auto *layout = new QVBoxLayout(this);                  // вертикальный контейнер
        statusLabel_ = new QLabel("Opening /dev/video0...", this); // стартовый текст
        statusLabel_->setWordWrap(true);
        layout->addWidget(statusLabel_);
        resize(720, 160);                                      // компактное окно
        setWindowTitle("Raw reader");                          // имя окна
        QTimer::singleShot(0, this, &ReaderWindow::startReader); // старт после показа
    }

    // Гасим поток и освобождаем буферы
    ~ReaderWindow() override { stopReader(); }

private:
    QLabel *statusLabel_ = nullptr; // строка состояния
    int videoFd_ = -1;              // дескриптор /dev/video0
    bool streaming_ = false;        // поток уже включён
    std::vector<Buffer> buffers_;   // mmap-буферы
    QTimer *pollTimer_ = nullptr;   // таймер чтения

    // Собираем весь запуск чтения в один проход
    void startReader() { openVideoDevice(); configureVideoFormat(); allocateBuffers(); queueBuffers(); startStreaming(); startPolling(); }

    // Открываем видеоустройство
    void openVideoDevice() { videoFd_ = ::open(kVideoDevice, O_RDWR | O_NONBLOCK); }

    // Ставим формат потока
    void configureVideoFormat()
    {
        v4l2_format format {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = kFrameWidth;
        format.fmt.pix.height = kFrameRows;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        format.fmt.pix.field = V4L2_FIELD_NONE;
        xioctl(videoFd_, VIDIOC_S_FMT, &format);
    }

    // Запрашиваем mmap-буферы
    void allocateBuffers()
    {
        v4l2_requestbuffers request {};
        request.count = kBufferCount;
        request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        request.memory = V4L2_MEMORY_MMAP;
        xioctl(videoFd_, VIDIOC_REQBUFS, &request);
        buffers_.resize(request.count);
        for (std::size_t index = 0; index < buffers_.size(); ++index) {
            buffers_[index] = mapBuffer(index);
        }
    }

    // Привязываем один буфер к памяти
    Buffer mapBuffer(std::size_t index)
    {
        v4l2_buffer buffer {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = static_cast<__u32>(index);
        xioctl(videoFd_, VIDIOC_QUERYBUF, &buffer);
        Buffer mappedBuffer;
        mappedBuffer.size = buffer.length;
        mappedBuffer.data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, videoFd_, static_cast<off_t>(buffer.m.offset));
        return mappedBuffer;
    }

    // Кладём все буферы в очередь драйвера
    void queueBuffers()
    {
        for (std::size_t index = 0; index < buffers_.size(); ++index) {
            queueBuffer(index);
        }
    }

    // Возвращаем один буфер обратно в драйвер
    void queueBuffer(std::size_t index)
    {
        v4l2_buffer buffer {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = static_cast<__u32>(index);
        xioctl(videoFd_, VIDIOC_QBUF, &buffer);
    }

    // Включаем поток
    void startStreaming()
    {
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(videoFd_, VIDIOC_STREAMON, &type);
        streaming_ = true;
    }

    // Запускаем таймер чтения
    void startPolling()
    {
        pollTimer_ = new QTimer(this);
        connect(pollTimer_, &QTimer::timeout, this, &ReaderWindow::pollFrames);
        pollTimer_->start(kPollIntervalMs);
        statusLabel_->setText("Reading frames...");
    }

    // Читаем столько кадров, сколько сейчас готово
    void pollFrames() { while (readNextFrame()) {} }

    // Забираем один буфер из драйвера
    bool readNextFrame()
    {
        v4l2_buffer buffer {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        if (xioctl(videoFd_, VIDIOC_DQBUF, &buffer) < 0) {
            return false;
        }
        showFrameStats(buffer);
        queueBuffer(buffer.index);
        return true;
    }

    // Показываем метрики кадра
    void showFrameStats(const v4l2_buffer &buffer)
    {
        if (buffer.index >= buffers_.size()) {
            return;
        }
        const auto *frameData = static_cast<const std::uint8_t *>(buffers_[buffer.index].data); // сырой кадр
        const FrameStats stats = calculateFrameStats(frameData); // считаем цифры
        setWindowTitle(QString("Raw reader | avg %1 | min %2 | max %3").arg(stats.average, 0, 'f', 1).arg(stats.minValue).arg(stats.maxValue));
        statusLabel_->setText(QString("bytesused=%1 expected=%2 tail=%3").arg(static_cast<qulonglong>(buffer.bytesused)).arg(static_cast<qulonglong>(stats.expectedBytes)).arg(static_cast<qulonglong>(buffer.bytesused - stats.expectedBytes)));
    }

    // Гасим всё по порядку
    void stopReader() { stopPolling(); stopStreaming(); unmapBuffers(); closeVideoDevice(); }

    // Останавливаем таймер
    void stopPolling()
    {
        if (pollTimer_ == nullptr) {
            return;
        }
        pollTimer_->stop();
        pollTimer_->deleteLater();
        pollTimer_ = nullptr;
    }

    // Выключаем поток
    void stopStreaming()
    {
        if (!streaming_ || videoFd_ < 0) {
            return;
        }
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(videoFd_, VIDIOC_STREAMOFF, &type);
        streaming_ = false;
    }

    // Освобождаем mmap
    void unmapBuffers()
    {
        for (const auto &buffer : buffers_) {
            if (buffer.data != nullptr && buffer.data != MAP_FAILED) {
                ::munmap(buffer.data, buffer.size);
            }
        }
        buffers_.clear();
    }

    // Закрываем устройство
    void closeVideoDevice()
    {
        if (videoFd_ < 0) {
            return;
        }
        ::close(videoFd_);
        videoFd_ = -1;
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ReaderWindow window;
    window.show();
    return app.exec();
}
