#ifndef APPLICATION_H
#define APPLICATION_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "HomeModel.h"
#include "qqmlhelpers.h"
#include "Common.h"

class Application : public QGuiApplication
{
    Q_OBJECT

public:
    QML_WRITABLE_PROPERTY(Common::AppStatus, applicationStatus)

public:
    Application(int &argc, char **argv);

    Q_INVOKABLE void login(QString user, QString pass, QString host);

private slots:
    void homeLoaded(QVariantMap &homeData);
    void loginFailed();

private:
    QQmlApplicationEngine engine;
    CalaosConnection *calaosConnect;
    HomeModel *homeModel;

};

#endif // APPLICATION_H
