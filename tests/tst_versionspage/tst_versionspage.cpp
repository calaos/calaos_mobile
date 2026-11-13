#include <QtQuickTest>
#include <QClipboard>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include "Common.h"
#include "VersionsModel.h"
#include "VersionsReport.h"

using namespace VersionsReport;

namespace {

const bool envReady = []
{
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    if (!qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    return true;
}();

QJsonObject systemData()
{
    return QJsonObject{
        { "calaos_os", "v4.4.0" },
        { "debian", "13.1" },
        { "kernel", "6.12.48-amd64" },
        { "container", "1.9.1" },
        { "packages", QJsonArray{
              QJsonObject{{ "name", "calaos-container" }, { "version", "1.9.1" }},
              QJsonObject{{ "name", "calaos-home" }, { "version", "4.5.1" }},
              QJsonObject{{ "name", "calaos-server" }, { "version", "4.8.0" }},
              QJsonObject{{ "name", "calaos-web-app" }, { "version", "4.2.0" }},
          }},
    };
}

} //namespace

//The page runs on the real VersionsModel; the box never answers by itself, the
//test decides what comes back and when.
class Harness: public QObject
{
    Q_OBJECT

public:
    explicit Harness(QQmlEngine *e): QObject(e), engine(e) {}

    Q_INVOKABLE void reset()
    {
        pending.clear();
        fetches = 0;
        auto *old = model;
        model = new VersionsModel(QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.1"), this);
        model->setFetcher([this](VersionsModel::Done done)
        {
            fetches++;
            pending << done;
        });
        engine->rootContext()->setContextProperty(QStringLiteral("versionsModel"), model);
        delete old;
    }

    Q_INVOKABLE int fetchCount() const { return fetches; }
    Q_INVOKABLE int pendingCount() const { return pending.size(); }

    //What the kiosk makes of the daemon's answer: 200 is the nominal one
    Q_INVOKABLE void answerDaemon(int status, const QString &server)
    {
        const bool ok = status == 200;
        deliver(fromDaemon(ok, status, ok ? QJsonValue(systemData()) : QJsonValue(), server));
    }

    //A 200 answer whose system block has no package list, or no Calaos OS version
    Q_INVOKABLE void answerVariant(const QString &kind, const QString &server)
    {
        QJsonObject sys = systemData();
        if (kind == QLatin1String("noPackages"))
            sys.insert(QStringLiteral("packages"), QJsonArray());
        else if (kind == QLatin1String("noCalaosOs"))
            sys.insert(QStringLiteral("calaos_os"), QString());
        deliver(fromDaemon(true, 200, QJsonValue(sys), server));
    }

    Q_INVOKABLE void answerRelayError(const QString &code, const QString &server)
    {
        deliver(fromRelayError(code, server));
    }

    Q_INVOKABLE QString clipboardText() const { return QGuiApplication::clipboard()->text(); }
    Q_INVOKABLE void setClipboardText(const QString &t) const { QGuiApplication::clipboard()->setText(t); }

    Q_INVOKABLE QString env(const QString &name) const
    {
        return qEnvironmentVariable(name.toUtf8().constData());
    }

    //A page missing from the .qrc builds and only fails at run time on the kiosk
    Q_INVOKABLE bool isResource(const QString &path) const
    {
        return QFile::exists(QStringLiteral(":/") + path);
    }

    Q_INVOKABLE QString resourceText(const QString &path) const
    {
        QFile f(QStringLiteral(":/") + path);
        return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
    }

private:
    void deliver(const Report &report)
    {
        if (pending.isEmpty())
            return;
        pending.takeFirst()(report);
    }

    QQmlEngine *engine;
    VersionsModel *model = nullptr;
    QList<VersionsModel::Done> pending;
    int fetches = 0;
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
        engine->rootContext()->setContextProperty(QStringLiteral("calaosFont"), fonts);
        auto *harness = new Harness(engine);
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), harness);
        harness->reset();
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_versionspage, Setup)

#include "tst_versionspage.moc"
