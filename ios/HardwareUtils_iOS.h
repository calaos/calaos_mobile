#ifndef HARDWAREUTILS_IOS_H
#define HARDWAREUTILS_IOS_H

#include "../src/HardwareUtils.h"

class HardwareUtils_iOS: public HardwareUtils
{
protected:
    friend class HardwareUtils;
    HardwareUtils_iOS(QObject *parent = 0);

    QString ioStartShortcut;

    //Admin codes consumed by this process: never captured again from a
    //notification still on screen.
    QSet<QString> consumedAdminCodes;

public:
    virtual ~HardwareUtils_iOS();

    //Installs the application/scene delegate hooks and registers for push
    //notifications, see the top of HardwareUtils.mm.
    virtual void platformInit(QQmlApplicationEngine *e);

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual QString getDeviceName();

    virtual void showNetworkActivity(bool en);
    
    virtual void setConfigOption(QString key, QString value);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual void resetAuthKeychain();

    //Kept in the App Group defaults: the notification service extension
    //reads it too and loads no picture after an explicit logout.
    virtual void setSessionActive(bool active);
    virtual bool isSessionActive();

    //Copied to the App Group defaults for the notification service extension.
    virtual void setTlsPins(const QVariantMap &pins);

    //Admin code stash: one JSON file per code in the App Group container
    //(admin_codes/<code_id>.json), written by the notification extension or,
    //when it did not run, by the app from the notification it sees.
    virtual QList<QJsonObject> adminCodeStash();
    virtual void stashAdminCode(const QJsonObject &entry);
    //Also withdraws the delivered notifications of these codes.
    virtual void removeAdminCodes(const QStringList &codeIds);
    //UIPasteboard item that expires by itself.
    virtual SecretCopy copySecret(const QString &text, int seconds);
    //Keychain item (own service, this device only); created only when the
    //keychain says it does not exist.
    virtual void loadPushKey();

    //Stashes (unless already there) and reports the admin code carried by a
    //notification the app sees: presented, tapped, or still delivered.
    void captureAdminCode(void *notification);

    virtual void inputTextDialog(const QString &title, const QString &message);

    bool hasNativePasswordEntry() const override { return true; }

    virtual QString getStartOption(const QString &key);
    virtual void setQuickLinks(QVariantList quicklinks);

    void handlePerformActionForShortcutItem(void *shortcut);
    void handleRegisterNotif(void *data);
    void handleRegisterNotifFail(void *err);

protected:
    bool showNativePasswordEntry(const QString &purpose, const QVariantMap &options) override;
};

#endif // HARDWAREUTILS_IOS_H
