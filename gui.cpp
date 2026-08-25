#include "gui.h"

#include <QLabel>
#include <QVBoxLayout>

// В конструкторе собираем окно из двух строк и вертикальной раскладки.
Gui::Gui(QWidget *parent)
    : QWidget(parent)
{
    status_ = new QLabel(QStringLiteral("Camera: starting..."), this); // начальное состояние
    frame_ = new QLabel(QStringLiteral("No frame received"), this);    // пока кадров нет

    auto *layout = new QVBoxLayout(this);               // вертикальная раскладка виджетов
    layout->addWidget(status_);                         // сверху строка состояния камеры
    layout->addWidget(frame_);                          // снизу данные о кадре

    setWindowTitle(QStringLiteral("RAW camera"));       // заголовок окна
    resize(420, 120);                                   // начальный размер окна
}

// Обновляем верхнюю строку — состояние камеры (запущена или ошибка).
void Gui::setCameraStatus(const QString &text)
{
    status_->setText(text);
}

// Показываем номер кадра и его размер в байтах в нижней строке.
void Gui::showFrame(std::size_t bytes, std::size_t number)
{
    frame_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                        .arg(number)
                        .arg(bytes));
}
