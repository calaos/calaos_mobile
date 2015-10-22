#include "Application.h"
#include "Common.h"

int main(int argc, char *argv[])
{
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
    Common::installMessageOutputHandler();
#endif

    Application app(argc, argv);
    return app.exec();
}
