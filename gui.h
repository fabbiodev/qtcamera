#pragma once

#include <QImage>
#include <QString>

#include <cstddef>

// Интерфейс без класса и namespace: состояние виджетов остаётся внутри gui.cpp.
void guiCreate();                                       // создать и показать главное окно
void guiSetCameraStatus(const QString &text);           // вывести состояние камеры над изображением
void guiShowFrame(const QImage &image, std::size_t bytes, std::size_t number); // показать кадр
