#include "gui.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace {

QWidget *window_ = nullptr;                             // само окно программы
QLabel *status_ = nullptr;                              // строка состояния камеры
QLabel *frame_ = nullptr;                               // строка с данными о кадре

} // namespace

namespace Gui {

// Собираем окно из двух строк и вертикальной раскладки и показываем его.
void create()
{
    window_ = new QWidget;                              // создаём окно (живёт до конца программы)
    status_ = new QLabel(QStringLiteral("Camera: starting..."), window_); // начальное состояние
    frame_ = new QLabel(QStringLiteral("No frame received"), window_);    // пока кадров нет

    auto *layout = new QVBoxLayout(window_);            // вертикальная раскладка виджетов
    layout->addWidget(status_);                         // сверху строка состояния камеры
    layout->addWidget(frame_);                          // снизу данные о кадре

    window_->setWindowTitle(QStringLiteral("RAW camera")); // заголовок окна
    window_->resize(420, 120);                          // начальный размер окна
    window_->show();                                    // показываем окно пользователю
}

// Обновляем верхнюю строку — состояние камеры (запущена или ошибка).
void setCameraStatus(const QString &text)
{
    status_->setText(text);
}

// Показываем номер кадра и его размер в байтах в нижней строке.
void showFrame(std::size_t bytes, std::size_t number)
{
    frame_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                        .arg(number)
                        .arg(bytes));
}

} // namespace Gui
