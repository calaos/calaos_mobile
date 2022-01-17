#ifndef OSINSTALLER_H
#define OSINSTALLER_H

#include <QObject>
#include <QQmlApplicationEngine>
#include "qqmlhelpers.h"

class OSInstaller : public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(bool, isInstalling)
    QML_READONLY_PROPERTY(bool, installFinished)

public:
    explicit OSInstaller(QQmlApplicationEngine *engine, QObject *parent = nullptr);

    Q_INVOKABLE void startInstallation(QString disk);

private slots:
    void sendLog(QString line);

private:
    QQmlApplicationEngine *engine;

};

#endif // OSINSTALLER_H
