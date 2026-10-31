#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include "RoomFilterModel.h"
#include "ControlPanelModel.h"
#include "LaunchLogin.h"
#include "TlsPinning.h"
#include "AdminCode.h"
#include <QQmlNetworkAccessManagerFactory>

#include "version.h"

#include <QProcess>
#include <qfappdispatcher.h>
#include <QScreen>

#ifdef CALAOS_DESKTOP
#include "CalaosWidgetModel.h"
#include "WeatherInfo.h"
#include "ScreenManager.h"
#include "UserInfoModel.h"
#include "CalaosOsAPI.h"
#ifdef HAVE_WEBENGINE
#include <QQuickWebEngineProfile>
#endif
#elif defined (Q_OS_ANDROID)
#include <QStyleHints>
#endif

//QML Image elements fetch event pictures from the server with a manager of
//their own: same pins, but no prompt from a loader thread, an unknown
//certificate just leaves the picture empty.
class PinnedNetworkAccessManagerFactory: public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override
    {
        QNetworkAccessManager *nam = new QNetworkAccessManager(parent);
        //Picture urls carry the credentials in their query string.
        nam->setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
        QObject::connect(nam, &QNetworkAccessManager::sslErrors, nam,
                         [](QNetworkReply *reply, const QList<QSslError> &errors)
        {
            TlsPinning::acceptIfPinned(reply, errors);
        });
        QObject::connect(nam, &QNetworkAccessManager::encrypted, nam, [](QNetworkReply *reply)
        {
            TlsPinning::keyMatchesPin(reply);
        });
        //A kept-alive channel was validated against the old pins. Queued to
        //the loader thread that owns nam when it is not the main one.
        QObject::connect(TlsPinStore::instance(), &TlsPinStore::pinsChanged,
                         nam, &QNetworkAccessManager::clearConnectionCache);
        return nam;
    }
};

Application::Application(int & argc, char ** argv) :
    QAPP(argc, argv)
{
    QCoreApplication::setOrganizationName("Calaos");
    QCoreApplication::setOrganizationDomain("calaos.fr");
    QCoreApplication::setApplicationName("CalaosHome");

    HardwareUtils::Instance()->setParent(this);
}

Application::~Application()
{
#ifdef CALAOS_DESKTOP
    CalaosWidgetModel::Instance()->saveToDisk();
#endif
}

void Application::createQmlApp()
{
#ifdef CALAOS_DESKTOP
    WeatherModel::registerQmlClasses();
    UserInfoModel::Instance()->load();
#endif

    loadSettings();

    setupLanguage();

    engine.addImportPath("qrc:/qml/");
    static PinnedNetworkAccessManagerFactory namFactory;
    engine.setNetworkAccessManagerFactory(&namFactory);

    connect(HardwareUtils::Instance(), &HardwareUtils::networkStatusChanged,
            this, &Application::networkStatusChanged);
    connect(HardwareUtils::Instance(), &HardwareUtils::calaosServerDetected,
            this, &Application::calaosServerDetected);

    //Only iOS reports these two. Going to the background drops the session
    //but is not a logout (FIX-7): a deliberate logout must survive the app
    //going to the background and back, and a network change seen while in
    //the background must not reconnect.
    connect(HardwareUtils::Instance(), &HardwareUtils::applicationWillResignActive, this, [=]()
    {
        //The password manager filling the native entry resigns the app too:
        //the page waiting for that password must still be there afterwards.
        const QString entry = HardwareUtils::Instance()->pendingNativePasswordPurpose();
        const bool updatesShown = boxUpdates && boxUpdates->isActive();
        resumePolicy.resigned(m_applicationStatus == Common::LoggedIn &&
                                  (entry == QLatin1String("admin_unlock") || updatesShown),
                              entry == QLatin1String("login"));
        syncKeepPage();

        qDebug() << "Application is in background, drop the session"
                 << (resumePolicy.keepPage() ? "(page kept)" : "");
        startedWithOptHandled = false;
        stopPushNotifRouting();
        calaosConnect->pause();
        update_applicationStatus(Common::NotConnected);
    });

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationBecomeActive, this, [=]()
    {
        calaosConnect->unpause();
        QTimer::singleShot(0, this, [=]()
        {
            //Logged out on purpose: stay on the login screen until the user
            //logs in again.
            if (calaosConnect->sessionClosedByUser())
            {
                resumePolicy.reset();
                syncKeepPage();
                qDebug() << "Application is in foreground, logged out by the user: no login";
                return;
            }

            //The native login entry is still open: the user logs in himself.
            const bool autoLogin = resumePolicy.becameActive();
            syncKeepPage();
            if (!autoLogin)
            {
                qDebug() << "Application is in foreground, login entry open: no automatic login";
                return;
            }

            qDebug() << "Application is in foreground, login again";
            startLogin(get_username(), get_password(), get_hostname(), false);
        });
    });

    Common::registerQml();

#ifdef Q_OS_ANDROID
    //The density stays forced to 1: Qt 6 already scales by the device pixel
    //ratio (the Android DisplayMetrics density), applying it again here would
    //double the scale.
    update_density(1);

    qDebug() << "Using forced density: " << get_density() << " Value from QScreen: " << primaryScreen()->devicePixelRatio();

    update_needBackButton(false);
    // Need to fix a bug on Android where text is scratched at runtime on some devices
    qputenv("QML_USE_GLYPHCACHE_WORKAROUND", QByteArray("1"));

    update_isAndroid(true);
    update_isIOS(false);
    update_isDesktop(false);
