#pragma once

#include "irsensor.h"

#include <QImage>

// Преобразование RAW-данных камеры в 8-битное чёрно-белое изображение.
QImage dataProcessingConvertRawFrame(const RawFrame &frame);
