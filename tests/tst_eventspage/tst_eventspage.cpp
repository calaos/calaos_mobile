#include <QtQuickTest>
#include <QFile>
#include <QFontDatabase>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QTranslator>
#include "Common.h"
#include "ControlPanelModel.h"

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

} //namespace

class Harness: public QObject
{
    Q_OBJECT

public:
    explicit Harness(QQmlEngine *e): QObject(e), engine(e), panel(new ControlPanelModel(this)) {}

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

    //The panel entry that opens a page: its icon, or an empty string without it
    Q_INVOKABLE QString panelIcon(const QString &clickId) const
    {
        for (int i = 0; i < panel->rowCount(); i++)
            if (panel->data(panel->index(i, 0), ControlPanelModel::RoleClickId).toString() == clickId)
                return panel->data(panel->index(i, 0), ControlPanelModel::RoleIcon).toString();
        return QString();
    }

    Q_INVOKABLE bool setLanguage(const QString &lang)
    {
        delete translator;
        translator = nullptr;
        if (!lang.isEmpty())
        {
            translator = new QTranslator;
            if (!translator->load(QStringLiteral(CALAOS_LANG_DIR "/calaos_") + lang) ||
                !QCoreApplication::installTranslator(translator))
                return false;
        }
        engine->retranslate();
        return true;
    }

private:
    QQmlEngine *engine;
    ControlPanelModel *panel;
    QTranslator *translator = nullptr;
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
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), new Harness(engine));
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_eventspage, Setup)

#include "tst_eventspage.moc"
