#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include "RoomFilterModel.h"
#include "ControlPanelModel.h"

#include "version.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif
#include <QProcess>
#include <qfappdispatcher.h>
#include <QScreen>

#ifdef CALAOS_DESKTOP
#include "CalaosWidgetModel.h"
#include "WeatherInfo.h"
#include "ScreenManager.h"
#include "UserInfoModel.h"
#include "CalaosOsAPI.h"
#elif defined (Q_OS_ANDROID) || defined (Q_OS_IOS)
#include <QtGui/qpa/qplatformwindow.h>
#endif

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

    connect(HardwareUtils::Instance(), SIGNAL(networkStatusChanged()),
            this, SLOT(networkStatusChanged()));
    connect(HardwareUtils::Instance(), SIGNAL(calaosServerDetected()),
            this, SLOT(calaosServerDetected()));

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationWillResignActive, this, [=]()
    {
        qDebug() << "Application is in background, logout";
        startedWithOptHandled = false;
        logout();
    });

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationBecomeActive, this, [=]()
    {
        qDebug() << "Application is in foreground, login again";
        QTimer::singleShot(0, this, [=]()
        {
            login(get_username(), get_password(), get_hostname());
        });
    });

    Common::registerQml();

#ifdef Q_OS_ANDROID
    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");
    QJniObject resource = activity.callObjectMethod("getResources","()Landroid/content/res/Resources;");
    QJniObject metrics = resource.callObjectMethod("getDisplayMetrics","()Landroid/util/DisplayMetrics;");
    //update_density(metrics.getField<float>("density"));
    update_density(1);

    qDebug() << "Using density from JNI: " << get_density() << " Value from QScreen: " << primaryScreen()->devicePixelRatio();

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
    connect(calaosConnect, SIGNAL(homeLoaded(QVariantMap)),
            this, SLOT(homeLoaded(QVariantMap)));
    connect(calaosConnect, SIGNAL(loginFailed()),
            this, SLOT(loginFailed()));
    connect(calaosConnect, &CalaosConnection::disconnected, this, [=]()
    {
        update_applicationStatus(Common::NotConnected);
        update_settingsLocked(false);

#ifdef CALAOS_DESKTOP
        HardwareUtils::Instance()->showAlertMessage(tr("Network error"),
                                                    tr("The connection to calaos_server was lost."
                                                       "It will reconnect automatically when calaos_server"
                                                       "is available again."),
                                                    tr("Close"));

        //restart autologin, only on desktop to continually try to connect
        QTimer::singleShot(1000, [=]()
        {
            //reload settings in case it was changed
            loadSettings();
            login(get_username(), get_password(), get_hostname());
        });
#endif
    });
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

#ifdef CALAOS_DESKTOP
    if (get_hasInstall())
    {
        usbDiskModel = new UsbDiskModel(&engine, this);
        engine.rootContext()->setContextProperty("usbDiskModel", usbDiskModel);
        osInstaller = new OSInstaller(&engine, this);
        engine.rootContext()->setContextProperty("osInstaller", osInstaller);
    }
#endif

    engine.rootContext()->setContextProperty("platformMarginsLeft", QVariant(0.0));
    engine.rootContext()->setContextProperty("platformMarginsRight", QVariant(0.0));
    engine.rootContext()->setContextProperty("platformMarginsTop", QVariant(0.0));
    engine.rootContext()->setContextProperty("platformMarginsBottom", QVariant(0.0));

    m_netAddresses = new QQmlObjectListModel<NetworkInfo>(this);

#ifdef CALAOS_DESKTOP
    CalaosWidgetModel::Instance()->loadFromDisk();
    engine.rootContext()->setContextProperty("widgetsModel", CalaosWidgetModel::Instance());
    engine.rootContext()->setContextProperty("screenManager", &ScreenManager::Instance());
    engine.rootContext()->setContextProperty("userInfoModel", UserInfoModel::Instance());

    controlPanelModel = new ControlPanelModel(this);
    engine.rootContext()->setContextProperty("controlPanelModel", controlPanelModel);

    //network info timer
    updateNetworkInfo();
    auto netTimer = new QTimer(this);
    connect(netTimer, &QTimer::timeout, this, &Application::updateNetworkInfo);
    netTimer->start(120000);

    //sys info timer
    sysInfoTimer = new QTimer();
    connect(sysInfoTimer, SIGNAL(timeout()), this, SLOT(sysInfoTimerSlot()));
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
    engine.load(QUrl(QStringLiteral("qrc:///qml/mobile/main.qml")));