#else
    if (arguments().contains("--force-hdpi"))
        update_density(2.0);
    else
        update_density(1.0);

    update_needBackButton(true);

#ifdef Q_OS_IOS
    update_isAndroid(false);
    update_isIOS(true);
    update_isDesktop(false);
    update_hasWebEngine(false);
#else
    update_isAndroid(false);
    update_isIOS(false);
    update_isDesktop(true);

#ifdef HAVE_WEBENGINE
    update_hasWebEngine(true);
#else
    update_hasWebEngine(false);
#endif

#endif
#endif

    update_appVersion(PKG_VERSION_STR);
    update_waitingForNetwork(false);
    update_keepPageOnResume(false);

    update_hasInstall(QFile::exists("/.calaos-live") ||
                      QFile::exists("/run/calaos/calaos-live") ||
                      QFile::exists("C:/temp/calaos/calaos-live"));

#ifdef CALAOS_DESKTOP

    update_isSnapshotBoot(false);
    CalaosOsAPI::Instance()->getFsStatus(
        [this](bool success, const QJsonValue &o)
        {
            if (!success)
            {
                qDebug() << "Failed to get fs status for rollback";
                return;
            }

            if (!o["filesystems"].isArray())
            {
                update_isSnapshotBoot(false);
                return;
            }

            auto a = o["filesystems"].toArray();
            if (a.isEmpty())
            {
                update_isSnapshotBoot(false);
                return;
            }

            auto obj = a.at(0).toObject();

            if (obj["target"].toString() == "/" &&
                obj["source"].toString() == "rootfs" &&
                obj["fstype"].toString() == "overlay")
            {
                update_isSnapshotBoot(true);
                return;
            }

            update_isSnapshotBoot(false);
        });

    update_uptime(0);
    update_cpuUsage(0);
    update_memoryUsage(0);
    updateSystemInfo();
#else
    update_isSnapshotBoot(false);
#endif

    update_settingsLocked(false);

    update_applicationStatus(Common::NotConnected);

    calaosConnect = new CalaosConnection(this);
#ifndef CALAOS_DESKTOP
    //Before anything can log in: a logout done before the app was closed
    //still holds (FIX-11). The launch login and iOS become-active check
    //sessionClosedByUser(). Mobile only: the desktop has no login screen, a
    //stray flag in its settings would block server discovery for good.
    LaunchLogin::restoreUserLogout(calaosConnect, HardwareUtils::Instance());
#endif
    connect(calaosConnect, &CalaosConnection::homeLoaded,
            this, &Application::homeLoaded);
    connect(calaosConnect, &CalaosConnection::loginFailed,
            this, &Application::loginFailed);
    connect(calaosConnect, &CalaosConnection::connectionStateChanged,
            this, &Application::connectionStateChanged);
    connect(calaosConnect, &CalaosConnection::disconnected, this, [=]()
    {
        //Emitted before connectionStateChanged(): a failed reconnection in
        //place must give the page up before the status changes.
        resumePolicy.sessionFailed();
        syncKeepPage();
        update_applicationStatus(Common::NotConnected);
        update_settingsLocked(false);

#ifdef CALAOS_DESKTOP
        //T14: no QTimer::singleShot(1000, login) here any more. CalaosConnection
        //owns the one and only reconnection timer and applies the exponential
        //backoff. The alert is only shown on the first failure of a run,
        //otherwise every retry would pop a dialog.
        //Not on a credentials rejection: the connection was not lost and it
        //will not come back on its own, so this text would be wrong twice.
        //loginFailed() shows the right message once the policy gives up.
        if (calaosConnect->reconnectFailureCount() == 1 &&
            !calaosConnect->failingOnCredentials())
        {
            HardwareUtils::Instance()->showAlertMessage(tr("Network error"),
                                                        tr("The connection to calaos_server was lost. "
                                                           "It will reconnect automatically when calaos_server "
                                                           "is available again."),
                                                        tr("Close"));
        }
#endif
    });
    connect(calaosConnect, &CalaosConnection::tlsTrustRequested,
            this, &Application::tlsTrustRequested);
    connect(calaosConnect, &CalaosConnection::tlsTrustCanceled,
            this, &Application::tlsTrustCanceled);
    connect(calaosConnect, &CalaosConnection::tlsTrustRejected, this, [=](const QString &host)
    {
        HardwareUtils::Instance()->showAlertMessage(tr("Not connected"),
#ifdef CALAOS_DESKTOP
                                                    tr("The certificate of %1 was not trusted, "
                                                       "the connection was cancelled.").arg(host),
#else
                                                    tr("The certificate of %1 was not trusted, "
                                                       "the connection was cancelled. Tap Login "
                                                       "to check it again.").arg(host),
#endif
                                                    tr("Close"));
    });
    connect(calaosConnect, &CalaosConnection::tlsCertificateRefused, this,
            [=](const QString &host, const QString &reason, bool weakKey)
    {
        //Old boxes made RSA-1024 certificates, which recent TLS libraries
        //refuse outright: the user needs to know the fix is on the box.
        const QString text = weakKey ?
                    tr("The certificate of %1 uses a key that is too weak (%2). "
                       "The connection was cancelled. Regenerate the certificate of the "
                       "Calaos box (/mnt/calaos/haproxy/server.pem).").arg(host, reason) :
                    tr("The certificate of %1 is not valid (%2). "
                       "The connection was cancelled.").arg(host, reason);
        HardwareUtils::Instance()->showAlertMessage(tr("Certificate refused"), text, tr("Close"));
    });
    connect(TlsPinStore::instance(), &TlsPinStore::pinsChanged,
            this, &Application::updateTlsPins);
    //Also hands the pins to the platform at every start, so an extension or
    //worker never keeps a copy older than the app's.
    updateTlsPins();

    connect(calaosConnect, &CalaosConnection::changeCredsFailed, this, [=]()
    {
        HardwareUtils::Instance()->showAlertMessage(tr("Credentials change failed"),
                                                    tr("Credentials were not changed. Please try again."),
                                                    tr("Close"));
    });
    connect(calaosConnect, &CalaosConnection::changeCredsSuccess, this, [=](QString username, QString password)
    {
        update_username(username);
        update_password(password);
        HardwareUtils::Instance()->saveAuthKeychain(get_username(), get_password());

        QTimer::singleShot(1000, []()
        {
            HardwareUtils::Instance()->showAlertMessage(tr("Credentials changed"),
                                                        tr("Credentials were successfully changed."),
                                                        tr("Close"));
        });
    });

    scenarioModel = new ScenarioModel(&engine, calaosConnect, this);
    scenarioSortModel = new ScenarioSortModel(&engine, this);
    scenarioSortModel->setSourceModel(scenarioModel);
    engine.rootContext()->setContextProperty("scenarioModel", scenarioSortModel);
    lightOnModel = new LightOnModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("lightOnModel", lightOnModel);
    homeModel = new HomeModel(&engine, calaosConnect, scenarioModel, lightOnModel, this);
    engine.rootContext()->setContextProperty("homeModel", homeModel);
    audioModel = new AudioModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("audioModel", audioModel);
    favModel = new FavoritesModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("favoritesModel", favModel);
    favHomeModel = new HomeFavModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("favoritesHomeModel", favHomeModel);
    cameraModel = new CameraModel(&engine, calaosConnect);
    engine.rootContext()->setContextProperty("cameraModel", cameraModel);
    langModel = new LangModel(&engine, this);
    engine.rootContext()->setContextProperty("langModel", langModel);
    eventLogModel = new EventLogModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("eventLogModel", eventLogModel);

    bookmarkModel = new BookmarkModel(this);
    engine.rootContext()->setContextProperty("bookmarkModel", bookmarkModel);

