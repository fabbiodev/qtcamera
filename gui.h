#pragma once

#include <cstddef>

#include <QString>

// Интерфейс без класса окна: свободные функции в пространстве имён Gui.
// Сами виджеты создаются и хранятся внутри gui.cpp.
namespace Gui {

void create();                                          // создать и показать окно
void setCameraStatus(const QString &text);              // обновить строку состояния камеры
void showFrame(std::size_t bytes, std::size_t number);  // показать данные о новом кадре

} // namespace Gui
