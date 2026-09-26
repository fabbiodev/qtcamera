#include "gui.h"

#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QWidget *window_ = nullptr;                             // главное окно программы
QLabel *status_ = nullptr;                              // строка состояния камеры
QLabel *frame_ = nullptr;                               // изображение последнего кадра
QLabel *info_ = nullptr;                                // номер кадра и размер RAW-данных
}

void guiCreate()
{
    window_ = new QWidget;                              // родитель для всех элементов окна
    status_ = new QLabel(QStringLiteral("Camera: starting..."), window_);
    frame_ = new QLabel(QStringLiteral("No frame received"), window_);
    info_ = new QLabel(window_);
    frame_->setMinimumSize(512, 384);                   // не сжимаем область изображения слишком сильно
    frame_->setAlignment(Qt::AlignCenter);              // центрируем кадр в свободном месте

    auto *layout = new QVBoxLayout(window_);            // вертикальное расположение элементов
    layout->addWidget(status_);                          // статус сверху
    layout->addWidget(frame_);                           // изображение по центру
    layout->addWidget(info_);                            // служебная строка снизу

    window_->setWindowTitle(QStringLiteral("RAW camera")); // заголовок окна
    window_->resize(560, 480);                           // начальный размер, дальше окно можно менять
    window_->show();                                     // показываем окно пользователю
}

void guiSetCameraStatus(const QString &text)
{
    status_->setText(text);                              // обновляем текст без пересоздания виджета
}

void guiShowFrame(const QImage &image, std::size_t bytes, std::size_t number)
{
    frame_->setPixmap(QPixmap::fromImage(image).scaled( // масштабируем только для вывода на экран
        frame_->size(), Qt::KeepAspectRatio, Qt::FastTransformation)); // без изменения исходного QImage
    info_->setText(QStringLiteral("Frame %1, %2 RAW bytes")
                      .arg(static_cast<qulonglong>(number))
                      .arg(static_cast<qulonglong>(bytes)));
}