#if defined(CALAOS_MOBILE)
    adminCodeModel = new AdminCodeModel(HardwareUtils::Instance(), this);
    engine.rootContext()->setContextProperty("adminCodeModel", adminCodeModel);
#endif

    //T31: asking the user to type a value is a platform concern, not a model
    //one. IOBase no longer opens a dialog itself; qml/mobile/main.qml drives
    //the native input dialog from here.
    engine.rootContext()->setContextProperty("hardwareUtils", HardwareUtils::Instance());

#ifdef HAVE_WEBENGINE
    //setStorageName() instead of the storage-name constructor: the latter
    //only exists in recent Qt and this must build on older Qt too (6.4)
    webProfile = new QQuickWebEngineProfile(this);
    webProfile->setStorageName(QStringLiteral("CalaosWebProfile"));
    webProfile->setOffTheRecord(false);
    webProfile->setPersistentCookiesPolicy(QQuickWebEngineProfile::ForcePersistentCookies);
    webProfile->setHttpCacheType(QQuickWebEngineProfile::DiskHttpCache);
    engine.rootContext()->setContextProperty("webEngineProfile", webProfile);
#endif

#ifdef CALAOS_DESKTOP
    if (get_hasInstall())
    {
        usbDiskModel = new UsbDiskModel(&engine, this);
        engine.rootContext()->setContextProperty("usbDiskModel", usbDiskModel);
        osInstaller = new OSInstaller(&engine, this);
        engine.rootContext()->setContextProperty("osInstaller", osInstaller);
    }
#endif

#ifndef CALAOS_DESKTOP
    //The kiosk talks to the daemon directly (UpdateManager); the phone goes
    //through calaos_server with the admin password.
    adminRelay = new AdminRelay(calaosConnect, this);
    adminRelay->bindConnection(calaosConnect);
    adminBackend = new AdminRelayBackend(adminRelay, this);
    boxUpdates = new AdminUpdates(adminBackend, adminRelay, this);
    engine.rootContext()->setContextProperty("boxUpdates", boxUpdates);
#endif

    m_netAddresses = new QQmlObjectListModel<NetworkInfo>(this);

