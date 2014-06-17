#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#ifdef Q_OS_ANDROID
#include <QtAndroidExtras/QAndroidJniObject>
#endif

Application::Application(int & argc, char ** argv) :
    QGuiApplication(argc, argv)
{
    Common::registerQml();

#ifdef Q_OS_ANDROID
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    QAndroidJniObject resource = activity.callObjectMethod("getResources","()Landroid/content/res/Resources;");
    QAndroidJniObject metrics = resource.callObjectMethod("getDisplayMetrics","()Landroid/util/DisplayMetrics;");
    update_density(metrics.getField<float>("density"));
#else
    update_density(1.0);
#endif

    update_applicationStatus(Common::NotConnected);

    calaosConnect = new CalaosConnection(this);
    connect(calaosConnect, SIGNAL(homeLoaded(QVariantMap&)),
            this, SLOT(homeLoaded(QVariantMap&)));
    connect(calaosConnect, SIGNAL(loginFailed()),
            this, SLOT(loginFailed()));

    homeModel = new HomeModel(&engine, this);
    engine.rootContext()->setContextProperty("homeModel", homeModel);
    engine.rootContext()->setContextProperty("calaosApp", this);
    engine.load(QUrl(QStringLiteral("qrc:///qml/main.qml")));
}

void Application::login(QString user, QString pass, QString host)
{
    update_applicationStatus(Common::Loading);
    calaosConnect->login(user, pass, host);
}

void Application::homeLoaded(QVariantMap &homeData)
{
    homeModel->load(homeData);
    update_applicationStatus(Common::LoggedIn);
}

void Application::loginFailed()
{
    update_applicationStatus(Common::NotConnected);
}
