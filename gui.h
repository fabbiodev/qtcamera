#pragma once

#include <QWidget>
#include <cstddef>

class QLabel;

class Gui final : public QWidget
{
public:
    explicit Gui(QWidget *parent = nullptr);
    void setCameraStatus(const QString &text);
    void showFrame(std::size_t bytes, std::size_t number);

private:
    QLabel *status_;
    QLabel *frame_;
};