#ifdef CALAOS_DESKTOP
    CalaosWidgetModel::Instance()->loadFromDisk();
    engine.rootContext()->setContextProperty("widgetsModel", CalaosWidgetModel::Instance());
    engine.rootContext()->setContextProperty("screenManager", &ScreenManager::Instance());
    engine.rootContext()->setContextProperty("userInfoModel", UserInfoModel::Instance());

    controlPanelModel = new ControlPanelModel(this);
    engine.rootContext()->setContextProperty("controlPanelModel", controlPanelModel);

    updateManager = new UpdateManager(&engine, this);
    engine.rootContext()->setContextProperty("updateManager", updateManager);
    engine.rootContext()->setContextProperty("updateCalaosModel", updateManager->calaosModel());
    engine.rootContext()->setContextProperty("updateGroupsModel", updateManager->groupsModel());
    engine.rootContext()->setContextProperty("updateProgressModel", updateManager->progressModel());

    //network info timer
    updateNetworkInfo();
    auto netTimer = new QTimer(this);
    connect(netTimer, &QTimer::timeout, this, &Application::updateNetworkInfo);
    netTimer->start(120000);

    //sys info timer
    sysInfoTimer = new QTimer();
    connect(sysInfoTimer, &QTimer::timeout, this, &Application::sysInfoTimerSlot);
    sysInfoTimer->start(5000);
    sysInfoTimerSlot();

    if (get_isSnapshotBoot())
    {
        QTimer::singleShot(2000, this, [=]()
        {
            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
            appDispatcher->dispatch("showReadOnlyBootDialog");
        });
    }
#endif

    engine.rootContext()->setContextProperty("calaosApp", this);

    //Register Units singleton
    //qmlRegisterSingletonType(QUrl("qrc:/qml/Units.qml"), "Units", 1, 0, "Units");

    qmlRegisterType<RoomFilterModel>("Calaos", 1, 0, "RoomFilterModel");
    qmlRegisterUncreatableType<QQmlObjectListModelBase> ("Calaos", 1, 0, "ObjectListModel",  "QQmlObjectListModel not creatable");
    qmlRegisterUncreatableType<QQmlVariantListModel> ("Calaos", 1, 0, "VariantListModel",  "QQmlVariantListModel not creatable");

#if defined(CALAOS_MOBILE)
#if defined (Q_OS_ANDROID)
    //From Android 15 the app is drawn edge to edge, under transparent system
    //bars whose icons follow the app's colour scheme. The app is dark
    //whatever the system theme: ask for light icons, or a light system theme
    //leaves dark icons on our black header. Android 14 and older keep their
    //opaque system bars, as before.
    if (QNativeInterface::QAndroidApplication::sdkVersion() >= 35)
        styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif

    //Safe areas (notch, Dynamic Island, home indicator, Android system bars)
    //are handled in QML, with the SafeArea attached property.
    engine.load(QUrl(QStringLiteral("qrc:///qml/mobile/main.qml")));
    //Not deferred to the login: the phone reads its codes without the box.
    //Queued so the sheet's connections exist.
    QTimer::singleShot(0, adminCodeModel, &AdminCodeModel::setReady);

#elif defined(CALAOS_DESKTOP)
    engine.load(QUrl(QStringLiteral("qrc:///qml/desktop/main.qml")));
#else
#error "Unknown UI type!"
#endif

#ifndef CALAOS_DESKTOP
    //Start autologin, only on mobile. On desktop we wait for calaos_server detection
    //Not after a logout, even one done before a restart (FIX-11): the login
    //screen shows the stored credentials until the user taps Login.
    QTimer::singleShot(100, this, [=]()
    {
        if (LaunchLogin::shouldAutoLogin(calaosConnect, get_username(), get_password(), get_hostname()))
            startLogin(get_username(), get_password(), get_hostname(), false);
        else
            qDebug() << "Launch: no automatic login (demo account or logged out by the user)";
    });
#endif
}

void Application::login(QString user, QString pass, QString host)
{
    //The login screen: a certificate refused earlier is asked about again.
    calaosConnect->forgetTlsRefusals();
    startLogin(user, pass, host, true);
}

void Application::answerTlsTrust(bool accept)
{
    calaosConnect->answerTlsTrust(accept);
}

void Application::forgetTlsPin(const QString &host)
{
    if (TlsPinStore::instance()->forget(host))
        qInfo() << "TLS: pin forgotten for" << host;
}

void Application::updateTlsPins()
{
    const QVariantMap pins = TlsPinStore::instance()->pins();
    QVariantList list;
    for (auto it = pins.constBegin(); it != pins.constEnd(); ++it)
    {
        QVariantMap m;
        m[QStringLiteral("host")] = it.key();
        m[QStringLiteral("fingerprint")] = TlsPinning::formatFingerprint(it.value().toString());
        list << m;
    }
    update_tlsPinList(list);
    HardwareUtils::Instance()->setTlsPins(pins);
}

void Application::startLogin(QString user, QString pass, QString host, bool userAction)
{
    host = host.trimmed();
    user = user.trimmed();

    //Before the network check: the properties are what the login screen
    //shows and what homeLoaded() saves, so a login recorded below for when
    //the network is back (FIX-9) is saved with its own credentials. They are
    //written even when deferLogin() refuses, which is harmless: an automatic
    //login writes the values it was given from them, and a refused tap on
    //Login keeps what was typed, as online.
    update_username(user);
    update_password(pass);
    update_hostname(host);

    if (HardwareUtils::Instance()->getNetworkStatus() == HardwareUtils::NotConnected)
    {
        //Launched in airplane mode, or Login tapped without network: the
        //login cannot start, but the intent is recorded with these
        //credentials so networkStatusChanged() -> resume() connects when the
        //link is back (FIX-9). An automatic login never reopens a session the
        //user closed: deferLogin() refuses it.
        const bool recorded = calaosConnect->deferLogin(user, pass, host,
                                                        userAction ? ReconnectPolicy::UserRequest
                                                                   : ReconnectPolicy::AutoRetry);
        qDebug() << "No network, login" << (recorded ? "deferred until the network is back" : "not deferred");

        //No modal alert any more (FIX-10): it stayed on screen after the
        //network came back and the app had connected. The banner shows the
        //wait instead, and hides by itself. A tap on Login also gets the
        //banner highlighted, as an answer to the tap.
        updateWaitingForNetwork();
        if (userAction)
            emit loginWaitsForNetwork();
        return;
    }

    qDebug() << "Try to login to host: " << host;

    //T14: no applicationStatus guard here any more. There is exactly one
    //anti double-login guard and it lives in CalaosConnection/ReconnectPolicy;
    //the two flags used to desynchronise and turn a login into a silent no-op.
    //applicationStatus is updated by connectionStateChanged() when (and only
    //when) the attempt actually starts.
    //UserRequest ignores the iOS pause on purpose (FIX-7): the pause only
    //blocks automatic attempts, and a login comes from the login screen, the
    //launch timer or become-active, i.e. with the app on screen (at worst
    //behind a system prompt); the next resign-active pauses it again.
    calaosConnect->login(user, pass, host);
}

