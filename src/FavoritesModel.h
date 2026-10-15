#ifndef FAVORITESMODEL_H
#define FAVORITESMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"

class HomeModel;
class IOBase;

class HomeFavModel: public QStandardItemModel
{
    Q_OBJECT
public:
    HomeFavModel(QQmlApplicationEngine *engine, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName
    };

    void load(const QVariantMap &homeData);

    Q_INVOKABLE QObject *getRoomModel(int idx) const;

private:

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

class FavoritesModel: public QStandardItemModel
{
    Q_OBJECT
public:
    explicit FavoritesModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleId,
        RoleIOType,
        RoleName
    };

    Q_INVOKABLE QObject *getItemModel(int idx);

    void load(QVariantList favList);
    QVariantList save();

    bool addFavorite(QString ioid, int type);
    void delItemFavorite(int idx);
    void moveFavorite(int idx, int newidx);

    //Hides the (non virtual) QStandardItemModel::clear() on purpose, the same
    //way LightOnModel does for its own cache: the favorites that could not be
    //rebuilt are kept outside of the rows, and dropping the rows without
    //dropping them would leave save() writing entries the model no longer
    //holds. Every caller (Application, load()) uses a FavoritesModel *.
    void clear();

    bool isLoaded() { return loaded; }

private:
    //Rebuilds one favorite from its saved map. position is the index the entry
    //had in the saved list, it is only used to put back the favorites that
    //could not be rebuilt where they were.
    bool addFavorite(const QVariantMap &fav, int position);

    static bool isKnownFavoriteType(int type);

    QString name, type;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    bool loaded = false;

    //Favorites read from the saved list that this build cannot turn into a
    //row (unknown Common::FavoriteType, or a non FavIO favorite whose data
    //does not resolve to a known IO). They are not shown, but save() writes
    //them back untouched instead of deleting them.
    QList<QPair<int, QVariantMap>> unsupportedFavorites;
};

#endif // FAVORITESMODEL_H
