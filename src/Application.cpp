#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>

Application::Application(int & argc, char ** argv) :
    QGuiApplication(argc, argv)
{
    Common::registerQml();

    set_applicationStatus(Common::NotConnected);

    calaosConnect = new CalaosConnection(this);
    connect(calaosConnect, SIGNAL(homeLoaded(QVariantMap&)),
            this, SLOT(homeLoaded(QVariantMap&)));
    connect(calaosConnect, SIGNAL(loginFailed()),
            this, SLOT(loginFailed()));

    homeModel = new HomeModel(this);
    engine.rootContext()->setContextProperty("homeModel", homeModel);
    engine.rootContext()->setContextProperty("calaosApp", this);
    engine.load(QUrl(QStringLiteral("qrc:///qml/main.qml")));
}

void Application::login(QString user, QString pass, QString host)
{
    set_applicationStatus(Common::Loading);
    calaosConnect->login(user, pass, host);
}

void Application::homeLoaded(QVariantMap &homeData)
{
    homeModel->load(homeData);
    set_applicationStatus(Common::LoggedIn);
}

void Application::loginFailed()
{
    set_applicationStatus(Common::NotConnected);
}
