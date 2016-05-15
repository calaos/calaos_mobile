#include "Application.h"
#include "Common.h"
#include "HardwareUtils.h"

int main(int argc, char *argv[])
{
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
    Common::installMessageOutputHandler();
#endif

    //Instanciate hardware class early so it can register
    //all observer before Qt starts
    HardwareUtils::Instance();

    Application app(argc, argv);

    //init platform code after QApp is created and initialized
    HardwareUtils::Instance()->platformInit();

    //QML app creation needs to be created after platform init is done
    app.createQmlApp();

    return app.exec();
}
