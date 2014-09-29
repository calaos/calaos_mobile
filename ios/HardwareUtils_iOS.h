#ifndef HARDWAREUTILS_IOS_H
#define HARDWAREUTILS_IOS_H

#include "../src/HardwareUtils.h"

class HardwareUtils_iOS: public HardwareUtils
{
protected:
    friend class HardwareUtils;
    HardwareUtils_iOS(QObject *parent = 0);

public:
    virtual ~HardwareUtils_iOS();

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
};

#endif // HARDWAREUTILS_IOS_H
