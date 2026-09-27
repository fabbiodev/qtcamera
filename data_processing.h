#pragma once

#include "irsensor.h"

#include <QImage>

// Преобразование RAW-данных камеры в 8-битное RGB-изображение.
QImage dataProcessingConvertRawFrame(const RawFrame &frame, int width, int rawHeight,
                                     int imageHeight, int blackLevel, int whiteLevel);

// Покраска пикселей по схеме "Радуга".
inline void Color_skin_1(uchar *pixel, uchar level);
