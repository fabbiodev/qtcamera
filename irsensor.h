#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Работа с камерой без класса: просто набор свободных функций в пространстве
// имён IRSensor. Внутреннее состояние (дескриптор, буферы) спрятано в .cpp.
namespace IRSensor {

// Один кадр целиком — это массив байт, как его отдаёт камера.
using RawFrame = std::vector<std::uint8_t>;

bool start(const std::string &device = "/dev/video0"); // открыть и запустить камеру
bool readFrame(RawFrame &frame);                        // прочитать один готовый кадр
void stop();                                            // остановить и освободить ресурсы

int width();                                            // ширина кадра в пикселях
int height();                                           // высота буфера в строках
const std::string &error();                             // текст последней ошибки

} // namespace IRSensor
