#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
    QApplication app(argc, argv);
    guiCreate();

    if (irsensorStart()) {
        guiSetCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                               .arg(irsensorWidth()).arg(irsensorHeight()));
    } else {
        guiSetCameraStatus(QStringLiteral("Camera error: %1")
                               .arg(QString::fromStdString(irsensorError())));
    }

    QTimer timer;
    QElapsedTimer reconnectTimer;
    reconnectTimer.start();
    std::size_t frameNumber = 0;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (!irsensorIsRunning()) {
            if (reconnectTimer.elapsed() < 1000) {
                return;
            }
            reconnectTimer.restart();
            if (irsensorStart()) {
                guiSetCameraStatus(QStringLiteral("Camera: reconnected, RAW mode %1 x %2")
                                       .arg(irsensorWidth()).arg(irsensorHeight()));
            } else {
                guiSetCameraStatus(QStringLiteral("Camera disconnected: %1")
                                       .arg(QString::fromStdString(irsensorError())));
            }
            return;
        }

        RawFrame frame;
        if (!irsensorReadFrame(frame)) {
            if (!irsensorIsRunning()) {
                reconnectTimer.restart();
                guiSetCameraStatus(QStringLiteral("Camera disconnected; waiting for reconnect..."));
            }
            return;
        }

        const QImage image = dataProcessingConvertRawFrame(frame);
        videoStreamSubmitRawFrame(frame);
        guiShowFrame(image, frame.size(), ++frameNumber);
    });
    timer.start(10);

    const int result = app.exec();
    irsensorStop();
    return result;
}
