#pragma once

#include "irsensor.h"

#include <QImage>

// Преобразование RAW-данных камеры в 8-битное RGB-изображение.
QImage dataProcessingConvertRawFrame(const RawFrame &frame, int width, int rawHeight,
                                     int imageHeight, int blackLevel, int whiteLevel);
