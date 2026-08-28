#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
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
    std::size_t frameNumber = 0;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        RawFrame frame;
        if (!irsensorReadFrame(frame)) {
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
