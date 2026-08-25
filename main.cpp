#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <cstddef>

#include <QApplication>
#include <QTimer>

// main связывает все компоненты: камеру (IRSensor), окно (Gui),
// видеопоток (VideoStream) и обработку данных (DataProcessing).
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                       // создаём Qt-приложение
    Gui gui;                                            // окно программы
    IRSensor sensor;                                    // камера с захватом RAW-кадров
    gui.show();                                         // показываем окно пользователю

    if (sensor.start()) {                               // пробуем открыть и запустить камеру
        gui.setCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                                .arg(sensor.width())
                                .arg(sensor.height()));
    } else {                                            // при ошибке показываем её причину
        gui.setCameraStatus(QStringLiteral("Camera error: %1")
                                .arg(QString::fromStdString(sensor.error())));
    }

    QTimer timer;                                       // таймер регулярного опроса камеры
    std::size_t frameNumber = 0;                        // счётчик полученных кадров
    QObject::connect(&timer, &QTimer::timeout, [&] {    // на каждый тик пробуем прочитать кадр
        IRSensor::RawFrame frame;                       // сюда попадёт сырой кадр
        if (sensor.readFrame(frame)) {                  // если камера отдала готовый кадр
            DataProcessing::submitRawFrame(frame);      // отдаём RAW-массив на обработку
            VideoStream::submitRawFrame(frame);         // и в видеопоток
            gui.showFrame(frame.size(), ++frameNumber); // обновляем данные о кадре в окне
        }
    });
    timer.start(10);                                    // опрашиваем камеру каждые 10 мс

    const int result = app.exec();                      // запускаем цикл обработки событий Qt
    sensor.stop();                                      // после выхода останавливаем камеру
    return result;                                      // возвращаем код завершения приложения
}
