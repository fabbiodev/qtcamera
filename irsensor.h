#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Работа с камерой без класса и без namespace: просто набор свободных функций
// с префиксом irsensor. Внутреннее состояние (дескриптор, буферы) спрятано в .cpp.

// Один кадр целиком — это массив байт, как его отдаёт камера.
using RawFrame = std::vector<std::uint8_t>;

bool irsensorStart(const std::string &device = "/dev/video0"); // открыть и запустить камеру
bool irsensorReadFrame(RawFrame &frame);                       // прочитать один готовый кадр
void irsensorStop();                                           // остановить и освободить ресурсы
bool irsensorIsRunning();                                      // камера подключена и поток запущен

int irsensorWidth();                                           // ширина кадра в пикселях
int irsensorHeight();                                          // высота буфера в строках
const std::string &irsensorError();                            // текст последней ошибки
