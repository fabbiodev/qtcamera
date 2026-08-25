#pragma once

#include "irsensor.h"

// VideoStream отвечает за создание и обработку видеопотока из сырых кадров.
// Пока это заготовка: функция принимает кадр, но ничего с ним не делает.
namespace VideoStream {

void submitRawFrame(const IRSensor::RawFrame &frame);   // передать сырой кадр в видеопоток

} // namespace VideoStream
