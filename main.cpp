#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Gui gui;
    IRSensor sensor;
    gui.show();

    if (sensor.start()) {
        gui.setCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                                .arg(sensor.width()).arg(sensor.height()));
    } else {
        gui.setCameraStatus(QStringLiteral("Camera error: %1")
                                .arg(QString::fromStdString(sensor.error())));
    }

    QTimer timer;
    std::size_t frameNumber = 0;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        IRSensor::RawFrame frame;
        if (sensor.readFrame(frame)) {
            DataProcessing::submitRawFrame(frame);
            gui.showFrame(frame.size(), ++frameNumber);
        }
    });
    timer.start(10);

    const int result = app.exec();
    sensor.stop();
    return result;
}
