#ifndef HARDWAREUTILS_H
#define HARDWAREUTILS_H

#include <QtCore>

class HardwareUtils: public QObject
{
    Q_OBJECT

public:
    HardwareUtils(QObject *parent = 0);

    void showAlertMessage(QString title, QString message);
};

#endif // HARDWAREUTILS_H
