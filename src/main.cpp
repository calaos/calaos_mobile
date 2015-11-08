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
    return app.exec();
}
