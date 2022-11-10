#include "OSInstaller.h"
#include <QProcess>
#include <qfappdispatcher.h>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

OSInstaller::OSInstaller(QQmlApplicationEngine *eng, QObject *parent):
    QObject(parent),
    engine(eng)
{
    update_isInstalling(false);
    update_installFinished(false);
    update_installError(false);
}

void OSInstaller::startInstallation(QString disk)
{
    QProcess *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    sendLog("** Starting installation on disk " + disk);
    update_isInstalling(true);

    connect(proc, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus exitStatus)
    {
        Q_UNUSED(exitStatus)
        if (exitCode != 0)
        {
            sendLog("Install process exited with a non clean error code.");
            sendLog("Error.");

            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
            QVariantMap m = {{ "title", tr("Error") },
                             { "message", tr("Installation failed. See log...") },
                             { "button", tr("Close") },
                             { "timeout", 0 }};
            appDispatcher->dispatch("showNotificationMsg", m);
            update_installError(true);
        }
        else
        {
            sendLog("Installation done.");
        }

        update_installFinished(true);
    });

    connect(proc, &QProcess::errorOccurred, this,
            [=](QProcess::ProcessError err)
    {
        //read remaining bytes if any
        if (proc->bytesAvailable())
        {
            QString out(proc->readAll());
            qDebug().noquote() << out;

            QTextStream str(out.toLatin1());
            while (!str.atEnd())
            {
                QString line = str.readLine();
                sendLog(line);
            }
        }

        if (err == QProcess::FailedToStart)
        {
            sendLog("Failed to start installation process. Aborted!");

            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
            QVariantMap m = {{ "title", tr("Error") },
                             { "message", tr("Installation failed. See log...") },
                             { "button", tr("Close") },
                             { "timeout", 0 }};
            appDispatcher->dispatch("showNotificationMsg", m);

            update_installError(true);
            update_installFinished(true);
        }
        else if (err == QProcess::Crashed)
        {
            sendLog("Installation process crashed... Aborted!");

            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
            QVariantMap m = {{ "title", tr("Error") },
                             { "message", tr("Installation failed. See log...") },
                             { "button", tr("Close") },
                             { "timeout", 0 }};
            appDispatcher->dispatch("showNotificationMsg", m);

            update_installError(true);
            update_installFinished(true);
        }
    });

    connect(proc, &QProcess::readyRead,
            [=]()
    {
        QString out(proc->readAll());
        qDebug().noquote() << out;

        QTextStream str(out.toLatin1());
        while (!str.atEnd())
        {
            QString line = str.readLine();
            sendLog(line);
        }
    });

    proc->start("/usr/bin/calaos_install.sh",
                QStringList() << disk);
}

void OSInstaller::sendLog(QString line)
{
    QString s;
    for (int i = 0;i < line.count();i++)
    {
        if (line.at(i).isPrint() || line.at(i) == QChar(0x1B))
            s.append(line.at(i));
        else if (line.at(i) == QChar(0x08)) //Backspace
        {
            if (s.length() > 1)
                s.remove(s.length() - 1, 1);
        }
    }

    QString color{"nocolor"};
    if (s.length() > 0 &&
        s.at(0) == QChar(0x1B)) //color code
    {
        //extract color and string from line
        s.remove(QChar(0x1B));
        s.remove("[0m");
        auto c = s.mid(0, s.indexOf('m') + 1);
        s.remove(c);

        if (c == "[0;36m" || c == "[0;34m" || c == "[1;34m" || c == "[1;36m") color = "blue";
        if (c == "[0;31m" || c == "[1;31m") color = "red";
        if (c == "[0;33m" || c == "[1;33m") color = "yellow";
        if (c == "[0;32m" || c == "[1;32m") color = "green";
    }

    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
    QVariantMap m = {
        { "line", s },
        { "color", color },
    };
    appDispatcher->dispatch("newLogItem", m);
}
