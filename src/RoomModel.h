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

class ScenarioSortModel: public QSortFilterProxyModel
{
    Q_OBJECT
public:
    ScenarioSortModel(QQmlApplicationEngine *eng, QObject *parent = 0):
        QSortFilterProxyModel(parent),
        engine(eng)
    {
        setDynamicSortFilter(true);
    }

    Q_INVOKABLE int indexToSource(int idx) { return mapToSource(index(idx, 0)).row(); }
    Q_INVOKABLE int indexFromSource(int idx) { return mapFromSource(index(idx, 0)).row(); }

    Q_INVOKABLE QObject *getItemModel(int idx);

protected:
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

private:
    QQmlApplicationEngine *engine;
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
        RoleRoomName,
        RoleStyle,
        RoleWarning,
    };

    enum {
        LoadNormal = 0, //Load only normal IOs
        LoadAll //load everything (even camera, audio, ...)
    };

    void load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag);

    Q_INVOKABLE QObject *getItemModel(int idx);

signals:
    void sig_light_on(IOBase *io);
    void sig_light_off(IOBase *io);
    void has_temp_sig(bool h);
    void temp_changed_sig(double tmp);

private slots:
    void temperatureIoDestroyed();
    void temperatureIoChanged();

private:
    QString name, type, hits;

    IOBase *temperatureIo = nullptr;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    QString detectOldGuiType(QString type);
};

class IOBase: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(Common::IOType, ioType, RoomModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(int, ioHits, RoomModel::RoleHits)
    QML_READONLY_PROPERTY_MODEL(QString, ioName, RoomModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, ioId, RoomModel::RoleId)
    QML_READONLY_PROPERTY_MODEL(QString, ioStyle, RoomModel::RoleStyle)

    QML_READONLY_PROPERTY_MODEL(QString, unit, RoomModel::RoleUnit)
    QML_READONLY_PROPERTY_MODEL(bool, rw, RoomModel::RoleRW)

    QML_READONLY_PROPERTY_MODEL(QString, room_name, RoomModel::RoleRoomName)

    QML_READONLY_PROPERTY_MODEL(bool, hasWarning, RoomModel::RoleWarning)

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

    QML_READONLY_PROPERTY(QColor, rgbColor)

public:
    IOBase(QQmlApplicationEngine *eng, CalaosConnection *con, int t);

    enum {
        IOInput, IOOutput
    };

    IOBase *cloneIO() const;

    void load(const QVariantMap &io);

    void checkFirstState();

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

    Q_INVOKABLE void sendColor(QColor c);

    Q_INVOKABLE void askStateText();

private:
    QVariantMap ioData;
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
    int ioType;

    void sendRGB(int r, int g, int b);

private slots:
    void inputChanged(QString id, QString key, QString value);
    void outputChanged(QString id, QString key, QString value);

    void textDialogValid(const QString &text);

signals:
    void stateChange();

    void light_on(IOBase *io);
    void light_off(IOBase *io);
};

Q_DECLARE_METATYPE(IOBase *)

#endif // ROOMMODEL_H
