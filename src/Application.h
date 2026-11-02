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

#define EXIT_RESTART_CODE        0x4E5E6E7E

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
#include "Machine.h"
#include "QQmlObjectListModel.h"
#include "QQmlVariantListModel.h"
#include "Lang.h"
#include "EventLogModel.h"
#include "OSInstaller.h"
#include "UsbDisk.h"
#include "ControlPanelModel.h"
#include "UpdateManager.h"
#include "AdminRelay.h"
#include "AdminRelayBackend.h"
#include "AdminUpdates.h"
#include "AdminBiometrics.h"
#include "ResumePolicy.h"
#include "BookmarkModel.h"

#ifdef HAVE_WEBENGINE
class QQuickWebEngineProfile;
#endif
class AdminCodeModel;

#ifdef CALAOS_DESKTOP
class BoxTimeManager;
#endif

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
    QML_READONLY_PROPERTY(QString, machineName)
    QML_OBJMODEL_PROPERTY(NetworkInfo, netAddresses)
    QML_READONLY_PROPERTY(int, cpuUsage)
    QML_READONLY_PROPERTY(int, memoryUsage)
    QML_READONLY_PROPERTY(int, uptime)
    QML_READONLY_PROPERTY(bool, hasWebEngine)
    QML_READONLY_PROPERTY(bool, hasInstall) //If running on live install enable installation mode
    QML_READONLY_PROPERTY(bool, isSnapshotBoot) //if system has been booted read only from a snapshot

    QML_READONLY_PROPERTY(bool, settingsLocked)

    QML_READONLY_PROPERTY(QString, appVersion)

    //No network while a session is wanted (FIX-10): the mobile UI shows a
    //banner instead of a modal alert, until the link is back. Always false
    //on desktop (HardwareUtils reports it always connected).
    QML_READONLY_PROPERTY(bool, waitingForNetwork)

    //Pinned server certificates, for the settings: {host, fingerprint}.
    QML_READONLY_PROPERTY(QVariantList, tlsPinList)
    //iOS: the page stack survives the current background cycle, see
    //ResumePolicy. main.qml neither pops to the login screen nor pushes the
    //home page while it is set.
    QML_READONLY_PROPERTY(bool, keepPageOnResume)
    //Set by LoginView while the user has typed in its fields: the automatic
    //login on return from the background must not replace that text.
    QML_WRITABLE_PROPERTY(bool, loginEdited)

public:
    Application(int &argc, char **argv);

    virtual ~Application();

    void createQmlApp();

    Q_INVOKABLE void login(QString user, QString pass, QString host);
    Q_INVOKABLE void logout();
    //The user asked to log out (settings button): same as logout(), and the
    //platform is told the session is over (see HardwareUtils::setSessionActive).
    Q_INVOKABLE void userLogout();
    Q_INVOKABLE void resetAllData();

    //Answer to tlsTrustRequested() (see CalaosConnection::answerTlsTrust()).
    Q_INVOKABLE void answerTlsTrust(bool accept);
    //host is the host:port of a tlsPinList entry.
    Q_INVOKABLE void forgetTlsPin(const QString &host);

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

    Q_INVOKABLE void rebootMachine();
    Q_INVOKABLE void restartApp();
    Q_INVOKABLE void rollbackSnapshot();

    QQmlApplicationEngine *getEngine() { return &engine; }

    Q_INVOKABLE void setLanguage(QString code);

    Q_INVOKABLE void updateNetworkInfo();
    Q_INVOKABLE void updateSystemInfo();

    Q_INVOKABLE bool unlockSettings(QString pass);
    Q_INVOKABLE void lockSettings();

    Q_INVOKABLE bool changeUsername(QString user);
    Q_INVOKABLE bool changePassword(QString pass);

    Q_INVOKABLE void updateNetwork(QString intfName, bool dhcp, QString ipv4, QString netmask, QString gateway, QString dns, QString dnsSearch);

signals:
    //Login was tapped without network (FIX-10): the login waits for the
    //network, the banner tells it.
    void loginWaitsForNetwork();

    //The certificate of a server needs the user's confirmation, see
    //CalaosConnection::tlsTrustRequested() for info.
    void tlsTrustRequested(const QVariantMap &info);
    void tlsTrustCanceled();

private slots:
    void homeLoaded(const QVariantMap &homeData);
    void loginFailed();
    //Mirrors CalaosConnection's ReconnectPolicy::State onto applicationStatus.
    //applicationStatus is derived from it, never maintained in parallel.
    void connectionStateChanged(int state);
    void networkStatusChanged();
    void calaosServerDetected();
    void sysInfoTimerSlot();
    void pushNotificationReceived(const QString &uuid);

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
    LangModel *langModel;
    EventLogModel *eventLogModel;
    //Mobile only: the admin codes pushed to this phone (AdminCodeSheet.qml).
    AdminCodeModel *adminCodeModel = nullptr;
    UsbDiskModel *usbDiskModel = nullptr;
    OSInstaller *osInstaller = nullptr;
    ControlPanelModel *controlPanelModel = nullptr;
    UpdateManager *updateManager = nullptr;
#ifdef CALAOS_DESKTOP
    BoxTimeManager *boxTime = nullptr;
#endif
    //mobile: box updates through calaos_server's admin relay
    AdminRelay *adminRelay = nullptr;
    AdminRelayBackend *adminBackend = nullptr;
    AdminUpdates *boxUpdates = nullptr;
    AdminBiometrics *adminBiometrics = nullptr;

    ResumePolicy resumePolicy;
    //Mirrors resumePolicy into keepPageOnResume, before any status change.
    void syncKeepPage() { update_keepPageOnResume(resumePolicy.keepPage()); }
    BookmarkModel *bookmarkModel = nullptr;

#ifdef HAVE_WEBENGINE
    QQuickWebEngineProfile *webProfile = nullptr;
#endif

    QVariantList favoritesList;

    QTimer *sysInfoTimer = nullptr;

    QTranslator translator;

    bool startedWithOptHandled = false;

    //Every login goes through here. userAction: typed on the login screen
    //(true) or automatic (launch, iOS become-active). Offline, the login is
    //not started but recorded so the link coming back connects (FIX-9).
    void startLogin(QString user, QString pass, QString host, bool userAction);
    //Recomputes waitingForNetwork from the network status and the session
    //intent kept by CalaosConnection (FIX-10). Called wherever either may
    //change: network status, login refused offline, connection state, logout.
    void updateWaitingForNetwork();

    //The session is down (logout, lost connection, reconnecting): foreground
    //push notifications stop being routed here, and a notification tapped
    //from now on is deferred by HardwareUtils until homeLoaded().
    void stopPushNotifRouting();

    //Opening of the notification tapped while not logged in, see
    //DeferredNotifGate.
    DeferredNotifGate deferredNotif;

    QTimer *timerLockSettings = nullptr;

    void loadSettings();
    void saveSettings();

    void setupLanguage();

    //tlsPinList, and the copy the platform keeps for the push extension or
    //worker, from TlsPinStore.
    void updateTlsPins();
};

#endif // APPLICATION_H
