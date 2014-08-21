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
    connect(calaosConnect, SIGNAL(disconnected()),
            this, SLOT(loginFailed()));

    scenarioModel = new ScenarioModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("scenarioModel", scenarioModel);
    homeModel = new HomeModel(&engine, calaosConnect, scenarioModel, this);
    engine.rootContext()->setContextProperty("homeModel", homeModel);
    audioModel = new AudioModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("audioModel", audioModel);
    favModel = new FavoritesModel(&engine, calaosConnect, this);
    engine.rootContext()->setContextProperty("favoritesModel", favModel);
    engine.rootContext()->setContextProperty("calaosApp", this);
    engine.load(QUrl(QStringLiteral("qrc:///qml/main.qml")));
}

void Application::login(QString user, QString pass, QString host)
{
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
    update_applicationStatus(Common::LoggedIn);

    favModel->load(favoritesList);

    saveSettings();
}

void Application::loginFailed()
{
    update_applicationStatus(Common::NotConnected);
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

    return false;
#endif
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
    QString file = QString("%1/calaos.conf").arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    QSettings settings(file, QSettings::NativeFormat);

    settings.setValue("calaos/cn_user", get_username());
    settings.setValue("calaos/cn_pass", get_password());
    settings.setValue("calaos/host", get_hostname());

    favoritesList = favModel->save();
    settings.setValue("app/favorites", favoritesList);

    settings.sync();
}

void Application::loadSettings()
{
    QString file = QString("%1/calaos.conf").arg(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    QSettings settings(file, QSettings::NativeFormat);

    update_username(settings.value("calaos/cn_user", "demo@calaos.fr").toString());
    update_password(settings.value("calaos/cn_pass", "demo").toString());
    update_hostname(settings.value("calaos/host", "calaos.fr").toString());

    favoritesList = settings.value("app/favorites").toList();
}

void Application::addItemFavorite(QString ioid, int type)
{
    favModel->addFavorite(ioid, type);

    saveSettings();
}
