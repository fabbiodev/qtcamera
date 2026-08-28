#pragma once

#include "irsensor.h"

#include <QImage>

QImage dataProcessingConvertRawFrame(const RawFrame &frame);
