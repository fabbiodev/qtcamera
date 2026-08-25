#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"

#include <QApplication>
#include <QTimer>

namespace {

constexpr int kCameraPollIntervalMs = 10;

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Gui gui;
    IRSensor sensor;

    gui.show();

    if (sensor.start()) {
        gui.setCameraStatus(
            QStringLiteral("Camera: RAW mode %1 x %2")
                .arg(sensor.width())
                .arg(sensor.height()));
    } else {
        gui.setCameraStatus(
            QStringLiteral("Camera error: %1")
                .arg(QString::fromStdString(sensor.lastError())));
    }

    QTimer timer;
    std::size_t frameNumber = 0;

    QObject::connect(&timer, &QTimer::timeout, [&]() {
        IRSensor::RawFrame rawFrame;
        if (!sensor.readFrame(rawFrame)) {
            return;
        }

        DataProcessing::submitRawFrame(rawFrame);
        gui.showFrame(rawFrame.size(), ++frameNumber);
    });

    timer.start(kCameraPollIntervalMs);

    const int result = app.exec();
    sensor.stop();
    return result;
}
