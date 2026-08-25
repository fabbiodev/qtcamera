#include "gui.h"

#include <QLabel>
#include <QVBoxLayout>

Gui::Gui(QWidget *parent)
    : QWidget(parent), status_(new QLabel(this)), frame_(new QLabel(this))
{
    setWindowTitle(QStringLiteral("RAW camera"));
    resize(420, 120);
    status_->setText(QStringLiteral("Camera: starting..."));
    frame_->setText(QStringLiteral("No frame received"));
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(status_);
    layout->addWidget(frame_);
}

void Gui::setCameraStatus(const QString &text) { status_->setText(text); }

void Gui::showFrame(std::size_t bytes, std::size_t number)
{
    frame_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                        .arg(static_cast<qulonglong>(number))
                        .arg(static_cast<qulonglong>(bytes)));
}