void Application::connectionStateChanged(int state)
{
    switch (state)
    {
    case ReconnectPolicy::Connecting:
        update_applicationStatus(Common::Loading);
        break;

    case ReconnectPolicy::Connected:
        //Deliberately not LoggedIn yet: homeLoaded() switches to LoggedIn once
        //the models are filled, otherwise QML would show an empty home.
        break;

    case ReconnectPolicy::Reconnecting:
    case ReconnectPolicy::Disconnected:
    default:
        //The reconnection in place failed: the normal way, to the login screen.
        resumePolicy.sessionFailed();
        syncKeepPage();
        //A connection lost on its own (server down, ping timeout) goes
        //through here, not through logout(): without this a notification
        //tapped while reconnecting was "opened" on a dead session (FIX-3).
        stopPushNotifRouting();
        update_applicationStatus(Common::NotConnected);
        update_settingsLocked(false);
        break;
    }

    //A credentials give-up ends the wait too (nothing would reconnect).
    updateWaitingForNetwork();
}

void Application::stopPushNotifRouting()
{
    disconnect(HardwareUtils::Instance(), &HardwareUtils::pushNotifReceived,
               this, &Application::pushNotificationReceived);

    HardwareUtils::Instance()->updateCalaosConnectState(false);

    //A deferred tap taken by homeLoaded() but not opened yet goes back to
    //HardwareUtils: the next login opens it, the pending delay will not.
    HardwareUtils::Instance()->restorePendingNotif(deferredNotif.loggedOut());
}

void Application::logout()
{
    resumePolicy.reset();
    syncKeepPage();
    stopPushNotifRouting();
    calaosConnect->logout();
    //The history belongs to the session just closed: the next login may be
    //to another home, its rows and paging must not show there (EVLOG-1).
    eventLogModel->reset();
    update_applicationStatus(Common::NotConnected);
    //Nothing is wanted any more: no banner, even offline.
    updateWaitingForNetwork();
}

void Application::updateWaitingForNetwork()
{
    const bool linkDown = HardwareUtils::Instance()->getNetworkStatus() == HardwareUtils::NotConnected;
    update_waitingForNetwork(calaosConnect->waitsForNetwork(linkDown));
}

void Application::userLogout()
{
    //Persisted: the next launches stay logged out until a login succeeds
    //(homeLoaded()), see LaunchLogin (FIX-11).
    HardwareUtils::Instance()->setSessionActive(false);
    //logout() first: it hands a tap taken but not opened yet back to
    //HardwareUtils (DeferredNotifGate::loggedOut() also makes any armed delay
    //stale), so the line below drops it too (FIX-7).
    logout();
    HardwareUtils::Instance()->forgetPendingUserActions();
}

void Application::resetAllData()
{
    //Also forgets a persisted logout (FIX-11): back to the state of a fresh
    //install, where the demo account is not logged in automatically.
    HardwareUtils::Instance()->resetAuthKeychain();
    //A pin is trust in one particular box: it goes with the host.
    TlsPinStore::instance()->clear();
    calaosConnect->forgetTlsRefusals();
    //Also reset hostname
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("calaos/host", Common::getDemoHost());
    settings.sync();
    //The login screen shown by the logout below must show what is stored now,
    //not the credentials and host that were just erased (FIX-8).
    loadSettings();
    logout();
    //Host and credentials are gone: a tap or Quick Action left from the old
    //home must not fire at the next login, possibly on another home (FIX-7).
    HardwareUtils::Instance()->forgetPendingUserActions();
    //After logout(): the old box is not sent the default name on the way out.
    HardwareUtils::Instance()->setDeviceNameOverride(QString());

    //Codes pushed by the old home go too; the push key stays (an approver
    //phone's key is frozen on its box).
    if (adminCodeModel)
        adminCodeModel->close();
    QStringList stashed;
    for (const QJsonObject &o: HardwareUtils::Instance()->adminCodeStash())
        stashed << AdminCode::Entry::fromJson(o).codeId;
    if (!stashed.isEmpty())
        HardwareUtils::Instance()->removeAdminCodes(stashed);
}

