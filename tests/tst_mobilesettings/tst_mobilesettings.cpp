#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>

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

        auto *fonts = new QQmlPropertyMap(engine);
        for (const char *key: { "fontFamily", "fontFamilyThin", "fontFamilyLight" })
            fonts->insert(QLatin1String(key), QString());
        engine->rootContext()->setContextProperty(QStringLiteral("calaosFont"), fonts);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_mobilesettings, Setup)

#include "tst_mobilesettings.moc"
