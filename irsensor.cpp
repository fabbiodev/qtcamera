#include "irsensor.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// Константы модуля. constexpr на уровне файла уже имеет внутреннюю связь,
// поэтому namespace не нужен.
constexpr int kBufferCount = 4;                         // сколько буферов просим у драйвера
constexpr int kFrameWidth = 256;                        // ширина полезной части кадра
constexpr int kFrameRows = 196;                         // полная высота буфера камеры

// Описание одного буфера, отображённого в память нашей программы.
struct Buffer {
    void *data = nullptr;                               // адрес памяти буфера
    std::size_t size = 0;                               // размер выделенной области
};

// Внутреннее состояние модуля вместо полей класса. static даёт этим переменным
// внутреннюю связь: они видны только в этом файле.
static int fd_ = -1;                                    // дескриптор устройства камеры
static int width_ = 256;                                // ширина полезной части кадра
static int height_ = 196;                               // полная высота буфера камеры
static std::size_t frameSize_ = 0;                      // ожидаемый размер полного кадра
static std::vector<Buffer> buffers_;                    // список отображённых буферов
static std::string error_;                              // последняя ошибка для интерфейса

static bool isDisconnectError(int error)
{
    return error == ENODEV || error == ENXIO || error == EIO || error == EBADF;
}

// Обёртка над ioctl: повторяем вызов, если его прервал сигнал (EINTR).
static bool call(unsigned long request, void *arg)
{
    int rc = 0;                                         // результат системного вызова
    do {
        rc = ::ioctl(fd_, request, arg);                // отправляем команду драйверу
    } while (rc == -1 && errno == EINTR);               // повторяем только после прерывания
    return rc != -1;                                    // true, если вызов удался
}

// Возвращаем буфер драйверу, чтобы камера записала в него следующий кадр.
static bool queue(std::size_t index)
{
    v4l2_buffer buffer {};                              // структура описания буфера
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // буфер для захвата видео
    buffer.memory = V4L2_MEMORY_MMAP;                   // память через mmap
    buffer.index = static_cast<__u32>(index);           // номер буфера в очереди
    return call(VIDIOC_QBUF, &buffer);                  // ставим буфер в очередь
}

// Запоминаем текст ошибки вместе с системным описанием errno.
static void fail(const char *message)
{
    error_ = message;                                   // человекочитаемая причина
    error_ += ": ";                                     // разделитель
    error_ += std::strerror(errno);                     // системное описание кода ошибки
}

// Отвязываем от памяти все ранее отображённые буферы.
static void releaseBuffers()
{
    for (Buffer &buffer : buffers_) {                   // проходим по всем буферам
        if (buffer.data != nullptr) {                   // если буфер был отображён
            ::munmap(buffer.data, buffer.size);         // возвращаем память системе
        }
    }
    buffers_.clear();                                   // очищаем список буферов
}

