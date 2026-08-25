#pragma once

#include <cstddef>

#include <QWidget>

class QLabel;

// Gui — простое окно программы. Показывает состояние камеры и данные о кадрах.
class Gui final : public QWidget {
    Q_OBJECT

public:
    explicit Gui(QWidget *parent = nullptr);            // создаём окно и его виджеты

    void setCameraStatus(const QString &text);          // обновляем строку состояния камеры
    void showFrame(std::size_t bytes, std::size_t number); // показываем данные о новом кадре

private:
    QLabel *status_ = nullptr;                          // строка состояния камеры
    QLabel *frame_ = nullptr;                           // строка с данными о последнем кадре
};
