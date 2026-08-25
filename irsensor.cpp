#include "irsensor.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace {

constexpr int kBufferCount = 4;                         // сколько буферов просим у драйвера
constexpr int kFrameWidth = 256;                        // ширина полезной части кадра
constexpr int kFrameRows = 196;                         // полная высота буфера камеры

} // namespace

IRSensor::~IRSensor()
{
    stop();                                             // гарантированно освобождаем ресурсы
}

// Обёртка над ioctl: повторяем вызов, если его прервал сигнал (EINTR).
bool IRSensor::call(unsigned long request, void *arg) const
{
    int rc = 0;                                         // результат системного вызова
    do {
        rc = ::ioctl(fd_, request, arg);                // отправляем команду драйверу
    } while (rc == -1 && errno == EINTR);               // повторяем только после прерывания
    return rc != -1;                                    // true, если вызов удался
}

// Возвращаем буфер драйверу, чтобы камера могла записать в него следующий кадр.
bool IRSensor::queue(std::size_t index)
{
    v4l2_buffer buffer {};                              // структура описания буфера
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // буфер для захвата видео
    buffer.memory = V4L2_MEMORY_MMAP;                   // память через mmap
    buffer.index = static_cast<__u32>(index);           // номер буфера в очереди
    return call(VIDIOC_QBUF, &buffer);                  // ставим буфер в очередь
}

// Запоминаем текст ошибки вместе с системным описанием errno.
void IRSensor::fail(const char *message)
{
    error_ = message;                                   // человекочитаемая причина
    error_ += ": ";                                     // разделитель
    error_ += std::strerror(errno);                     // системное описание кода ошибки
}

// Открываем устройство камеры, настраиваем формат, выделяем и запускаем буферы.
bool IRSensor::start(const std::string &device)
{
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
        return false;
    }
    width_ = static_cast<int>(format.fmt.pix.width);    // запоминаем принятую ширину
    height_ = static_cast<int>(format.fmt.pix.height);  // и принятую высоту буфера

    v4l2_requestbuffers request {};                     // запрос на выделение буферов
    request.count = kBufferCount;                       // просим четыре буфера
    request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;         // буферы для видеозахвата
    request.memory = V4L2_MEMORY_MMAP;                  // доступ через mmap
    if (!call(VIDIOC_REQBUFS, &request)) {              // просим драйвер выделить память
        fail("VIDIOC_REQBUFS");
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
            return false;
        }

        buffers_[index].size = buffer.length;           // сохраняем размер для munmap
        buffers_[index].data = ::mmap(nullptr, buffer.length, PROT_READ | PROT_WRITE,
                                      MAP_SHARED, fd_, static_cast<off_t>(buffer.m.offset));
        if (buffers_[index].data == MAP_FAILED) {       // отображение памяти не удалось
            buffers_[index].data = nullptr;
            fail("mmap");
            return false;
        }
        if (!queue(index)) {                            // ставим буфер в очередь камеры
            fail("VIDIOC_QBUF");
            return false;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;             // тип запускаемого потока
    if (!call(VIDIOC_STREAMON, &type)) {                // включаем передачу кадров
        fail("VIDIOC_STREAMON");
        return false;
    }
    return true;                                        // камера успешно запущена
}

// Забираем один готовый кадр, копируем его в frame и возвращаем буфер камере.
bool IRSensor::readFrame(RawFrame &frame)
{
    v4l2_buffer buffer {};                              // сюда драйвер запишет данные о кадре
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;          // тип буфера видеозахвата
    buffer.memory = V4L2_MEMORY_MMAP;                   // способ работы с памятью
    if (!call(VIDIOC_DQBUF, &buffer)) {                 // пытаемся забрать готовый кадр
        return false;                                   // EAGAIN - кадра ещё нет, это нормально
    }

    if (buffer.index < buffers_.size()) {               // индекс буфера корректен
        const auto *data = static_cast<const std::uint8_t *>(buffers_[buffer.index].data);
        frame.assign(data, data + buffer.bytesused);    // копируем весь сырой кадр в массив
    }
    queue(buffer.index);                                // возвращаем буфер камере
    return true;                                        // кадр получен
}

// Останавливаем поток, отвязываем буферы и закрываем устройство.
void IRSensor::stop()
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

// Отвязываем от памяти все ранее отображённые буферы.
void IRSensor::releaseBuffers()
{
    for (Buffer &buffer : buffers_) {                   // проходим по всем буферам
        if (buffer.data != nullptr) {                   // если буфер был отображён
            ::munmap(buffer.data, buffer.size);         // возвращаем память системе
        }
    }
    buffers_.clear();                                   // очищаем список буферов
}