void Application::homeLoaded(const QVariantMap &homeData)
{
    update_settingsLocked(true);
    homeModel->load(homeData);
    audioModel->load(homeData);
    favHomeModel->load(homeData);
    cameraModel->load(homeData);
    eventLogModel->refresh();
    update_applicationStatus(Common::LoggedIn);
    //After the status: main.qml read it to leave the page stack alone.
    resumePolicy.loggedIn();
    syncKeepPage();

    favModel->load(favoritesList);

    saveSettings();
    HardwareUtils::Instance()->setSessionActive(true);

    if (!startedWithOptHandled)
    {
        qDebug() << "handling start options if any";
        startedWithOptHandled = true;
        if (HardwareUtils::Instance()->hasStartedWithOption())
        {
            HardwareUtils::Instance()->resetStartedWithOption();

            //If app has been started with option, it should be a scenario (only supported action for now)
            QString io = HardwareUtils::Instance()->getStartOption("scenario");
            qDebug() << "Start option: activate scenario: " << io;
            IOBase *iosc = IOCache::Instance().searchInput(io);
            if (!iosc)
                qDebug() << "Unable to start scenario " << io << " reason: not found";
            else
                iosc->sendTrue(); //Start scenario
        }
    }

    //A notification tapped while not logged in (cold start, app in the
    //background, or during a reconnection) is opened now, exactly once.
    //Deliberately outside the startedWithOptHandled block: that flag is only
    //reset when the app resigns active, which Android never reports, so a
    //tap during a reconnection used to be dropped there (FIX-3).
    //The short delay lets the home page show first. A session that drops
    //meanwhile voids the ticket (stopPushNotifRouting()).
    const quint64 notifTicket = deferredNotif.loggedIn(HardwareUtils::Instance()->takePendingNotif());
    if (notifTicket != 0)
    {
        QTimer::singleShot(400, this, [this, notifTicket]()
        {
            const QString uuid = deferredNotif.fire(notifTicket);
            if (!uuid.isEmpty())
                pushNotificationReceived(uuid);
        });
    }

    //Export 4 first scenarios to hardware (for QuickLinks on iOS)
    //This allows iOS with 3D touch to quick start a scenario on home screen
    QVariantList scenariosLinks;
    for (int i = 0;i < scenarioSortModel->rowCount() && i < 4;i++)
    {
        IOBase *io = dynamic_cast<IOBase *>(scenarioSortModel->getItemModel(i));
        QVariantMap sc = {{ "id", io->get_ioId() },
                          { "name", io->get_ioName() }};
        scenariosLinks.append(sc);
    }
    HardwareUtils::Instance()->setQuickLinks(scenariosLinks);

    //UniqueConnection: homeLoaded() runs again after every reconnection, and a
    //duplicate connection would open each notification twice.
    connect(HardwareUtils::Instance(), &HardwareUtils::pushNotifReceived,
            this, &Application::pushNotificationReceived, Qt::UniqueConnection);

    //We are now connected, tell HardwareUtils
    HardwareUtils::Instance()->updateCalaosConnectState(true);
}

void Application::loginFailed()
{
    //T14: the guard comes FIRST. It used to sit after the eight clear() calls,
    //so a stray loginFailed() received while we were already disconnected
    //emptied every model all over again.
    resumePolicy.reset();
    syncKeepPage();
    if (m_applicationStatus == Common::NotConnected)
        return;

    qDebug() << "loginFailed called";

    update_settingsLocked(false);
    homeModel->clear();
    audioModel->clear();
    scenarioModel->clear();
    scenarioSortModel->invalidate();
    favModel->clear();
    favHomeModel->clear();
    lightOnModel->clear();
    cameraModel->clear();
    eventLogModel->reset();

    update_applicationStatus(Common::NotConnected);

    //Only emitted once the credentials were rejected for good, so this dialog
    //is shown once and the login screen takes over. No automatic retry is
    //scheduled here: retrying a wrong password forever is exactly the loop
    //this ticket removes.
    HardwareUtils::Instance()->showAlertMessage(tr("Login failed"),
                                                tr("Connection failed, please check your credentials."),
                                                tr("Close"));
}

bool Application::needPictureHDPI()
{
#if defined(Q_OS_ANDROID)
    //if screen has higher dpi, bigger pictures need to be used
    if (get_density() > 1.0)
        return true;
#elif defined(Q_OS_IOS)
    //it is done automatically on iOS
    return false;
#else
    if (get_density() > 1.0)
        return true; //force true for testing purpose on desktop

#endif
    return false;
}

QString Application::getPictureSized(QString pic)
{
    return getPictureSizedPrefix(pic, "img");
}

QString Application::getPictureSizedPrefix(QString pic, QString prefix)
{
    QString ret;

    if (pic.isEmpty())
        return ret;

    //if file exists in svg, use it first
    ret = QStringLiteral(":/%1/%2.svg").arg(prefix, pic);
    if (QFile::exists(ret))
        return QStringLiteral("qrc%1").arg(ret);

    //force @2x images for specific platform (android) as it's not done automatically by Qt
    if (needPictureHDPI())
        ret = QString("qrc:/%1/%2@2x.png").arg(prefix, pic);
    else
        ret = QString("qrc:/%1/%2.png").arg(prefix, pic);

    //qDebug() << "PIC: " << ret;

    return ret;
}

void Application::saveSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    HardwareUtils::Instance()->saveAuthKeychain(get_username(), get_password());
    settings.setValue("calaos/host", get_hostname());
    HardwareUtils::Instance()->setConfigOption("calaos/host", get_hostname());

    //Do not save a wrong or empty model
    if (favModel->isLoaded())
    {
        favoritesList = favModel->save();
        settings.setValue("app/favorites", favoritesList);
    }

    settings.sync();
}

