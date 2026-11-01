#include "CameraFrameDecoder.h"

#include <QBuffer>
#include <QImageReader>

//Enough to keep up with a wall of cameras; more would only take cores away
//from the render thread on a phone or a small kiosk.
static const int DecoderThreads = 2;

CameraFrameDecoder::CameraFrameDecoder(QObject *parent):
    QObject(parent)
{
    pool.setMaxThreadCount(DecoderThreads);
    pool.setThreadPriority(QThread::LowPriority);
}

CameraFrameDecoder::~CameraFrameDecoder()
{
    //The workers emit on this object: it has to outlive them
    pool.waitForDone();
}

quint64 CameraFrameDecoder::submit(const QByteArray &data, const QSize &maxSize)
{
    const quint64 ticket = ++lastTicket;
    pool.start([this, ticket, data, maxSize]()
    {
        emit decoded(ticket, decode(data, maxSize));
    });
    return ticket;
}

QImage CameraFrameDecoder::decode(const QByteArray &data, const QSize &maxSize)
{
    QByteArray bytes = data;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    //Naming the format spares every frame a probe by every installed image
    //plugin, but Qt then reads nothing else: only name it on a JPEG signature.
    if (data.startsWith("\xFF\xD8"))
        reader.setFormat("jpeg");

    //Asking the reader rather than scaling afterwards is the whole gain: the
    //JPEG handler then skips most of the DCT, at a fraction of the time and of
    //the memory of a full decode.
    const QSize full = reader.size();
    if (!maxSize.isEmpty() && full.isValid() &&
        (full.width() > maxSize.width() || full.height() > maxSize.height()))
    {
        reader.setScaledSize(full.scaled(maxSize, Qt::KeepAspectRatio).expandedTo(QSize(1, 1)));
    }

    return reader.read();
}
