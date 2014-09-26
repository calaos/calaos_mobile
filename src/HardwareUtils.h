#ifndef HARDWAREUTILS_H
#define HARDWAREUTILS_H

#include <QtCore>

class HardwareUtils: public QObject
{
    Q_OBJECT

private:
    HardwareUtils(QObject *parent = 0);

public:
    static HardwareUtils *Instance(QObject *parent = NULL)
    {
        static HardwareUtils *hu = new HardwareUtils(parent);
        return hu;
    }

    ~HardwareUtils();

    void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    int getNetworkStatus();


    //This is used by private classes
    void emitNetworkStatusChanged();

signals:
    void networkStatusChanged();
};

#endif // HARDWAREUTILS_H
