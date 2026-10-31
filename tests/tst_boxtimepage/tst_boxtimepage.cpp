#include <QtQuickTest>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QTemporaryDir>
#include "BoxTimeManager.h"
#include "CalaosOsAPI.h"
#include "Common.h"
#include "FakeTimeDaemon.h"

//Before QGuiApplication, and before the engine for the style
static const bool envReady = []() {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    return true;
}();

class AppStub: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList netAddresses READ netAddresses CONSTANT)
    Q_PROPERTY(int applicationStatus READ applicationStatus CONSTANT)
    Q_PROPERTY(QString appVersion READ text CONSTANT)
    Q_PROPERTY(QString machineName READ text CONSTANT)
    Q_PROPERTY(int uptime READ zero CONSTANT)
    Q_PROPERTY(int cpuUsage READ zero CONSTANT)
    Q_PROPERTY(int memoryUsage READ zero CONSTANT)
    Q_PROPERTY(int restartCount READ restarts NOTIFY restartsChanged)

public:
    QVariantList netAddresses() const { return {}; }
    int applicationStatus() const { return Common::LoggedIn; }
    QString text() const { return QStringLiteral("test"); }
    int zero() const { return 0; }
    int restarts() const { return m_restarts; }

    Q_INVOKABLE void updateSystemInfo() {}
    Q_INVOKABLE void updateNetworkInfo() {}

    Q_INVOKABLE void restartApp()
    {
        m_restarts++;
        emit restartsChanged();
    }

    Q_INVOKABLE void resetRestarts()
    {
        m_restarts = 0;
        emit restartsChanged();
    }

    Q_INVOKABLE QString getPictureSized(const QString &pic) const
    {
        const QString res = QStringLiteral(":/img/") + pic + QStringLiteral(".png");
        return QFile::exists(res) ? QStringLiteral("qrc") + res : QString();
    }

signals:
    void restartsChanged();

private:
    int m_restarts = 0;
};

class Harness: public QObject
{
    Q_OBJECT

public:
    void setEngine(QQmlEngine *e) { engine = e; }

    //A new box and a new page state for each case: the manager keeps the zones
    Q_INVOKABLE void reset()
    {
        delete manager;
        delete api;
        delete daemon;

        daemon = new FakeTimeDaemon;
        daemon->token = QString(64, QLatin1Char('k'));
        daemon->start();

        const QString tokenPath = dir.filePath(QStringLiteral("calaos-ct.token"));
        QFile f(tokenPath);
        if (f.open(QIODevice::WriteOnly))
            f.write(daemon->token.toLatin1());
        f.close();

        api = new CalaosOsAPI;
        api->setBaseAddr(daemon->baseAddr());
        api->setTokenFile(tokenPath);
        manager = new BoxTimeManager(api);
        manager->setLanguage(QStringLiteral("en"));
        engine->rootContext()->setContextProperty(QStringLiteral("boxTime"), manager);
    }

    Q_INVOKABLE void setBox(const QString &key, const QVariant &value)
    {
        daemon->box[key] = QJsonValue::fromVariant(value);
    }

    Q_INVOKABLE void setOldDaemon(bool old) { daemon->oldDaemon = old; }

    Q_INVOKABLE void fail(const QString &method, const QString &path, int status,
                          const QString &code, const QString &msg, const QVariantMap &data = {})
    {
        daemon->script(method.toLatin1(), path, status,
                       FakeTimeDaemon::failure(code, msg, QJsonObject::fromVariantMap(data)));
    }

    Q_INVOKABLE int count(const QString &method, const QString &path) const
    {
        return daemon->requestsTo(method.toLatin1(), path).size();
    }

    Q_INVOKABLE QString lastBody(const QString &path) const
    {
        const auto posts = daemon->requestsTo("POST", path);
        return posts.isEmpty() ? QString() : QString::fromUtf8(posts.last().body);
    }

    Q_INVOKABLE QString env(const QString &name) const
    {
        return qEnvironmentVariable(name.toUtf8().constData());
    }

private:
    QQmlEngine *engine = nullptr;
    QTemporaryDir dir;
    FakeTimeDaemon *daemon = nullptr;
    CalaosOsAPI *api = nullptr;
    BoxTimeManager *manager = nullptr;
};

class Setup: public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        Q_UNUSED(envReady)
        engine->addImportPath(QStringLiteral(CALAOS_QML_DIR));
        engine->addImportPath(QStringLiteral(CALAOS_STUB_QML_DIR));
        Common::registerQml();

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

        auto *ctx = engine->rootContext();
        ctx->setContextProperty(QStringLiteral("calaosFont"), fonts);
        ctx->setContextProperty(QStringLiteral("calaosApp"), new AppStub);
        auto *harness = new Harness;
        harness->setEngine(engine);
        ctx->setContextProperty(QStringLiteral("harness"), harness);
        harness->reset();
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_boxtimepage, Setup)

#include "tst_boxtimepage.moc"
