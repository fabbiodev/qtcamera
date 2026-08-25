#pragma once

#include "irsensor.h"

// DataProcessing — алгоритм обработки данных. Получает итоговый массив
// с RAW-данными, полученными с камеры. Пока это пустая заготовка.
namespace DataProcessing {

void submitRawFrame(const IRSensor::RawFrame &frame);   // передать RAW-массив на обработку

} // namespace DataProcessing
