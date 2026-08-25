#pragma once

#include <QWidget>

#include <cstddef>

class QLabel;

class Gui final : public QWidget
{
public:
    explicit Gui(QWidget *parent = nullptr);

    void setCameraStatus(const QString &status);
    void showFrame(std::size_t byteCount, std::size_t frameNumber);

private:
    QLabel *statusLabel_ = nullptr;
    QLabel *frameLabel_ = nullptr;
};
