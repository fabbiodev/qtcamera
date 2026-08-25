#include "data_processing.h"
#include "gui.h"
#include "irsensor.h"
#include "video_stream.h"

#include <cstddef>

#include <QApplication>
#include <QTimer>

// main связывает все компоненты: камеру (IRSensor), окно (Gui),
// видеопоток (VideoStream) и обработку данных (DataProcessing).
// Всё построено на свободных функциях в пространствах имён, без классов.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);                       // создаём Qt-приложение
    Gui::create();                                      // создаём и показываем окно

    if (IRSensor::start()) {                            // пробуем открыть и запустить камеру
        Gui::setCameraStatus(QStringLiteral("Camera: RAW mode %1 x %2")
                                 .arg(IRSensor::width())
                                 .arg(IRSensor::height()));
    } else {                                            // при ошибке показываем её причину
        Gui::setCameraStatus(QStringLiteral("Camera error: %1")
                                 .arg(QString::fromStdString(IRSensor::error())));
    }

    QTimer timer;                                       // таймер регулярного опроса камеры
    std::size_t frameNumber = 0;                        // счётчик полученных кадров
    QObject::connect(&timer, &QTimer::timeout, [&] {    // на каждый тик пробуем прочитать кадр
        IRSensor::RawFrame frame;                       // сюда попадёт сырой кадр
        if (IRSensor::readFrame(frame)) {               // если камера отдала готовый кадр
            DataProcessing::submitRawFrame(frame);      // отдаём RAW-массив на обработку
            VideoStream::submitRawFrame(frame);         // и в видеопоток
            Gui::showFrame(frame.size(), ++frameNumber);// обновляем данные о кадре в окне
        }
    });
    timer.start(10);                                    // опрашиваем камеру каждые 10 мс

    const int result = app.exec();                      // запускаем цикл обработки событий Qt
    IRSensor::stop();                                   // после выхода останавливаем камеру
    return result;                                      // возвращаем код завершения приложения
}
