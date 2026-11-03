#ifndef NETWORKLIMITS_H
#define NETWORKLIMITS_H

#include <QNetworkReply>
#include <QObject>

//Size bounds for every network response read into memory. A hostile server or
//relay must not make the app buffer an endless body. Header only, like
//LogRedact.h, so small units can use it without linking CalaosConnection.
namespace NetworkLimits {

constexpr qint64 MaxPictureBytes = 16 * 1024 * 1024;
//The whole home as JSON is a few hundred KiB on a large installation.
constexpr qint64 MaxJsonBytes = 16 * 1024 * 1024;
//OpenWeatherMap answers are a few KiB.
constexpr qint64 MaxWeatherBytes = 1024 * 1024;
//Body of an error answer, logged redacted: no need for more.
constexpr qint64 MaxErrorBodyBytes = 64 * 1024;

//The announced length is refused up front, and the bytes actually received
//stay bounded when the server lies or sends none. Aborting ends the reply with
//OperationCanceledError, handled by the usual error path of the caller.
inline void limitDownload(QNetworkReply *reply, qint64 max = MaxPictureBytes)
{
    //Qt reports no progress for a short first chunk: the headers settle it.
    QObject::connect(reply, &QNetworkReply::metaDataChanged, reply, [reply, max]()
    {
        if (reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > max)
            reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [reply, max](qint64 received, qint64 total)
    {
        if (received > max || total > max)
            reply->abort();
    });
}

} // namespace NetworkLimits

#endif
