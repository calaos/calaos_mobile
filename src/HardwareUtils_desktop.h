#ifndef HARDWAREUTILSDESK_H
#define HARDWAREUTILSDESK_H

#include "HardwareUtils.h"

class HardwareUtilsDesktop: public HardwareUtils
{
    Q_OBJECT

protected:
    friend class HardwareUtils;
    HardwareUtilsDesktop(QObject *parent = 0);

    QString configDir, cacheDir, configBase, cacheBase;
    QString getConfigFile(QString configType);
    QString getCacheFile(QString cacheFile);
    void initConfigOptions(QString configdir, QString cachedir);
    QHash<QString, QString> getAllOptions();

public:
    virtual ~HardwareUtilsDesktop();

    void platformInit();

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);

    virtual void setConfigOption(QString key, QString value);
    virtual QString getConfigOption(QString key);
};

#endif // HARDWAREUTILSDESK_H
