#include "gui.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class FrameWidget final : public QWidget
{
public:
    explicit FrameWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(512, 384);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAutoFillBackground(false);
    }

    void setFrame(const QImage &image)
    {
        frame_ = image.copy();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        if (frame_.isNull()) {
            return;
        }

        const QSize targetSize = frame_.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target((width() - targetSize.width()) / 2,
                           (height() - targetSize.height()) / 2,
                           targetSize.width(),
                           targetSize.height());
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(target, frame_);
    }

private:
    QImage frame_;
};

QWidget *window_ = nullptr;
QLabel *status_ = nullptr;
FrameWidget *frame_ = nullptr;
QLabel *info_ = nullptr;

} // namespace

void guiCreate()
{
    window_ = new QWidget;
    status_ = new QLabel(QStringLiteral("Camera: starting..."), window_);
    frame_ = new FrameWidget(window_);
    info_ = new QLabel(window_);

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
    frame_->setFrame(image);
    info_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                      .arg(static_cast<qulonglong>(number))
                      .arg(static_cast<qulonglong>(bytes)));
}
