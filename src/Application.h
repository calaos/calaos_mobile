#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>
#ifndef Q_MOC_RUN
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
#include <QGuiApplication>
#define QAPP    QGuiApplication
#else
#include <QApplication>
#define QAPP    QApplication
#endif
#endif

#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "HomeModel.h"
#include "qqmlhelpers.h"
#include "Common.h"
#include "AudioModel.h"
#include "RoomModel.h"
#include "FavoritesModel.h"
#include "HardwareUtils.h"
#include "CameraModel.h"

class Application : public QAPP
{
    Q_OBJECT

public:
    QML_READONLY_PROPERTY(Common::AppStatus, applicationStatus)
    QML_READONLY_PROPERTY(double, density)
    QML_READONLY_PROPERTY(QString, username)
    QML_READONLY_PROPERTY(QString, password)
    QML_READONLY_PROPERTY(QString, hostname)
    QML_READONLY_PROPERTY(bool, needBackButton)
    QML_READONLY_PROPERTY(bool, isAndroid)
    QML_READONLY_PROPERTY(bool, isIOS)
    QML_READONLY_PROPERTY(bool, isDesktop)

public:
    Application(int &argc, char **argv);

    void createQmlApp();

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

    //Utility functions
    Q_INVOKABLE int getColorHue(QColor c) { return c.hue(); }
    Q_INVOKABLE int getColorSaturation(QColor c) { return c.saturation(); }
    Q_INVOKABLE int getColorLightness(QColor c) { return c.lightness(); }

private slots:
    void homeLoaded(const QVariantMap &homeData);
    void loginFailed();
    void networkStatusChanged();
    void calaosServerDetected();

private:
    QQmlApplicationEngine engine;
    CalaosConnection *calaosConnect;
    HomeModel *homeModel;
    AudioModel *audioModel;
    ScenarioModel *scenarioModel;
    ScenarioSortModel *scenarioSortModel;
    FavoritesModel *favModel;
    HomeFavModel *favHomeModel;
    LightOnModel *lightOnModel;
    CameraModel *cameraModel;

    QVariantList favoritesList;

    bool startedWithOptHandled = false;

    void loadSettings();
    void saveSettings();

};

#endif // APPLICATION_H
