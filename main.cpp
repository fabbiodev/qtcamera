#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <string>

using namespace std;

int main(int argc, char *argv[])
{
    // Все параметры, которые можно менять без поиска по модулям программы.
    const string device = "/dev/video0";                // устройство камеры V4L2
    const int requestedWidth = 256;                      // ширина RAW-кадра, запрашиваемая у камеры
    const int requestedHeight = 196;                     // высота полного RAW-буфера
    const int captureBufferCount = 4;                    // число V4L2-буферов в очереди
    const int imageHeight = 192;                         // число строк, выводимых в изображение
    const int blackLevel = 4700;                         // RAW-значение чёрного цвета
    const int whiteLevel = 5300;                         // RAW-значение белого цвета
    const int timerIntervalMs = 38;                      // период проверки готового кадра / интервал запуска таймера
    const int reconnectIntervalMs = 1000;                // пауза между попытками переподключения
    const int minFrameWidth = 2 * 512;                   // минимальная область вывода кадра
    const int minFrameHeight = 2 * 384;
    const int windowWidth = 2 * 560;                     // начальная ширина окна
    const int windowHeight = 2 * 480;
    const QString windowTitle = QStringLiteral("RAW camera");

    QApplication app(argc, argv);                       // основной цикл и объекты Qt
    guiCreate(windowTitle, minFrameWidth, minFrameHeight, windowWidth, windowHeight);

    if (irsensorStart(device, requestedWidth, requestedHeight, captureBufferCount)) {
        guiSetCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                               .arg(irsensorWidth()).arg(irsensorHeight()));
    } else {
        guiSetCameraStatus(QStringLiteral("Camera error: %1")
                               .arg(QString::fromStdString(irsensorError())));
    }

    QTimer timer;                                       // регулярно проверяем наличие нового кадра
    QElapsedTimer reconnectTimer;                       // ограничивает частоту повторных подключений
    reconnectTimer.start();                             // начинаем отсчёт для первой попытки
    size_t frameNumber = 0;                             // порядковый номер показанного кадра
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (!irsensorIsRunning()) {                     // камера отключена или поток остановлен
            if (reconnectTimer.elapsed() < reconnectIntervalMs) { // не чаще заданного интервала
                return;
            }
            reconnectTimer.restart();
            if (irsensorStart(device, requestedWidth, requestedHeight, captureBufferCount)) {
                guiSetCameraStatus(QStringLiteral("Camera: reconnected, RAW mode %1 x %2")
                                       .arg(irsensorWidth()).arg(irsensorHeight()));
            } else {
                guiSetCameraStatus(QStringLiteral("Camera disconnected: %1")
                                       .arg(QString::fromStdString(irsensorError())));
            }
            return;
        }

        RawFrame frame;                                 // сюда копируется очередной полный RAW-кадр
        if (!irsensorReadFrame(frame)) {                // кадра может ещё не быть или камера отключилась
            if (!irsensorIsRunning()) {
                reconnectTimer.restart();
                guiSetCameraStatus(QStringLiteral("Camera disconnected; waiting for reconnect..."));
            }
            return;
        }

        const QImage image = dataProcessingConvertRawFrame( // RAW -> градации серого
            frame, irsensorWidth(), irsensorHeight(), imageHeight, blackLevel, whiteLevel);
        videoStreamSubmitRawFrame(frame);               // передаём исходные данные в модуль потока
        guiShowFrame(image, frame.size() * 2, ++frameNumber); // выводим кадр в окно
    });
    timer.start(timerIntervalMs);                        // проверка очереди V4L2 с заданным периодом

    const int result = app.exec();                       // работаем до закрытия окна
    irsensorStop();                                      // освобождаем V4L2-буферы перед выходом
    return result;
}