void Application::loadSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    QString cnuser, cnpass;
    HardwareUtils::Instance()->loadAuthKeychain(cnuser, cnpass);
    if (cnuser.isEmpty()) cnuser = Common::getDemoUser();
    if (cnpass.isEmpty()) cnpass = Common::getDemoPass();

    update_username(cnuser);
    update_password(cnpass);
    update_hostname(HardwareUtils::Instance()->getServerHost());

    favoritesList = settings.value("app/favorites").toList();
}

void Application::addItemFavorite(QString ioid, int type)
{
    favModel->addFavorite(ioid, type);

    saveSettings();
}

void Application::delItemFavorite(int idx)
{
    favModel->delItemFavorite(idx);

    saveSettings();
}

void Application::moveFavorite(int idx, int newidx)
{
    favModel->moveFavorite(idx, newidx);

    saveSettings();
}

void Application::networkStatusChanged()
{
    qDebug() << "Network status changed, " << HardwareUtils::Instance()->getNetworkStatus();

    if (HardwareUtils::Instance()->getNetworkStatus() == HardwareUtils::NotConnected)
    {
        //Airplane mode, cable unplugged... Drop the session, but unlike a real
        //logout remember that a session is wanted so the link coming back
        //reconnects on its own (T14). After a logout nothing is wanted, so
        //nothing comes back (FIX-7).
        stopPushNotifRouting();
        calaosConnect->suspend();
        updateWaitingForNetwork();
        return;
    }

    //Hidden as soon as the link is back, before resume() starts connecting.
    updateWaitingForNetwork();

    //Link is back. resume() is a no-op unless a session was wanted and none is
    //in flight, so a status change while connected costs nothing. It is also
    //a no-op after a logout and while the app is in the background (FIX-7).
    //No loadSettings() here (FIX-8): it replaced the credentials of a login
    //that was not accepted (yet) by the stored ones, which overwrote what the
    //user had typed on the login screen and could even log in to another home
    //(the demo one). resume() reconnects with the credentials of the session
    //it resumes, kept by CalaosConnection. That includes a login refused
    //while offline (launch in airplane mode, FIX-9): it was recorded by
    //deferLogin(), with the credentials of the latest such login.
    calaosConnect->resume();
}

void Application::calaosServerDetected()
{
    //T14: no applicationStatus guard and no extra timer. The single guard in
    //CalaosConnection refuses the call while a session is up or an attempt is
    //in flight; when a backoff delay is pending it short-circuits it instead
    //of stacking one more timer.
    //Discovery is how the desktop finds its server, so the stored settings
    //(credentials and the discovered host) are read here, but only when an
    //attempt will actually start: a refused request must not touch the
    //username/password/hostname properties (FIX-8).
    if (!calaosConnect->acceptsImmediateReconnect())
        return;

    loadSettings();
    calaosConnect->requestImmediateReconnect(get_username(), get_password(), get_hostname());
}

void Application::rebootMachine()
{
    qInfo() << "Full reboot requested";
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->rebootMachine([this](const CalaosOsReply &reply)
    {
        //the daemon refuses to cut dpkg while an upgrade runs
        if (reply.ok || !reply.upgradeInProgress)
            return;

        QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
        QVariantMap m = {{ "title", tr("Reboot") },
                         { "message", tr("An update is being installed. Reboot once it has finished.") },
                         { "button", tr("Close") }};
        appDispatcher->dispatch("showNotificationMsg", m);
    });
#endif
}

void Application::restartApp()
{
    qInfo() << "Restart of calaos_home requested";
#ifdef CALAOS_DESKTOP
    updateManager->restartApp();
#endif
}

void Application::rollbackSnapshot()
{
    qInfo() << "Rollback calaos os";
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->rollbackSnapshot([this](bool success)
                                              {
                                                  if (!success)
                                                  {
                                                      QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
                                                      QVariantMap m = {{ "title", "Error" },
                                                                       { "message", "Failed to rollback calaos os to previous version. An error occured. Please check logs" },
                                                                       { "button", "Close" }};
                                                      appDispatcher->dispatch("showNotificationMsg", m);
                                                  }
                                                  else
                                                  {
                                                      CalaosOsAPI::Instance()->rebootMachine({});
                                                  }
                                              });
#endif
}

void Application::sysInfoTimerSlot()
{
    updateSystemInfo();
}

void Application::setupLanguage()
{
    QStringList uiLanguages;
    uiLanguages = QLocale::system().uiLanguages();

    QString lang = HardwareUtils::Instance()->getConfigOption("lang");
    if (lang != "")
    {
        //set language from config
        uiLanguages.prepend(lang);
    }

    foreach (QString locale, uiLanguages)
    {
        //Set language
        QString langfile = QString(":/lang/calaos_%1.qm").arg(locale);
        qInfo() << "Trying to set language: " << langfile;

        if (translator.load(langfile))
        {
            if (!installTranslator(&translator))
                qCritical() << "Failed to install " << langfile;
            else
            {
                qDebug() << "Translator installed";
                break;
            }

            if (!translator.load(QString())) // unload()
                qDebug() << "Failure to unload translation";
        }
        else if (locale == QStringLiteral("C") ||
                 locale == QStringLiteral("en"))
        {
            //use built-in
            break;
        }
    }

    engine.retranslate();
}

