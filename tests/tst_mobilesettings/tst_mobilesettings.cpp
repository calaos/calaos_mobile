#include <QtQuickTest>
#include <QFile>
#include <QFontDatabase>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include "Common.h"

namespace {

//Offscreen and software rendering, as CI runs, unless the caller chose.
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
    using QObject::QObject;

    Q_INVOKABLE QString env(const QString &name) const
    {
        return qEnvironmentVariable(name.toUtf8().constData());
    }

    //A page missing from the .qrc builds and only fails at run time on a phone
    Q_INVOKABLE bool isResource(const QString &path) const
    {
        return QFile::exists(QStringLiteral(":/") + path);
    }
};

//The pages read calaosFont from the root context; everything else they use
//is stood in for by each test file.
class Setup: public QObject
{
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QStringLiteral(CALAOS_QML_DIR));
        engine->addImportPath(QStringLiteral(CALAOS_STUB_QML_DIR));
        Common::registerQml();

        //The real Roboto files, so that the captures look like the app
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

QUICK_TEST_MAIN_WITH_SETUP(tst_mobilesettings, Setup)

#include "tst_mobilesettings.moc"
