#ifndef ROOMMODEL_H
#define ROOMMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "Common.h"
#include "CalaosConnection.h"

class RoomModel: public QStandardItemModel
{
    Q_OBJECT
public:
    explicit RoomModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName,
        RoleId
    };

    void load(QVariantMap &roomData);

    Q_INVOKABLE QObject *getItemModel(int idx);

private:
    QString name, type, hits;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

class IOBase: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(Common::IOType, ioType, RoomModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(int, ioHits, RoomModel::RoleHits)
    QML_READONLY_PROPERTY_MODEL(QString, ioName, RoomModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, ioId, RoomModel::RoleId)

    Q_PROPERTY(bool stateBool READ getStateBool NOTIFY stateChange)
    Q_PROPERTY(double stateInt READ getStateInt NOTIFY stateChange)
    Q_PROPERTY(QString stateString READ getStateString NOTIFY stateChange)

public:
    IOBase(CalaosConnection *con, int t);

    enum {
        IOInput, IOOutput
    };

    void load(QVariantMap &io);

    Q_INVOKABLE void sendTrue();
    Q_INVOKABLE void sendFalse();
    Q_INVOKABLE bool getStateBool();
    Q_INVOKABLE double getStateInt();
    Q_INVOKABLE QString getStateString();

private:
    QVariantMap ioData;
    CalaosConnection *connection;
    int ioType;

signals:
    void stateChange();
};

#endif // ROOMMODEL_H
