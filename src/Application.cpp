#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include "RoomFilterModel.h"
#ifdef Q_OS_ANDROID
#include <QtAndroidExtras/QAndroidJniObject>
#endif
#include <QProcess>
#include <qfappdispatcher.h>

#ifdef CALAOS_DESKTOP
#include "CalaosWidgetModel.h"
#include "WeatherInfo.h"
#include "ScreenManager.h"
#endif

Application::Application(int & argc, char ** argv) :
    QAPP(argc, argv)
{
    QCoreApplication::setOrganizationName("Calaos");
    QCoreApplication::setOrganizationDomain("calaos.fr");
    QCoreApplication::setApplicationName("CalaosHome");

    HardwareUtils::Instance()->setParent(this);

#ifdef CALAOS_DESKTOP
    WeatherModel::registerQmlClasses();
#endif
}

Application::~Application()
{
#ifdef CALAOS_DESKTOP
    CalaosWidgetModel::Instance()->saveToDisk();
#endif
}

void Application::createQmlApp()
{
    loadSettings();

    setupLanguage();

    engine.addImportPath("qrc:/qml/");

    connect(HardwareUtils::Instance(), SIGNAL(networkStatusChanged()),
            this, SLOT(networkStatusChanged()));
    connect(HardwareUtils::Instance(), SIGNAL(calaosServerDetected()),
            this, SLOT(calaosServerDetected()));

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationWillResignActive, [=]()
    {
        qDebug() << "Application is in background, logout";
        startedWithOptHandled = false;
        logout();
    });

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationBecomeActive, [=]()
    {
        qDebug() << "Application is in foreground, login again";
        QTimer::singleShot(0, [=]()
        {
            login(get_username(), get_password(), get_hostname());
        });
    });

    Common::registerQml();

#ifdef Q_OS_ANDROID
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    QAndroidJniObject resource = activity.callObjectMethod("getResources","()Landroid/content/res/Resources;");
    QAndroidJniObject metrics = resource.callObjectMethod("getDisplayMetrics","()Landroid/util/DisplayMetrics;");
    update_density(metrics.getField<float>("density"));

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
#else
    update_isAndroid(false);
    update_isIOS(false);
    update_isDesktop(true);
#endif
#endif

    update_applicationStatus(Common::NotConnected);

    calaosConnect = new CalaosConnection(this);
    connect(calaosConnect, SIGNAL(homeLoaded(QVariantMap)),
            this, SLOT(homeLoaded(QVariantMap)));
    connect(calaosConnect, SIGNAL(loginFailed()),
            this, SLOT(loginFailed()));
    connect(calaosConnect, &CalaosConnection::disconnected, [=]()
    {
        update_applicationStatus(Common::NotConnected);

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

    m_netAddresses = new QQmlObjectListModel<NetworkInfo>(this);

#ifdef CALAOS_DESKTOP
    CalaosWidgetModel::Instance()->loadFromDisk();
    engine.rootContext()->setContextProperty("widgetsModel", CalaosWidgetModel::Instance());
    engine.rootContext()->setContextProperty("screenManager", &ScreenManager::Instance());

    update_machineName(Machine::getHostname());
    QList<NetworkInfo *> nets = Machine::getNetworkInfo();
    for (int i = 0;i < nets.count();i++)
    {
        if (nets.at(i)->get_isLoopback())
            continue;
        m_netAddresses->append(nets.at(i));
    }

    sysInfoTimer = new QTimer();
    connect(sysInfoTimer, SIGNAL(timeout()), this, SLOT(sysInfoTimerSlot()));
    sysInfoTimer->start(5000);
    sysInfoTimerSlot();
#endif

    engine.rootContext()->setContextProperty("calaosApp", this);

    //Register Units singleton
    //qmlRegisterSingletonType(QUrl("qrc:/qml/Units.qml"), "Units", 1, 0, "Units");

    qmlRegisterType<RoomFilterModel>("Calaos", 1, 0, "RoomFilterModel");
    qmlRegisterUncreatableType<QQmlObjectListModelBase> ("Calaos", 1, 0, "ObjectListModel",  "QQmlObjectListModel not creatable");
    qmlRegisterUncreatableType<QQmlVariantListModel> ("Calaos", 1, 0, "VariantListModel",  "QQmlVariantListModel not creatable");

#if defined(CALAOS_MOBILE)
    engine.load(QUrl(QStringLiteral("qrc:///qml/mobile/main.qml")));
#elif defined(CALAOS_DESKTOP)
    engine.load(QUrl(QStringLiteral("qrc:///qml/desktop/main.qml")));
#else
#error "Unknown UI type!"
#endif

#ifndef CALAOS_DESKTOP
    //Start autologin, only on mobile. On desktop we wait for calaos_server detection
    QTimer::singleShot(100, [=]()
    {
        login(get_username(), get_password(), get_hostname());
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
            //If app has been started with notification option, it should open it
            QTimer::singleShot(400, [=]()
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
}

void Application::loginFailed()
{
    homeModel->clear();
    audioModel->clear();
    scenarioModel->clear();
    scenarioSortModel->clear();
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

    //force @2x images for specific platform (android) as it's not done automatically by Qt
    if (needPictureHDPI())
        ret = QString("qrc:/%1/%2@2x.png").arg(prefix).arg(pic);
    else
        ret = QString("qrc:/%1/%2.png").arg(prefix).arg(pic);

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
    QProcess::startDetached("/bin/sh", QStringList() <<
                            "-c" <<
                            "sync; reboot");
#endif
}

void Application::restartApp()
{
    qInfo() << "Restart of calaos_home requested";
#ifdef CALAOS_DESKTOP
    this->quit();
    QProcess::startDetached(arguments()[0], arguments());
#endif
}

quint32 Application::getUptimeDays()
{
    return Machine::getMachineUptime() / 60 / 60 / 24;
}

void Application::sysInfoTimerSlot()
{
    update_cpuUsage(Machine::getCpuUsage());
    update_memoryUsage(Machine::getMemoryUsage());
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

    QTranslator translator;

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

            translator.load(QString()); // unload()
        }
        else if (locale == QStringLiteral("C") ||
                 locale == QStringLiteral("en"))
        {
            //use built-in
            break;
        }
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
    engine.retranslate();
#endif
}

void Application::setLanguage(QString code)
{
    HardwareUtils::Instance()->setConfigOption("lang", code);
    setupLanguage();
}