void Application::setLanguage(QString code)
{
    HardwareUtils::Instance()->setConfigOption("lang", code);
    setupLanguage();
}

void Application::pushNotificationReceived(const QString &uuid)
{
    Q_UNUSED(uuid)
#ifndef CALAOS_DESKTOP
    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
    QVariantMap m = {{ "notifUuid", uuid }};
    appDispatcher->dispatch("openEventPushViewerUuid", m);
#endif
}

void Application::updateNetworkInfo()
{
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->getNetworkInterfaces(
        [this](bool success, const QJsonValue &data)
        {
            m_netAddresses->clear();

            if (!success) return;

            const QJsonArray &jarr = data.toArray();

            for (int i = 0;i < jarr.size();i++)
            {
                const QJsonObject &o = jarr[i].toObject();

                if (o["is_loopback"].toBool())
                    continue;

                NetworkInfo *net = new NetworkInfo();

                net->update_netinterface(o["name"].toString());
                net->setIPv4CIDR(o["ipv4"].toString());
                net->update_ipv6(o["ipv6"].toString());
                net->update_mac(o["mac"].toString());
                net->update_isLoopback(o["is_loopback"].toBool());
                net->update_gateway(o["gateway"].toString());
                net->update_netstate(o["state"].toString());
                net->update_isDHCP(o["dhcp"].toBool());

                //DNS
                QJsonArray dns = o["dns_servers"].toArray();
                QStringList dnsList;
                for (int j = 0;j < dns.size();j++)
                    dnsList << dns[j].toString();

                net->update_dnsServers(dnsList.join(", "));

                //Search domains
                QJsonArray search = o["search_domains"].toArray();
                QStringList searchList;
                for (int j = 0;j < search.size();j++)
                    searchList << search[j].toString();

                net->update_searchDomains(searchList.join(", "));

                m_netAddresses->append(net);
            }
        }
    );
#endif
}

void Application::updateSystemInfo()
{
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->getSystemInfo(
        [this](bool success, const QJsonValue &data)
        {
            if (!success) return;

            update_cpuUsage(Common::toIntSafe(data["cpu_usage"], 0, "system_info.cpu_usage"));
            update_memoryUsage(Common::toIntSafe(data["mem_usage"], 0, "system_info.mem_usage"));
            update_machineName(data["hostname"].toString());
            update_uptime(Common::toIntSafe(data["uptime"], 0, "system_info.uptime"));
        }
    );
#endif
}

bool Application::unlockSettings(QString pass)
{
    if (pass.isEmpty())
        return false;

    if (pass != get_password())
        return false;

    update_settingsLocked(false);

    //relock after 5 minutes
    if (!timerLockSettings)
    {
        timerLockSettings = new QTimer(this);
        connect(timerLockSettings, &QTimer::timeout, this, &Application::lockSettings);
    }
    timerLockSettings->start(5 * 60 * 1000);

    return true;
}

void Application::lockSettings()
{
    update_settingsLocked(true);
}

bool Application::changeUsername(QString user)
{
    if (get_applicationStatus() != Common::LoggedIn)
    {
        //save user in local config file only
        QString u, p;
        HardwareUtils::Instance()->loadAuthKeychain(u, p);
        HardwareUtils::Instance()->saveAuthKeychain(user, p);
    }
    else if (user != get_username())
    {
        //save user on server
        calaosConnect->changeCredentials(user, get_password());
    }

    return true;
}

bool Application::changePassword(QString pass)
{
    if (get_applicationStatus() != Common::LoggedIn)
    {
        //save pass in local config file only
        QString u, p;
        HardwareUtils::Instance()->loadAuthKeychain(u, p);
        HardwareUtils::Instance()->saveAuthKeychain(u, pass);
    }
    else if (pass != get_password())
    {
        //save user on server
        calaosConnect->changeCredentials(get_username(), pass);
    }

    return true;
}

void Application::updateNetwork(QString intfName, bool dhcp, QString ipv4, QString netmask, QString gateway, QString dns, QString dnsSearch)
{
#ifdef CALAOS_DESKTOP
    //Find the network interface
    NetworkInfo *net = nullptr;

    for (int i = 0;i < m_netAddresses->size();i++)
    {
        if (m_netAddresses->at(i)->get_netinterface() == intfName)
        {
            net = m_netAddresses->at(i);
            break;
        }
    }

    if (!net)
    {
        qWarning() << "Network interface not found: " << intfName;
        return;
    }

    net->update_isDHCP(dhcp);
    net->update_ipv4(ipv4);
    net->update_netmask(netmask);
    net->update_gateway(gateway);
    net->update_dnsServers(dns);
    net->update_searchDomains(dnsSearch);

    //Update the network
    CalaosOsAPI::Instance()->configureNetwork(net->toJson(),
                                              [this](bool success)
                                              {
                                                  if (!success)
                                                  {
                                                      QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
                                                      QVariantMap m = {{ "title", "Error" },
                                                                       { "message", "Failed to configure network. An error occured. Please check logs" },
                                                                       { "button", "Close" }};
                                                      appDispatcher->dispatch("showNotificationMsg", m);
                                                  }
                                                  else
                                                  {
                                                      QTimer::singleShot(1000, this, [this]()
                                                                         {
                                                                             updateNetworkInfo();
                                                                         });
                                                  }
                                              });
#endif
}
