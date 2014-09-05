#ifndef APPLICATION_H
#define APPLICATION_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "HomeModel.h"
#include "qqmlhelpers.h"
#include "Common.h"
#include "AudioModel.h"
#include "RoomModel.h"
#include "FavoritesModel.h"

class Application : public QGuiApplication
{
    Q_OBJECT

public:
    QML_READONLY_PROPERTY(Common::AppStatus, applicationStatus)
    QML_READONLY_PROPERTY(double, density)
    QML_READONLY_PROPERTY(QString, username)
    QML_READONLY_PROPERTY(QString, password)
    QML_READONLY_PROPERTY(QString, hostname)
    QML_READONLY_PROPERTY(bool, needBackButton)

public:
    Application(int &argc, char **argv);

    Q_INVOKABLE void login(QString user, QString pass, QString host);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void resetAllData();

    Q_INVOKABLE void addItemFavorite(QString ioid, int type);
    Q_INVOKABLE void delItemFavorite(int idx);
    Q_INVOKABLE void moveFavorite(int idx, int newidx);

    //Pictures utility functions
    Q_INVOKABLE bool needPictureHDPI();
    Q_INVOKABLE QString getPictureSized(QString pic);
    Q_INVOKABLE QString getPictureSizedPrefix(QString pic, QString prefix);

private slots:
    void homeLoaded(QVariantMap &homeData);
    void loginFailed();

private:
    QQmlApplicationEngine engine;
    CalaosConnection *calaosConnect;
    HomeModel *homeModel;
    AudioModel *audioModel;
    ScenarioModel *scenarioModel;
    FavoritesModel *favModel;
    HomeFavModel *favHomeModel;

    QVariantList favoritesList;

    void loadSettings();
    void saveSettings();

};

#endif // APPLICATION_H
