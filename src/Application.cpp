#include "Application.h"
#include <QtQml>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QtAndroidExtras/QAndroidJniObject>
#endif

Application::Application(int & argc, char ** argv) :
    QGuiApplication(argc, argv)
{
    HardwareUtils::Instance(this);

    connect(HardwareUtils::Instance(), SIGNAL(networkStatusChanged()),
            this, SLOT(networkStatusChanged()));

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationWillResignActive, [=]()
    {
        qDebug() << "Application is in background, logout";
        logout();
    });

    connect(HardwareUtils::Instance(), &HardwareUtils::applicationBecomeActive, [=]()
    {
        //TOFIX: does not work...
        //qDebug() << "Application is in foreground, login again";
        //login(get_username(), get_password(), get_hostname());
    });

    QCoreApplication::setOrganizationName("Calaos");
    QCoreApplication::setOrganizationDomain("calaos.fr");
    QCoreApplication::setApplicationName("CalaosMobile");

    Common::registerQml();

#ifdef Q_OS_ANDROID
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative", "activity", "()Landroid/app/Activity;");
    QAndroidJniObject resource = activity.callObjectMethod("getResources","()Landroid/content/res/Resources;");
    QAndroidJniObject metrics = resource.callObjectMethod("getDisplayMetrics","()Landroid/util/DisplayMetrics;");
    update_density(metrics.getField<float>("density"));

    update_needBackButton(false);
    // Need to fix a bug on Android where text is scratched at runtime on some devices
    qputenv("QML_USE_GLYPHCACHE_WORKAROUND", QByteArray("1"));
#else
    if (arguments().contains("--force-hdpi"))
        update_density(2.0);
    else
        update_density(1.0);

    update_needBackButton(true);
#endif

    loadSettings();

    update_applicationStatus(Common::NotConnected);

    calaosConnect = new CalaosConnection(this);
    connect(calaosConnect, SIGNAL(homeLoaded(QVariantMap&)),
            this, SLOT(homeLoaded(QVariantMap&)));
    connect(calaosConnect, SIGNAL(loginFailed()),
            this, SLOT(loginFailed()));
    connect(calaosConnect, &CalaosConnection::disconnected, [=]()
    {
        update_applicationStatus(Common::NotConnected);
    });

    scenarioModel = new ScenarioModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("scenarioModel", scenarioModel);
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
    engine.rootContext()->setContextProperty("calaosApp", this);
    voiceApi = new VoiceApiAi(this);
    engine.rootContext()->setContextProperty("voiceApi", voiceApi);
    engine.load(QUrl(QStringLiteral("qrc:///qml/main.qml")));

    apiMatcher = new ApiMatcher(this);
    connect(voiceApi, SIGNAL(requestFinished(QVariantMap)), apiMatcher, SLOT(processJson(QVariantMap)));
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
    QString file = QString("%1/calaos.conf").arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    QSettings settings(file, QSettings::IniFormat);
    settings.clear();
    settings.sync();
    update_username("demo@calaos.fr");
    update_password("demo");
    update_hostname("calaos.fr");
    logout();
}

void Application::homeLoaded(QVariantMap &homeData)
{
    homeModel->load(homeData);
    audioModel->load(homeData);
    favHomeModel->load(homeData);
    update_applicationStatus(Common::LoggedIn);

    favModel->load(favoritesList);

    saveSettings();
}

void Application::loginFailed()
{
    if (m_applicationStatus == Common::NotConnected)
        return;

    qDebug() << "loginFailed called";

    update_applicationStatus(Common::NotConnected);

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

    //force @2x images for specific platform (android) as it's not done automatically by Qt
    if (needPictureHDPI())
        ret = QString("qrc:/%1/%2@2x.png").arg(prefix).arg(pic);
    else
        ret = QString("qrc:/%1/%2.png").arg(prefix).arg(pic);

    //qDebug() << "PIC: " << ret;

    return ret;
}

int Application::dp(int i)
{
    return double(i) * get_density();
}

void Application::saveSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    HardwareUtils::Instance()->saveAuthKeychain(get_username(), get_password());
    settings.setValue("calaos/host", get_hostname());

    favoritesList = favModel->save();
    settings.setValue("app/favorites", favoritesList);

    settings.sync();
}

void Application::loadSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    QString cnuser, cnpass;
    HardwareUtils::Instance()->loadAuthKeychain(cnuser, cnpass);
    if (cnuser.isEmpty()) cnuser = "demo@calaos.fr";
    if (cnpass.isEmpty()) cnpass = "demo";

    update_username(cnuser);
    update_password(cnpass);
    update_hostname(settings.value("calaos/host", "calaos.fr").toString());

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
