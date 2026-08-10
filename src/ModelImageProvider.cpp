#include "ModelImageProvider.h"

#include <QMutexLocker>
#include <QStringList>

void ImageCache::setImage(const QString &id, const QImage &image)
{
    QMutexLocker locker(&mutex);
    images.insert(id, image);
}

void ImageCache::removeImage(const QString &id)
{
    QMutexLocker locker(&mutex);
    images.remove(id);
}

void ImageCache::clear()
{
    QMutexLocker locker(&mutex);
    images.clear();
}

QImage ImageCache::image(const QString &id) const
{
    QMutexLocker locker(&mutex);
    //QImage is implicitly shared: this is a refcount bump, not a deep copy.
    //The returned copy is detached from the hash entry, so the caller may keep
    //using it (and scale it) after the mutex has been released.
    return images.value(id);
}

ModelImageProvider::ModelImageProvider(const ImageCachePtr &c):
    QQuickImageProvider(QQuickImageProvider::Image),
    cache(c)
{
}

QImage ModelImageProvider::requestImage(const QString &qid, QSize *size, const QSize &requestedSize)
{
    QImage retimg;

    if (!cache)
        return retimg;

    const QStringList sl = qid.split('/');
    if (sl.isEmpty())
        return retimg;

    const QString &id = sl.at(0);
    if (id.isEmpty())
        return retimg;

    //Keep the historical guard rejecting negative numeric ids
    bool numeric = false;
    const int numericId = id.toInt(&numeric);
    if (numeric && numericId < 0)
        return retimg;

    //Only the cache is touched here, never the item model. The lock is released
    //before the (potentially expensive) scaling below.
    retimg = cache->image(id);

    if (size)
        *size = retimg.size();

    if (retimg.isNull())
        return retimg;

    if (requestedSize.isValid())
        return retimg.scaled(requestedSize, Qt::KeepAspectRatio);

    return retimg;
}

QPixmap ModelImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    return QPixmap::fromImage(requestImage(id, size, requestedSize));
}
