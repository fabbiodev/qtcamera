#include "data_processing.h"

#include <algorithm>

namespace {
constexpr int kWidth = 256;                             // ширина кадра, заданная камерой
constexpr int kRawHeight = 196;                         // строк в транспортном RAW-буфере
constexpr int kImageHeight = 192;                       // строки, выводимые в видеоролик
constexpr int kBlackLevel = 4700;                       // RAW-значение для чёрного цвета
constexpr int kWhiteLevel = 5500;                       // RAW-значение для белого цвета
}

QImage dataProcessingConvertRawFrame(const RawFrame &frame)
{
    constexpr std::size_t bytesPerPixel = 2;            // камера отдаёт 16 бит на пиксель
    const std::size_t required = static_cast<std::size_t>(kWidth) * kRawHeight * bytesPerPixel;
    if (frame.size() < required) {                      // неполный кадр показывать нельзя
        return {};
    }

    QImage image(kWidth, kImageHeight, QImage::Format_Grayscale8); // 8 бит для показа
    for (int y = 0; y < kImageHeight; ++y) {           // пропускаем служебные строки RAW-буфера
        auto *row = image.scanLine(y);                  // строка изображения Qt
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * kWidth + x) * bytesPerPixel;
            const int raw = frame[offset] | (static_cast<int>(frame[offset + 1]) << 8); // little-endian
            const int clipped = std::clamp(raw, kBlackLevel, kWhiteLevel); // без автоконтраста
            row[x] = static_cast<uchar>((clipped - kBlackLevel) * 255 /    // 4700..5500 -> 0..255
                                         (kWhiteLevel - kBlackLevel));
        }
    }
    return image;
}
