#include "data_processing.h"

#include <algorithm>

namespace {
constexpr int kWidth = 256;
constexpr int kRawHeight = 196;
constexpr int kImageHeight = 192;
constexpr int kBlackLevel = 4700;
constexpr int kWhiteLevel = 5500;
}

QImage dataProcessingConvertRawFrame(const RawFrame &frame)
{
    constexpr std::size_t bytesPerPixel = 2;
    const std::size_t required = static_cast<std::size_t>(kWidth) * kRawHeight * bytesPerPixel;
    if (frame.size() < required) {
        return {};
    }

    QImage image(kWidth, kImageHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < kImageHeight; ++y) {
        auto *row = image.scanLine(y);
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * kWidth + x) * bytesPerPixel;
            const int raw = frame[offset] | (static_cast<int>(frame[offset + 1]) << 8);
            const int clipped = std::clamp(raw, kBlackLevel, kWhiteLevel);
            row[x] = static_cast<uchar>((clipped - kBlackLevel) * 255 /
                                         (kWhiteLevel - kBlackLevel));
        }
    }
    return image;
}
