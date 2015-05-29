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

    void load(QVariantMap &homeData);

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

    bool isLoaded() { return loaded; }

private:
    QString name, type;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    bool loaded = false;
};

#endif // FAVORITESMODEL_H