// Открываем устройство камеры, настраиваем формат, выделяем и запускаем буферы.
bool irsensorStart(const std::string &device)
{
    irsensorStop();                                      // сбрасываем старое подключение
    fd_ = ::open(device.c_str(), O_RDWR | O_NONBLOCK);  // открываем без блокировки чтения
    if (fd_ == -1) {                                    // не удалось открыть устройство
        fail("open");
        return false;
    }

    v4l2_format format {};                              // настройки формата кадра
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // режим видеозахвата
    format.fmt.pix.width = kFrameWidth;                 // ширина 256 пикселей
    format.fmt.pix.height = kFrameRows;                 // высота буфера 196 строк
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;     // транспортный формат USB-видео
    format.fmt.pix.field = V4L2_FIELD_NONE;             // без чересстрочной развёртки
    if (!call(VIDIOC_S_FMT, &format)) {                 // передаём формат драйверу
        fail("VIDIOC_S_FMT");
        irsensorStop();
        return false;
    }
    width_ = static_cast<int>(format.fmt.pix.width);    // запоминаем принятую ширину
    height_ = static_cast<int>(format.fmt.pix.height);  // и принятую высоту буфера
    frameSize_ = format.fmt.pix.sizeimage;              // размер кадра, объявленный драйвером
    if (frameSize_ == 0) {
        frameSize_ = static_cast<std::size_t>(format.fmt.pix.bytesperline) * height_;
    }

    v4l2_requestbuffers request {};                     // запрос на выделение буферов
    request.count = kBufferCount;                       // просим четыре буфера
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;         // буферы для видеозахвата
    request.memory = V4L2_MEMORY_MMAP;                  // доступ через mmap
    if (!call(VIDIOC_REQBUFS, &request)) {              // просим драйвер выделить память
        fail("VIDIOC_REQBUFS");
        irsensorStop();
        return false;
    }

    buffers_.resize(request.count);                     // столько же буферов в программе
    for (std::size_t index = 0; index < buffers_.size(); ++index) {
        v4l2_buffer buffer {};                          // описание одного буфера
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;      // тип - видеозахват
        buffer.memory = V4L2_MEMORY_MMAP;               // память драйвера
        buffer.index = static_cast<__u32>(index);       // номер буфера
        if (!call(VIDIOC_QUERYBUF, &buffer)) {          // узнаём размер и смещение буфера
            fail("VIDIOC_QUERYBUF");
            irsensorStop();
            return false;
        }

        buffers_[index].size = buffer.length;           // сохраняем размер для munmap
        buffers_[index].data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd_, static_cast<off_t>(buffer.m.offset));
        if (buffers_[index].data == MAP_FAILED) {       // отображение памяти не удалось
            buffers_[index].data = nullptr;
            fail("mmap");
            irsensorStop();
            return false;
        }
        if (!queue(index)) {                            // ставим буфер в очередь камеры
            fail("VIDIOC_QBUF");
            irsensorStop();
            return false;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;             // тип запускаемого потока
    if (!call(VIDIOC_STREAMON, &type)) {                // включаем передачу кадров
        fail("VIDIOC_STREAMON");
        irsensorStop();
        return false;
    }
    return true;                                        // камера успешно запущена
}

// Забираем один готовый кадр, копируем его в frame и возвращаем буфер камере.
bool irsensorReadFrame(RawFrame &frame)
{
    v4l2_buffer buffer {};                              // сюда драйвер запишет данные о кадре
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // тип буфера видеозахвата
    buffer.memory = V4L2_MEMORY_MMAP;                   // способ работы с памятью
    if (!call(VIDIOC_DQBUF, &buffer)) {                 // пытаемся забрать готовый кадр
        if (isDisconnectError(errno)) {
            fail("Camera disconnected");
            irsensorStop();
        }
        return false;                                   // EAGAIN - кадра ещё нет, это нормально
    }

    if (buffer.index >= buffers_.size()) {
        fail("Invalid camera buffer index");
        irsensorStop();
        return false;
    }

    if (buffer.flags & V4L2_BUF_FLAG_ERROR) {           // драйвер обнаружил ошибку USB-пакетов
        if (!queue(buffer.index) && isDisconnectError(errno)) {
            fail("Camera disconnected");
            irsensorStop();
        }
        return false;                                   // и не показываем полосатый кадр
    }

    if (buffer.bytesused > buffers_[buffer.index].size ||
        (frameSize_ != 0 && buffer.bytesused != frameSize_)) {
        fail("Invalid camera frame");
        irsensorStop();
        return false;
    }

    const auto *data = static_cast<const std::uint8_t *>(buffers_[buffer.index].data);
    frame.assign(data, data + buffer.bytesused);        // копируем до возврата буфера
    if (!queue(buffer.index)) {                         // только после копирования
        if (isDisconnectError(errno)) {
            fail("Camera disconnected");
            irsensorStop();
        }
        return false;
    }
    return true;                                        // кадр целиком скопирован
}

// Останавливаем поток, отвязываем буферы и закрываем устройство.
void irsensorStop()
{
    if (fd_ == -1) {                                    // камера уже остановлена
        return;
    }
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;             // тип останавливаемого потока
    call(VIDIOC_STREAMOFF, &type);                      // выключаем передачу кадров
    releaseBuffers();                                   // отвязываем память буферов
    ::close(fd_);                                       // закрываем дескриптор устройства
    fd_ = -1;                                           // помечаем камеру как закрытую
}

int irsensorWidth() { return width_; }                  // ширина кадра в пикселях
int irsensorHeight() { return height_; }                // высота буфера в строках
const std::string &irsensorError() { return error_; }   // текст последней ошибки
bool irsensorIsRunning() { return fd_ != -1; }          // состояние подключения
