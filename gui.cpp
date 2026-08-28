#include "gui.h"

#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QWidget *window_ = nullptr;
QLabel *status_ = nullptr;
QLabel *frame_ = nullptr;
QLabel *info_ = nullptr;
}

void guiCreate()
{
    window_ = new QWidget;
    status_ = new QLabel(QStringLiteral("Camera: starting..."), window_);
    frame_ = new QLabel(QStringLiteral("No frame received"), window_);
    info_ = new QLabel(window_);
    frame_->setMinimumSize(512, 384);
    frame_->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(window_);
    layout->addWidget(status_);
    layout->addWidget(frame_);
    layout->addWidget(info_);

    window_->setWindowTitle(QStringLiteral("RAW camera"));
    window_->resize(560, 480);
    window_->show();
}

void guiSetCameraStatus(const QString &text)
{
    status_->setText(text);
}

void guiShowFrame(const QImage &image, std::size_t bytes, std::size_t number)
{
    frame_->setPixmap(QPixmap::fromImage(image).scaled(
        frame_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
    info_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                      .arg(static_cast<qulonglong>(number))
                      .arg(static_cast<qulonglong>(bytes)));
}
