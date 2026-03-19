#include "BookmarkModel.h"
#include "HardwareUtils.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

#ifdef CALAOS_DESKTOP
#include "HardwareUtils_desktop.h"
#endif

BookmarkModel::BookmarkModel(QObject *parent)
    : QObject(parent)
{
    loadBookmarks();
}

QVariantList BookmarkModel::bookmarks() const
{
    return m_bookmarks;
}

QString BookmarkModel::bookmarkFilePath() const
{
#ifdef CALAOS_DESKTOP
    HardwareUtilsDesktop *hw = dynamic_cast<HardwareUtilsDesktop *>(HardwareUtils::Instance());
    if (hw)
        return hw->getConfigFile("bookmarks.json");
#endif

    // Fallback for non-desktop or if cast fails
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir d(path);
    d.mkpath(".");
    return QStringLiteral("%1/bookmarks.json").arg(path);
}

void BookmarkModel::loadBookmarks()
{
    QString filePath = bookmarkFilePath();
    QFile file(filePath);

    if (!file.exists())
        return;

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "BookmarkModel: Failed to open" << filePath << "for reading";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "BookmarkModel: JSON parse error:" << error.errorString();
        return;
    }

    if (!doc.isArray())
    {
        qWarning() << "BookmarkModel: Expected JSON array in" << filePath;
        return;
    }

    m_bookmarks.clear();

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr)
    {
        QJsonObject obj = val.toObject();
        QVariantMap bookmark;
        bookmark["title"] = obj["title"].toString();
        bookmark["url"] = obj["url"].toString();
        m_bookmarks.append(bookmark);
    }

    qInfo() << "BookmarkModel: Loaded" << m_bookmarks.size() << "bookmarks from" << filePath;
}

void BookmarkModel::saveBookmarks()
{
    QString filePath = bookmarkFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "BookmarkModel: Failed to open" << filePath << "for writing";
        return;
    }

    QJsonArray arr;
    for (const QVariant &v : m_bookmarks)
    {
        QVariantMap bookmark = v.toMap();
        QJsonObject obj;
        obj["title"] = bookmark["title"].toString();
        obj["url"] = bookmark["url"].toString();
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qInfo() << "BookmarkModel: Saved" << m_bookmarks.size() << "bookmarks to" << filePath;
}

void BookmarkModel::addBookmark(const QString &title, const QString &url)
{
    QVariantMap bookmark;
    bookmark["title"] = title;
    bookmark["url"] = url;
    m_bookmarks.append(bookmark);

    saveBookmarks();
    emit bookmarksChanged();
}

void BookmarkModel::removeBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;

    m_bookmarks.removeAt(index);

    saveBookmarks();
    emit bookmarksChanged();
}

void BookmarkModel::updateBookmark(int index, const QString &title, const QString &url)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;

    QVariantMap bookmark;
    bookmark["title"] = title;
    bookmark["url"] = url;
    m_bookmarks[index] = bookmark;

    saveBookmarks();
    emit bookmarksChanged();
}
