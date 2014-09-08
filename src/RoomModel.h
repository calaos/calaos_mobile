#ifndef ROOMMODEL_H
#define ROOMMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "Common.h"
#include "CalaosConnection.h"

class IOBase;

class IOCache
{
public:
    static IOCache &Instance()
    {
        static IOCache cache;
        return cache;
    }
    ~IOCache() { clearCache(); }

    IOBase *searchInput(QString id);
    IOBase *searchOutput(QString id);
    void addInput(IOBase *io);
    void addOutput(IOBase *io);
    void delInput(IOBase *io);
    void delOutput(IOBase *io);

    void clearCache();

private:
    IOCache() {}

    QHash<QString, IOBase *> inputCache;
    QHash<QString, IOBase *> outputCache;
};

class ScenarioModel: public QStandardItemModel
{
    Q_OBJECT
public:
    explicit ScenarioModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleName,
        RoleId,
    };

    Q_INVOKABLE QObject *getItemModel(int idx);

private:
    QString name, type;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

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
        RoleId,
        RoleUnit,
        RoleRW,
        RoleRoomName
    };

    enum {
        LoadNormal = 0, //Load only normal IOs
        LoadAll //load everything (even camera, audio, ...)
    };

    void load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag);

    Q_INVOKABLE QObject *getItemModel(int idx);

signals:
    void sig_light_on();
    void sig_light_off();

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

    QML_READONLY_PROPERTY_MODEL(QString, unit, RoomModel::RoleUnit)
    QML_READONLY_PROPERTY_MODEL(bool, rw, RoomModel::RoleRW)

    QML_READONLY_PROPERTY_MODEL(QString, room_name, RoomModel::RoleRoomName)

    Q_PROPERTY(bool stateBool READ getStateBool NOTIFY stateChange)
    Q_PROPERTY(double stateInt READ getStateInt NOTIFY stateChange)
    Q_PROPERTY(QString stateString READ getStateString NOTIFY stateChange)

    //for rgb only
    Q_PROPERTY(int stateRed READ getStateRed NOTIFY stateChange)
    Q_PROPERTY(int stateGreen READ getStateGreen NOTIFY stateChange)
    Q_PROPERTY(int stateBlue READ getStateBlue NOTIFY stateChange)

    //for shutter smart only
    Q_PROPERTY(int stateShutterPos READ getStateShutterPos NOTIFY stateChange)
    QML_READONLY_PROPERTY(bool, stateShutterBool)
    QML_READONLY_PROPERTY(QString, stateShutterTxt)
    QML_READONLY_PROPERTY(QString, stateShutterTxtAction)

public:
    IOBase(CalaosConnection *con, int t);

    enum {
        IOInput, IOOutput
    };

    IOBase *cloneIO() const;

    void load(const QVariantMap &io);

    Q_INVOKABLE void sendTrue();
    Q_INVOKABLE void sendFalse();
    Q_INVOKABLE void sendInc();
    Q_INVOKABLE void sendDec();
    Q_INVOKABLE void sendDown();
    Q_INVOKABLE void sendUp();
    Q_INVOKABLE void sendStop();
    Q_INVOKABLE void sendStringValue(QString value);
    Q_INVOKABLE void sendIntValue(double value);

    Q_INVOKABLE bool getStateBool();
    Q_INVOKABLE double getStateInt();
    Q_INVOKABLE QString getStateString();

    Q_INVOKABLE int getStateRed();
    Q_INVOKABLE int getStateGreen();
    Q_INVOKABLE int getStateBlue();

    Q_INVOKABLE int getStateShutterPos();

    Q_INVOKABLE void sendValueRed(int value);
    Q_INVOKABLE void sendValueGreen(int value);
    Q_INVOKABLE void sendValueBlue(int value);

private:
    QVariantMap ioData;
    CalaosConnection *connection;
    int ioType;

    void sendRGB(int r, int g, int b);

private slots:
    void inputChanged(QString id, QString key, QString value);
    void outputChanged(QString id, QString key, QString value);

signals:
    void stateChange();

    void light_on();
    void light_off();
};

#endif // ROOMMODEL_H
