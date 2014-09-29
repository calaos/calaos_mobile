#ifndef HARDWAREUTILS_H
#define HARDWAREUTILS_H

#include <QtCore>

class HardwareUtils: public QObject
{
    Q_OBJECT

protected:
    HardwareUtils(QObject *parent = 0);

public:
    static HardwareUtils *Instance(QObject *parent = NULL);

    virtual ~HardwareUtils();

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual void showNetworkActivity(bool en);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);

    //This is used by private classes
    void emitNetworkStatusChanged();
    void emitApplicationActiveChanged(bool active);

signals:
    void networkStatusChanged();
    void applicationWillResignActive();
    void applicationBecomeActive();
};

#endif // HARDWAREUTILS_H
