#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Камера отдаёт сырые (RAW) кадры через интерфейс V4L2 в Linux.
// Класс IRSensor скрывает всю работу с драйвером: открытие устройства,
// настройку формата, выделение буферов и чтение готовых кадров.
class IRSensor {
public:
    // Один кадр целиком — это просто массив байт, как его отдаёт камера.
    using RawFrame = std::vector<std::uint8_t>;

    IRSensor() = default;                                   // конструктор по умолчанию
    ~IRSensor();                                            // в деструкторе освобождаем ресурсы

    IRSensor(const IRSensor &) = delete;                    // запрещаем копирование дескриптора
    IRSensor &operator=(const IRSensor &) = delete;         // и присваивание тоже запрещаем

    // Открываем камеру и запускаем поток кадров. Возвращаем false при ошибке.
    bool start(const std::string &device = "/dev/video0");

    // Забираем один готовый кадр в frame. Возвращаем false, если кадра ещё нет.
    bool readFrame(RawFrame &frame);

    // Останавливаем поток и освобождаем все буферы и дескриптор.
    void stop();

    int width() const { return width_; }                   // ширина кадра в пикселях
    int height() const { return height_; }                 // высота буфера в строках
    const std::string &error() const { return error_; }    // текст последней ошибки

private:
    // Описание одного буфера, отображённого в память нашей программы.
    struct Buffer {
        void *data = nullptr;                               // адрес памяти буфера
        std::size_t size = 0;                               // размер выделенной области
    };

    bool call(unsigned long request, void *arg) const;      // обёртка над ioctl с повтором
    bool queue(std::size_t index);                          // вернуть буфер драйверу
    void fail(const char *message);                         // запомнить текст ошибки
    void releaseBuffers();                                  // отвязать память буферов

    int fd_ = -1;                                           // дескриптор устройства камеры
    int width_ = 256;                                       // ширина полезной части кадра
    int height_ = 196;                                      // полная высота буфера камеры
    std::vector<Buffer> buffers_;                           // список отображённых буферов
    std::string error_;                                     // последняя ошибка для интерфейса
};
