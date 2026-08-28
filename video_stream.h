#pragma once

#include "irsensor.h"

// Создание и обработка видеопотока из сырых кадров. Без класса и без namespace:
// одна свободная функция с префиксом videoStream. Пока это заготовка: функция
// принимает кадр, но ничего с ним не делает.

void videoStreamSubmitRawFrame(const RawFrame &frame);  // передать сырой кадр в видеопоток
