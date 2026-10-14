#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QFontDatabase>
#include <QFile>
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
        seqs << 0;
    }

    void sendNumberedCommand(QString, QString value, QString, QString, int seq) override
    {
        sent << value;
        seqs << seq;
    }

    QStringList sent;
    QVector<int> seqs;

signals:
    void eventInputChange(QString id, QString key, QString value);
    void eventOutputChange(QString id, QString key, QString value);
    void eventIoStatusChange(QString id, QVariantMap statusData);
    void eventDeviceChange(QString id, QVariantMap device);
    void eventCommandRefused(QString id);
    void eventCommandAnswered(QString id, int seq, bool ok);
};

//What the QML tests see of the application: climate items backed by the real
//IOBase, and the commands they sent.
namespace {
qint64 fakeNow = 1000000;
}

class Harness: public QObject
{
    Q_OBJECT

public:
    Harness()
    {
        //What the frames are stamped with, so that a test moves time as it likes
        IOBase::setClock([]() -> qint64 { return fakeNow; });
    }

    ~Harness() override
    {
        reset();
        IOBase::setClock(nullptr);
    }

    Q_INVOKABLE void setNow(double ms) { fakeNow = qint64(ms); }
    Q_INVOKABLE double now() const { return double(fakeNow); }

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

    //The answer of the server to the command `seq` of an io, in the order the test likes
    Q_INVOKABLE void pushAnswer(const QString &id, int seq, bool ok) { emit con.eventCommandAnswered(id, seq, ok); }

    //Numbers the commands carried, in the order they were sent (0 for a plain one)
    Q_INVOKABLE QVariantList seqs() const
    {
        QVariantList l;
        for (int s: con.seqs)
            l << s;
        return l;
    }

    Q_INVOKABLE QString env(const QString &name) const { return qEnvironmentVariable(name.toUtf8().constData()); }

    Q_INVOKABLE QStringList sent() const { return con.sent; }
    Q_INVOKABLE void clearSent() { con.sent.clear(); con.seqs.clear(); }

    Q_INVOKABLE void reset()
    {
        qDeleteAll(rooms);
        rooms.clear();
        qDeleteAll(ios);
        ios.clear();
        con.sent.clear();
        con.seqs.clear();
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
    //The images of the app are compiled into the test, so that the faces of the
    //buttons and the backdrop of the dialogs are the real ones
    Q_INVOKABLE QString getPictureSized(const QString &pic) const
    {
        const QString res = QStringLiteral(":/img/") + pic + QStringLiteral(".png");
        return QFile::exists(res) ? QStringLiteral("qrc") + res : QString();
    }
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

        //The real Roboto files, all weights as Fonts.qml loads them, so that text metrics
        //and screenshots match the app
        for (const char *w: { "Black", "Bold", "Medium", "Italic" })
            QFontDatabase::addApplicationFont(
                QStringLiteral(CALAOS_FONTS_DIR "/Roboto-") + QLatin1String(w) + QStringLiteral(".ttf"));

        auto *fonts = new QQmlPropertyMap(engine);
        const struct { const char *key; const char *file; } faces[] = {
            { "fontFamily", "Roboto-Regular.ttf" },
            { "fontFamilyThin", "Roboto-Thin.ttf" },
            { "fontFamilyLight", "Roboto-Light.ttf" },
        };
        for (const auto &f: faces)
        {
            const int id = QFontDatabase::addApplicationFont(
                QStringLiteral(CALAOS_FONTS_DIR "/") + QLatin1String(f.file));
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            fonts->insert(QLatin1String(f.key), families.isEmpty() ? QString() : families.first());
        }

        engine->rootContext()->setContextProperty(QStringLiteral("calaosFont"), fonts);
        engine->rootContext()->setContextProperty(QStringLiteral("calaosApp"), new AppStub);
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), new Harness);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_climatewidget, Setup)

#include "tst_climatewidget.moc"
