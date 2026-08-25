#include "gui.h"
#include <QLabel>
#include <QVBoxLayout>
Gui::Gui(QWidget *parent) : QWidget(parent) { setWindowTitle(QStringLiteral("RAW camera")); resize(420, 120); auto *layout = new QVBoxLayout(this); statusLabel_ = new QLabel(QStringLiteral("Camera: starting..."), this); frameLabel_ = new QLabel(QStringLiteral("No frame received"), this); layout->addWidget(statusLabel_); layout->addWidget(frameLabel_); }
void Gui::setCameraStatus(const QString &status) { statusLabel_->setText(status); }
void Gui::showFrame(std::size_t byteCount, std::size_t frameNumber) { frameLabel_->setText(QStringLiteral("Frame %1, %2 RAW bytes").arg(static_cast<qulonglong>(frameNumber)).arg(static_cast<qulonglong>(byteCount))); }
