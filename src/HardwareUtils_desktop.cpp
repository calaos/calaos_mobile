#include "HardwareUtils_desktop.h"
#include <QMessageBox>
#include <stdexcept>
#include <QCommandLineParser>
#include <QDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QApplication>

#define PREFIX_CONFIG_PATH      ETC_DIR"/calaos"
#define LOCAL_CONFIG            "local_config.xml"
#define ETC_CONFIG_PATH         "/etc/calaos"
#define HOME_CONFIG_PATH        ".config/calaos"
#define HOME_CACHE_PATH         ".cache/calaos"
#define BCAST_UDP_PORT          4545

HardwareUtilsDesktop::HardwareUtilsDesktop(QObject *parent):
    HardwareUtils(parent)
{
}

HardwareUtilsDesktop::~HardwareUtilsDesktop()
{
}

void HardwareUtilsDesktop::platformInit()
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Calaos Home");
    parser.addHelpOption();

    QCommandLineOption confpath("config",
                            QCoreApplication::translate("config", "Set config path to <directory>"),
                            QCoreApplication::translate("config", "directory"));
    parser.addOption(confpath);
    QCommandLineOption cachepath("cache",
                                 QCoreApplication::translate("cache", "Set cache path to <directory>"),
                                 QCoreApplication::translate("cache", "directory"));
    parser.addOption(cachepath);
    parser.process(*qApp);

    initConfigOptions(parser.value(confpath), parser.value(cachepath));

    if (getConfigOption("show_cursor") == "true")
    {
        QPixmap pix(":/img/cursor.png");
        QApplication::setOverrideCursor(QCursor(pix, pix.width() / 2, pix.height() / 2));
    }
    else
    {
        //Create a transparent cursor
        QPixmap pix(1, 1);
        pix.fill(Qt::transparent);
        QApplication::setOverrideCursor(QCursor(pix));
    }

    qInfo() << "Trying to detect calaos_server on LAN...";
    udpSocket = new QUdpSocket(this);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &HardwareUtilsDesktop::calaosDiscover);
    calaosDiscover();
    timer->start(1000);
    connect(udpSocket, SIGNAL(readyRead()),
            this, SLOT(readPendingDatagrams()));

    QString h = getConfigOption("calaos_server_host");
    if (h != "")
    {
        QTimer::singleShot(10, [=]
        {
            calaosServerHost = h;
            qInfo() << "Force calaos_server on " << h << " from config file";

            emitCalaosServerDetected();
        });
    }
}

void HardwareUtilsDesktop::showAlertMessage(QString title, QString message, QString buttontext)
{
    QMessageBox box;
    box.setInformativeText(message);
    box.setText(title);
    box.addButton(buttontext, QMessageBox::AcceptRole);
    box.exec();
}

void HardwareUtilsDesktop::loadAuthKeychain(QString &email, QString &pass)
{
    email = getConfigOption("cn_user");
    pass = getConfigOption("cn_pass");
}

void HardwareUtilsDesktop::saveAuthKeychain(const QString &email, const QString &pass)
{
    setConfigOption("cn_user", email);
    setConfigOption("cn_pass", pass);
}

QString HardwareUtilsDesktop::getConfigFile(QString configType)
{
    if (configBase.isEmpty())
    {
        QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);

        QStringList confDirs;
        confDirs << QStringLiteral("%1/%2").arg(home).arg(HOME_CONFIG_PATH);
        confDirs << ETC_CONFIG_PATH;
        confDirs << PREFIX_CONFIG_PATH;

        //Check config in that order:
        // - $HOME/.config/calaos/
        // - /etc/calaos
        // - pkg_prefix/etc/calaos
        // - create $HOME/.config/calaos/ if nothing found

        foreach (const QString &conf, confDirs)
        {
            QDir d(conf);
            if (d.exists())
            {
                configBase = conf;
                break;
            }
        }

        if (configBase.isEmpty())
        {
            //no config dir found, create $HOME/.config/calaos
            configBase = QStringLiteral("%1/%2").arg(home).arg(HOME_CONFIG_PATH);
            QDir d(configBase);
            d.mkpath(".");
        }
    }

    return QStringLiteral("%1/%2").arg(configBase).arg(configType);
}

QString HardwareUtilsDesktop::getCacheFile(QString cacheFile)
{
    if (cacheBase.isEmpty())
    {
        QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        cacheBase = QStringLiteral("%1/%2").arg(home).arg(HOME_CACHE_PATH);
        QDir d(cacheBase);
        d.mkpath(".");
    }

    return QStringLiteral("%1/%2").arg(cacheBase).arg(cacheFile);
}

