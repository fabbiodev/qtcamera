#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                       // основной цикл и объекты Qt
    guiCreate();                                         // создаём окно до запуска камеры

    if (irsensorStart()) {                              // пробуем открыть /dev/video0
        guiSetCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                               .arg(irsensorWidth()).arg(irsensorHeight()));
    } else {
        guiSetCameraStatus(QStringLiteral("Camera error: %1")
                               .arg(QString::fromStdString(irsensorError())));
    }

    QTimer timer;                                       // регулярно проверяем наличие нового кадра
    QElapsedTimer reconnectTimer;                       // ограничивает частоту повторных подключений
    reconnectTimer.start();                             // начинаем отсчёт для первой попытки
    std::size_t frameNumber = 0;                        // порядковый номер показанного кадра
    QObject::connect(&timer, &QTimer::timeout, [&] {
        if (!irsensorIsRunning()) {                     // камера отключена или поток остановлен
            if (reconnectTimer.elapsed() < 1000) {      // пробуем восстановить не чаще раза в секунду
                return;
            }
            reconnectTimer.restart();
            if (irsensorStart()) {                      // повторно открываем устройство камеры
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

        const QImage image = dataProcessingConvertRawFrame(frame); // RAW -> градации серого
        videoStreamSubmitRawFrame(frame);               // передаём исходные данные в модуль потока
        guiShowFrame(image, frame.size(), ++frameNumber); // выводим кадр в окно
    });
    timer.start(10);                                    // проверка очереди V4L2 каждые 10 мс

    const int result = app.exec();                       // работаем до закрытия окна
    irsensorStop();                                      // освобождаем V4L2-буферы перед выходом
    return result;
}
