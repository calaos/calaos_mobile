#ifndef EVENTLOGMODEL_H
#define EVENTLOGMODEL_H

#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"

#define DEFAULT_PER_PAGE    50

class EventLogItem;

class EventLogModel : public QStandardItemModel
{
    Q_OBJECT
public:
    explicit EventLogModel(QQmlApplicationEngine *engine, CalaosConnection *con, QObject *parent = nullptr);
    virtual ~EventLogModel() {}

    enum
    {
        RoleTitle = Qt::UserRole + 1,
        RoleDate,
        RoleTime,
        RoleType,
        RoleIconSource,
        RoleRoomName,
        RoleNotifText,
        RoleNotifHasPic,
        RoleNotifPicUrl,
        RoleActionText,
    };

    Q_INVOKABLE QObject *getEventModel(int idx) const;

    Q_INVOKABLE void load(int page = 0, int per_page = DEFAULT_PER_PAGE);

    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void refresh();

    Q_INVOKABLE QObject *loadEvent(QString uuid);

private slots:
    void logEventLoaded(const QVariantMap &data);

private:
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    bool loading = false;
    bool needClear = false;
};

class EventLogItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, evTitle, EventLogModel::RoleTitle)
    QML_READONLY_PROPERTY_MODEL(QString, evDate, EventLogModel::RoleDate)
    QML_READONLY_PROPERTY_MODEL(QString, evTime, EventLogModel::RoleTime)
    QML_READONLY_PROPERTY_MODEL(Common::EventType, evType, EventLogModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(QString, evIconSource, EventLogModel::RoleIconSource)
    QML_READONLY_PROPERTY_MODEL(QString, evRoomName, EventLogModel::RoleRoomName)
    QML_READONLY_PROPERTY_MODEL(QString, evNotifText, EventLogModel::RoleNotifText)
    QML_READONLY_PROPERTY_MODEL(bool, evHasPicture, EventLogModel::RoleNotifHasPic)
    QML_READONLY_PROPERTY_MODEL(QString, evPictureUrl, EventLogModel::RoleNotifPicUrl)
    QML_READONLY_PROPERTY_MODEL(QString, evActionText, EventLogModel::RoleActionText)

    QML_READONLY_PROPERTY(bool, loading)

public:
    EventLogItem(QQmlApplicationEngine *engine, CalaosConnection *con);
    virtual ~EventLogItem() {}

    void load(const QVariantMap &data);
    void load(const QString &uuid);

private slots:
    void logEventLoaded(const QVariantMap &data);

private:
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

#endif // EVENTLOGMODEL_H
