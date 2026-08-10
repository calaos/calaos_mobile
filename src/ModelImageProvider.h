#ifndef MODELIMAGEPROVIDER_H
#define MODELIMAGEPROVIDER_H

#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSharedPointer>
#include <QString>

/*
 * Thread safe image cache shared between a QStandardItemModel that lives on the
 * GUI thread and a QQuickImageProvider that Qt Quick may call from another
 * thread (render thread or a dedicated image loading thread).
 *
 * The GUI thread publishes a copy of every new image through setImage(), the
 * provider thread only reads it back through image(). The provider never walks
 * the item model, so no model access ever happens outside the GUI thread.
 *
 * The cache is refcounted so that the provider (owned by the QML engine) and
 * the model items (owned by the model) may be destroyed in any order.
 */
class ImageCache
{
public:
    //Called from the GUI thread when a new image is available for an item
    void setImage(const QString &id, const QImage &image);
    //Called from the GUI thread when an item goes away
    void removeImage(const QString &id);
    //Called from the GUI thread when the whole model is reloaded
    void clear();

    //Called from the provider thread. Returns a copy of the cached image, or a
    //null QImage when the id is unknown.
    QImage image(const QString &id) const;

private:
    mutable QMutex mutex;
    QHash<QString, QImage> images;
};

typedef QSharedPointer<ImageCache> ImageCachePtr;

/*
 * Base class factoring out the image providers of the camera and audio models.
 * Image urls are built as "image://<scheme>/<item id>/<random cache buster>",
 * only the first path element is used to look the image up.
 */
class ModelImageProvider: public QQuickImageProvider
{
public:
    explicit ModelImageProvider(const ImageCachePtr &cache);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    ImageCachePtr cache;
};

#endif // MODELIMAGEPROVIDER_H
