#pragma once

#include <QImage>
#include <QString>

#include <cstddef>

using std::size_t;

// Интерфейс без класса и namespace: состояние виджетов остаётся внутри gui.cpp.
void guiCreate(const QString &windowTitle, int minFrameWidth, int minFrameHeight,
               int windowWidth, int windowHeight);     // создать и показать главное окно
void guiSetCameraStatus(const QString &text);           // вывести состояние камеры над изображением
void guiShowFrame(const QImage &image, size_t bytes, size_t number); // показать кадр
