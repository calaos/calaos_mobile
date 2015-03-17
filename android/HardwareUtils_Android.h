#ifndef HARDWAREUTILS_ANDROID_H
#define HARDWAREUTILS_ANDROID_H

#include "../src/HardwareUtils.h"

class HardwareUtils_Android: public HardwareUtils
{
private:
    friend class HardwareUtils;
    HardwareUtils_Android(QObject *parent = 0);

public:
    virtual ~HardwareUtils_Android();

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();
};

#endif // HARDWAREUTILS_ANDROID_H
