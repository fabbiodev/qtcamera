#include "data_processing.h"

#include <algorithm>

QImage dataProcessingConvertRawFrame(const RawFrame &frame, int width, int rawHeight,
                                     int imageHeight, int blackLevel, int whiteLevel)
{
    constexpr std::size_t bytesPerPixel = 2;            // камера отдаёт 16 бит на пиксель
    if (width <= 0 || rawHeight <= 0 || imageHeight <= 0 || imageHeight > rawHeight ||
        blackLevel >= whiteLevel) {                     // проверяем настройки из main.cpp
        return {};
    }
    const std::size_t required = static_cast<std::size_t>(width) * rawHeight * bytesPerPixel;
    if (frame.size() < required) {                      // неполный кадр показывать нельзя
        return {};
    }

    QImage image(width, imageHeight, QImage::Format_Grayscale8); // 8 бит для показа
    for (int y = 0; y < imageHeight; ++y) {            // пропускаем служебные строки RAW-буфера
        auto *row = image.scanLine(y);                  // строка изображения Qt
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * bytesPerPixel;
            const int raw = frame[offset] | (static_cast<int>(frame[offset + 1]) << 8); // little-endian
            const int clipped = std::clamp(raw, blackLevel, whiteLevel); // без автоконтраста
            row[x] = static_cast<uchar>((clipped - blackLevel) * 255 /    // 4700..5500 -> 0..255
                                         (whiteLevel - blackLevel));
        }
    }
    return image;
}
