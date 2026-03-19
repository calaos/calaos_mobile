#ifndef BOOKMARKMODEL_H
#define BOOKMARKMODEL_H

#include <QObject>
#include <QVariantList>

class BookmarkModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)

public:
    explicit BookmarkModel(QObject *parent = nullptr);

    QVariantList bookmarks() const;

    Q_INVOKABLE void addBookmark(const QString &title, const QString &url);
    Q_INVOKABLE void removeBookmark(int index);
    Q_INVOKABLE void updateBookmark(int index, const QString &title, const QString &url);

signals:
    void bookmarksChanged();

private:
    void loadBookmarks();
    void saveBookmarks();
    QString bookmarkFilePath() const;

    QVariantList m_bookmarks;
};

#endif // BOOKMARKMODEL_H
