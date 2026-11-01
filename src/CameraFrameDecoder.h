#ifndef CAMERAFRAMEDECODER_H
#define CAMERAFRAMEDECODER_H

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QThreadPool>

//A frame from a current camera takes tens of milliseconds to decode, which
//stalls the whole UI when done on the GUI thread.
class CameraFrameDecoder: public QObject
{
    Q_OBJECT

public:
    explicit CameraFrameDecoder(QObject *parent = nullptr);
    ~CameraFrameDecoder() override;

    //Returns a ticket, unique and never 0, repeated by decoded() so that the
    //caller can drop a frame it no longer wants.
    quint64 submit(const QByteArray &data, const QSize &maxSize);

    //Fits the picture in maxSize, keeping its aspect ratio and never enlarging
    //it. An empty maxSize keeps the full resolution.
    static QImage decode(const QByteArray &data, const QSize &maxSize);

signals:
    //Emitted from a worker thread. The image is null when decoding failed.
    void decoded(quint64 ticket, const QImage &image);

private:
    QThreadPool pool;
    quint64 lastTicket = 0;
};

#endif // CAMERAFRAMEDECODER_H