void HardwareUtilsDesktop::initConfigOptions(QString configdir, QString cachedir)
{
    if (!configdir.isEmpty())
        configDir = configdir;
    if (!cachedir.isEmpty())
        cacheDir = configdir;

    QString file = getConfigFile(LOCAL_CONFIG);

    qInfo() << "Using config path: " << getConfigFile("");
    qInfo() << "Using cache path: " << getCacheFile("");

    QFileInfo fconf(getConfigFile(""));
    if (!fconf.isWritable())
        throw (std::runtime_error("config path is not writable"));

    QFileInfo fcache(getCacheFile(""));
    if (!fcache.isWritable())
        throw (std::runtime_error("cache path is not writable"));

    if (!QFile::exists(file))
    {
        QFile conf(file);
        if (!conf.open(QFile::ReadWrite))
            throw (std::runtime_error("config path is not writable"));

        conf.write("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
        conf.write("<calaos:config xmlns:calaos=\"http://www.calaos.fr\">\n");
        conf.write("<calaos:option name=\"fw_version\" value=\"0\" />\n");
        conf.write("</calaos:config>");
        conf.close();

        setConfigOption("fw_version", "0");
        setConfigOption("show_cursor", "true");
        setConfigOption("dpms_enable", "false");
        setConfigOption("cn_user", "user");
        setConfigOption("cn_pass", "pass");
        setConfigOption("longitude", "2.322235");
        setConfigOption("latitude", "48.864715");

        qInfo() << "WARNING: no local_config.xml found, generating default config with username: \"user\" and password: \"pass\"" << endl;
    }
}

void HardwareUtilsDesktop::setConfigOption(QString key, QString value)
{
    auto values = getAllOptions();
    values[key] = value;

    QFile conf(getConfigFile(LOCAL_CONFIG));
    if (!conf.open(QFile::ReadWrite | QFile::Truncate))
        throw (std::runtime_error("config file is not writable"));

    QXmlStreamWriter stream(&conf);
    stream.setAutoFormatting(true);
    stream.writeStartDocument("1.0");
    stream.writeNamespace("http://www.calaos.fr", "calaos");
    stream.writeStartElement("http://www.calaos.fr", "config");

    for (auto i = values.constBegin();i != values.constEnd();i++)
    {
        stream.writeStartElement("http://www.calaos.fr", "option");
        QXmlStreamAttributes attr;
        attr.append("name", i.key());
        attr.append("value", i.value());
        stream.writeAttributes(attr);
        stream.writeEndElement();
    }
    stream.writeEndElement();
    stream.writeEndDocument();
}

QString HardwareUtilsDesktop::getConfigOption(QString key)
{
    auto values = getAllOptions();
    return values[key];
}

QHash<QString, QString> HardwareUtilsDesktop::getAllOptions()
{
    QFile conf(getConfigFile(LOCAL_CONFIG));
    if (!conf.open(QFile::ReadOnly))
        throw (std::runtime_error("config file is not writable"));

    QHash<QString, QString> values;
    QXmlStreamReader reader(&conf);

    while (!reader.atEnd())
    {
        reader.readNext();

        if (reader.isStartElement())
        {
            if (reader.name() == "option")
            {
                QXmlStreamAttributes attrs = reader.attributes();
                values[attrs.value("name").toString()] = attrs.value("value").toString();
            }
        }
    }
    if (reader.hasError())
    {
        qWarning() << "Failed to parse config XML file!";
        qWarning() << reader.errorString();
    }

    return values;
}

void HardwareUtilsDesktop::readPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        QHostAddress sender;
        quint16 senderPort;

        datagram.resize(udpSocket->pendingDatagramSize());
        udpSocket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);

        QString msg(datagram.left(9));
        QString ip(datagram.mid(9));

        if (msg != QString("CALAOS_IP"))
            return;

        if (calaosServerHost != ip)
        {
            calaosServerHost = ip;
            qInfo() << "Found calaos_server on " << ip;
        }

        emitCalaosServerDetected();
    }
}

void HardwareUtilsDesktop::calaosDiscover()
{
    QByteArray datagram = "CALAOS_DISCOVER";
    QHostAddress broadcastAddress = QHostAddress("255.255.255.255");
    udpSocket->writeDatagram(datagram.data(), datagram.size(), broadcastAddress , BCAST_UDP_PORT);
}

QString HardwareUtilsDesktop::getServerHost()
{
    return calaosServerHost;
}