#if defined (Q_OS_ANDROID) || defined (Q_OS_IOS)
    auto rootObjs = engine.rootObjects();
    QObject *rootObject = rootObjs.first();
    if (rootObject)
    {
        QWindow *w = qobject_cast<QWindow *>(rootObject);
        if (w)
        {
            QPlatformWindow *pw = w->handle();
            QMargins margins = pw->safeAreaMargins();
            engine.rootContext()->setContextProperty("platformMarginsLeft", margins.left());
            engine.rootContext()->setContextProperty("platformMarginsRight", margins.right());
            engine.rootContext()->setContextProperty("platformMarginsTop", margins.top());
            engine.rootContext()->setContextProperty("platformMarginsBottom", margins.bottom());
        }
    }
#endif

#elif defined(CALAOS_DESKTOP)
    engine.load(QUrl(QStringLiteral("qrc:///qml/desktop/main.qml")));
#else
#error "Unknown UI type!"
#endif

#ifndef CALAOS_DESKTOP
    //Start autologin, only on mobile. On desktop we wait for calaos_server detection
    QTimer::singleShot(100, this, [=]()
    {
        bool demoMode = false;
        QString user = get_username();
        QString pass = get_password();
        QString h = get_hostname();

        if ((user == "demo" || user == "demo@calaos.fr") &&
            pass == "demo" &&
            h == "demo.calaos.fr")
        {
            demoMode = true;
        }

        if (!demoMode)
            login(user, pass, h);
    });
#endif
}

void Application::login(QString user, QString pass, QString host)
{
    if (HardwareUtils::Instance()->getNetworkStatus() == HardwareUtils::NotConnected)
    {
        HardwareUtils::Instance()->showAlertMessage(tr("Network error"),
                                                    tr("No network connection found, "
                                                       "this application requires a "
                                                       "network connection to work."),
                                                    tr("Close"));
        return;
    }

    if (get_applicationStatus() != Common::NotConnected)
        return;

    host = host.trimmed();
    user = user.trimmed();
    qDebug() << "Try to login to host: " << host;

    update_username(user);
    update_password(pass);
    update_hostname(host);

    update_applicationStatus(Common::Loading);
    calaosConnect->login(user, pass, host);
}

void Application::logout()
{
    disconnect(HardwareUtils::Instance(), &HardwareUtils::pushNotifReceived,
               this, &Application::pushNotificationReceived);

    HardwareUtils::Instance()->updateCalaosConnectState(false);
    calaosConnect->logout();
    update_applicationStatus(Common::NotConnected);
}

void Application::resetAllData()
{
    HardwareUtils::Instance()->resetAuthKeychain();
    //Also reset hostname
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("calaos/host", Common::getDemoHost());
    settings.sync();
    logout();
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

    favModel->load(favoritesList);

    saveSettings();

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

        if (HardwareUtils::Instance()->hasStartedWithNotif())
        {
            HardwareUtils::Instance()->resetStartedWithNotif();

            //If app has been started with notification option, it should open it
            QTimer::singleShot(400, this, [=]()
            {
                QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(&engine);
                QVariantMap m = {{ "notifUuid", HardwareUtils::Instance()->getNotifUuid() }};
                appDispatcher->dispatch("openEventPushViewerUuid", m);
            });
        }
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

    connect(HardwareUtils::Instance(), &HardwareUtils::pushNotifReceived,
            this, &Application::pushNotificationReceived);

    //We are now connected, tell HardwareUtils
    HardwareUtils::Instance()->updateCalaosConnectState(true);
}

void Application::loginFailed()
{
    update_settingsLocked(false);
    homeModel->clear();
    audioModel->clear();
    scenarioModel->clear();
    scenarioSortModel->invalidate();
    favModel->clear();
    favHomeModel->clear();
    lightOnModel->clear();
    cameraModel->clear();
    eventLogModel->clear();

    if (m_applicationStatus == Common::NotConnected)
        return;

    qDebug() << "loginFailed called";

    update_applicationStatus(Common::NotConnected);

    HardwareUtils::Instance()->showAlertMessage(tr("Login failed"),
                                                tr("Connection failed, please check your credentials."),
                                                tr("Close"));

#ifdef CALAOS_DESKTOP
    //restart autologin, only on desktop to continually try to connect
    QTimer::singleShot(1000, [=]()
    {
        //reload settings in case it was changed
        loadSettings();
        login(get_username(), get_password(), get_hostname());
    });
#endif
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
        logout();
    }
}

void Application::calaosServerDetected()
{
    if (get_applicationStatus() != Common::NotConnected)
        return;

    loadSettings();
    login(get_username(), get_password(), get_hostname());
}

void Application::rebootMachine()
{
    qInfo() << "Full reboot requested";
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->rebootMachine({});
#endif
}

void Application::restartApp()
{
    qInfo() << "Restart of calaos_home requested";
#ifdef CALAOS_DESKTOP
    CalaosOsAPI::Instance()->restartApp({});
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

            update_cpuUsage(data["cpu_usage"].toInt());
            update_memoryUsage(data["mem_usage"].toInt());
            update_machineName(data["hostname"].toString());
            update_uptime(data["uptime"].toInt());
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
