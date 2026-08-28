#pragma once

#include <QImage>
#include <QString>

#include <cstddef>

void guiCreate();
void guiSetCameraStatus(const QString &text);
void guiShowFrame(const QImage &image, std::size_t bytes, std::size_t number);
