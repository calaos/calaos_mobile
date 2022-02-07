#include "Application.h"
#include "Common.h"
#include "HardwareUtils.h"
#ifdef HAVE_WEBENGINE
#include <QtWebEngineQuick/QtWebEngineQuick>
#endif

int main(int argc, char *argv[])
{
#if defined(CALAOS_DESKTOP)
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
#endif

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
    Common::installMessageOutputHandler();
#endif

#ifdef Q_OS_ANDROID
    //This disables Handles that are visible somehow in the HomeView
    qputenv( "QT_QPA_NO_TEXT_HANDLES", QByteArray( "1" ) );
#endif

    //Instanciate hardware class early so it can register
    //all observer before Qt starts
    HardwareUtils::Instance();

#if defined(CALAOS_DESKTOP) && defined(HAVE_WEBENGINE)
    QtWebEngineQuick::initialize();
#endif

    //QAPP::setAttribute(Qt::AA_EnableHighDpiScaling);
    Application app(argc, argv);

    //init platform code after QApp is created and initialized
    HardwareUtils::Instance()->platformInit(app.getEngine());

    //QML app creation needs to be created after platform init is done
    app.createQmlApp();

    return app.exec();
}
