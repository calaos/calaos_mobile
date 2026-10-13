#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QStringList>
#include <QVector>

#include "Common.h"
#include "IOConnection.h"
#include "RoomModel.h"

namespace {

//The offscreen platform and the software scene graph are what CI has; set
//before the application object exists, unless the caller chose otherwise.
const bool envReady = []
{
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    return true;
}();

} //namespace

class RecordingConnection: public QObject, public IOConnection
{
    Q_OBJECT

public:
    QObject *eventSource() override { return this; }
    bool isHttpApiV2() const override { return true; }

    void sendCommand(QString, QString value, QString, QString) override
    {
        sent << value;
    }

    QStringList sent;

signals:
    void eventInputChange(QString id, QString key, QString value);
    void eventOutputChange(QString id, QString key, QString value);
    void eventIoStatusChange(QString id, QVariantMap statusData);
    void eventDeviceChange(QString id, QVariantMap device);
    void eventCommandRefused(QString id);
};

//What the QML tests see of the application: climate items backed by the real
//IOBase, and the commands they sent.
class Harness: public QObject
{
    Q_OBJECT

public:
    ~Harness() override { reset(); }

    Q_INVOKABLE QObject *makeClimate(const QVariantMap &item)
    {
        IOBase *io = new IOBase(nullptr, &con, IOBase::IOOutput);
        io->load(item);
        QQmlEngine::setObjectOwnership(io, QQmlEngine::CppOwnership);
        ios << io;
        return io;
    }

    //A room view model holding one IOBase per item, as RoomModel::load() builds it
    Q_INVOKABLE QObject *makeRoom(const QVariantList &items)
    {
        auto *room = new RoomModel(nullptr, &con);
        for (const QVariant &v: items)
        {
            IOBase *io = new IOBase(nullptr, &con, IOBase::IOOutput);
            io->load(v.toMap());
            room->appendRow(io);
        }
        QQmlEngine::setObjectOwnership(room, QQmlEngine::CppOwnership);
        rooms << room;
        return room;
    }

    //Same order as the decoder: the device first, then the scalar state.
    Q_INVOKABLE void pushFrame(const QString &id, const QString &state, const QVariantMap &device)
    {
        if (!device.isEmpty())
            emit con.eventDeviceChange(id, device);
        if (!state.isNull())
            emit con.eventOutputChange(id, QStringLiteral("state"), state);
    }

    Q_INVOKABLE void pushRefusal(const QString &id) { emit con.eventCommandRefused(id); }

    Q_INVOKABLE QStringList sent() const { return con.sent; }
    Q_INVOKABLE void clearSent() { con.sent.clear(); }

    Q_INVOKABLE void reset()
    {
        qDeleteAll(rooms);
        rooms.clear();
        qDeleteAll(ios);
        ios.clear();
        con.sent.clear();
    }

private:
    RecordingConnection con;
    QVector<IOBase *> ios;
    QVector<RoomModel *> rooms;
};

class AppStub: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QString getPictureSized(const QString &) const { return QString(); }
};

class Setup: public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral(CALAOS_QML_DIR));
        engine->addImportPath(QStringLiteral(CALAOS_STUB_QML_DIR));
        Common::registerQml();

        auto *fonts = new QQmlPropertyMap(engine);
        for (const char *k: { "fontFamily", "fontFamilyThin", "fontFamilyLight" })
            fonts->insert(QLatin1String(k), QString());

        engine->rootContext()->setContextProperty(QStringLiteral("calaosFont"), fonts);
        engine->rootContext()->setContextProperty(QStringLiteral("calaosApp"), new AppStub);
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), new Harness);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_climatewidget, Setup)

#include "tst_climatewidget.moc"
