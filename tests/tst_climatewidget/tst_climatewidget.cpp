#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QFontDatabase>
#include <QFile>
#include <QCoreApplication>
#include <QTranslator>
#include <QXmlStreamReader>
#include <QVariantMap>
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

    void setEngine(QQmlEngine *e) { engine = e; }

    //Installs lang/calaos_<lang>.qm as the app does ("" goes back to English) and
    //re-evaluates the texts that are on screen
    Q_INVOKABLE bool setLanguage(const QString &lang)
    {
        if (translator)
        {
            QCoreApplication::removeTranslator(translator);
            delete translator;
            translator = nullptr;
        }
        bool ok = true;
        if (!lang.isEmpty())
        {
            translator = new QTranslator(this);
            ok = translator->load(QStringLiteral(CALAOS_LANG_DIR "/calaos_%1.qm").arg(lang));
            if (ok)
                QCoreApplication::installTranslator(translator);
        }
        if (engine)
            engine->retranslate();
        return ok;
    }

    //Every message of the climate contexts of lang/calaos_<lang>.ts, as read from the file:
    //{ context, source, translation, finished }
    Q_INVOKABLE QVariantList climateStrings(const QString &lang) const
    {
        QVariantList out;
        QFile f(QStringLiteral(CALAOS_LANG_DIR "/calaos_%1.ts").arg(lang));
        if (!f.open(QIODevice::ReadOnly))
            return out;
        QXmlStreamReader xml(&f);
        QString context, source, translation;
        bool finished = true, numerusEmpty = false;
        while (!xml.atEnd())
        {
            xml.readNext();
            if (xml.isStartElement())
            {
                const auto name = xml.name();
                if (name == u"context")
                    context.clear();
                else if (name == u"name" && context.isEmpty())
                    context = xml.readElementText();
                else if (name == u"message")
                {
                    source.clear();
                    translation.clear();
                    finished = true;
                    numerusEmpty = false;
                }
                else if (name == u"source")
                    source = xml.readElementText();
                else if (name == u"translation")
                {
                    const QString type = xml.attributes().value(QLatin1String("type")).toString();
                    finished = type.isEmpty();
                    translation = xml.readElementText(QXmlStreamReader::IncludeChildElements);
                }
                else if (name == u"numerusform")
                {
                    if (xml.readElementText().isEmpty())
                        numerusEmpty = true;
                }
            }
            else if (xml.isEndElement() && xml.name() == u"message")
            {
                if (context.contains(QLatin1String("Climate")))
                    out << QVariantMap{ { "context", context }, { "source", source },
                                        { "translation", translation.trimmed() },
                                        { "finished", finished && !numerusEmpty } };
            }
            else if (xml.isEndElement() && xml.name() == u"context")
                context.clear();
        }
        return out;
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

    //What a reconnect does to the io a sheet was opened for
    Q_INVOKABLE void destroyIo(QObject *io)
    {
        ios.removeAll(static_cast<IOBase *>(io));
        delete io;
    }

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
    QQmlEngine *engine = nullptr;
    QTranslator *translator = nullptr;
    RecordingConnection con;
    QVector<IOBase *> ios;
    QVector<RoomModel *> rooms;
};

//What DialogNetInterface reads of calaosApp.netAddresses
class NetStub: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QVariantMap get(int) const
    {
        return { { "netinterface", "eth0" }, { "isDHCP", false }, { "ipv4", "192.168.0.2" },
                 { "netmask", "255.255.255.0" }, { "gateway", "192.168.0.1" },
                 { "dnsServers", "192.168.0.1" }, { "searchDomains", "" } };
    }
};

class AppStub: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject *netAddresses READ netAddresses CONSTANT)
    Q_PROPERTY(int applicationStatus READ applicationStatus NOTIFY applicationStatusChanged)

public:
    int applicationStatus() const { return status; }
    QObject *netAddresses() { return &nets; }

    Q_INVOKABLE void setStatus(int s)
    {
        status = s;
        emit applicationStatusChanged(status);
    }

    //The images of the app are compiled into the test, so that the faces of the
    //buttons and the backdrop of the dialogs are the real ones
    Q_INVOKABLE QString getPictureSized(const QString &pic) const
    {
        const QString res = QStringLiteral(":/img/") + pic + QStringLiteral(".png");
        return QFile::exists(res) ? QStringLiteral("qrc") + res : QString();
    }

signals:
    void applicationStatusChanged(int status);

private:
    int status = Common::LoggedIn;
    NetStub nets;
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
        auto *app = new AppStub;
        engine->rootContext()->setContextProperty(QStringLiteral("calaosApp"), app);
        auto *harness = new Harness;
        harness->setEngine(engine);
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), harness);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_climatewidget, Setup)

#include "tst_climatewidget.moc"
