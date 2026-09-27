#include "data_processing.h"

#include <algorithm>

using namespace std;

QImage dataProcessingConvertRawFrame(const RawFrame &frame, int width, int rawHeight,
                                     int imageHeight, int blackLevel, int whiteLevel)
{
    constexpr size_t bytesPerPixel = 2;                 // камера отдаёт 16 бит на пиксель
    constexpr int bytesPerRgbPixel = 3;                 // R, G и B занимают по одному байту
    if (width <= 0 || rawHeight <= 0 || imageHeight <= 0 || imageHeight > rawHeight ||
        blackLevel >= whiteLevel) {                     // проверяем настройки из main.cpp
        return {};
    }
    const size_t required = static_cast<size_t>(width) * rawHeight * bytesPerPixel;
    if (frame.size() < required) {                      // неполный кадр показывать нельзя
        return {};
    }

    QImage image(width, imageHeight, QImage::Format_RGB888); // три канала RGB по 8 бит
    for (int y = 0; y < imageHeight; ++y) {            // пропускаем служебные строки RAW-буфера
        auto *row = image.scanLine(y);                  // строка изображения Qt
        for (int x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * width + x) * bytesPerPixel;
            const int raw = frame[offset] | (static_cast<int>(frame[offset + 1]) << 8); // little-endian
            const int clipped = clamp(raw, blackLevel, whiteLevel); // без автоконтраста
            const uchar level = static_cast<uchar>((clipped - blackLevel) * 255 /
                                                   (whiteLevel - blackLevel)); // чёрный..белый -> 0..255
            uchar *pixel = row + x * bytesPerRgbPixel; // начало RGB-пикселя в строке

            Color_skin_1(pixel, level);                 // красим пиксель по схеме "Радуга"
        }
    }
    return image;
}

// Покраска пикселей по схеме "Радуга".
// Функция получает адрес RGB-пикселя и уровень яркости от 0 до 255.
inline void Color_skin_1(uchar *pixel, uchar level)
{
    if (level < 64) {
        pixel[0] = 0;                                   // R
        pixel[1] = level * 4;                           // G
        pixel[2] = 255;                                 // B
    } else if (level < 128) {
        pixel[0] = 0;                                   // R
        pixel[1] = 255;                                 // G
        pixel[2] = 255 - (level - 64) * 4;              // B
    } else if (level < 192) {
        pixel[0] = (level - 128) * 4;                   // R
        pixel[1] = 255;                                 // G
        pixel[2] = 0;                                   // B
    } else {
        pixel[0] = 255;                                 // R
        pixel[1] = 255 - (level - 192) * 4;             // G
        pixel[2] = 0;                                   // B
    }
}
