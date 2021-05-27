#ifndef NOTIFICATIONSERVICE_H
#define NOTIFICATIONSERVICE_H

#include <QObject>
#include <firebase/messaging.h>

class NotificationService : public QObject
{
    Q_OBJECT
public:
    static NotificationService *Instance();

    explicit NotificationService(QObject *parent = 0);

    void setNotification(const QString &title, const QString &body, const QString &uuid);
    void handleMessage(const firebase::messaging::Message &message);

signals:
    void notificationChanged();

private slots:
    void updateAndroidNotification();

private:
    void registerNativeMethods();
    QString notifTitle;
    QString notifBody;
    QString notifEventUUID;
};

#endif // NOTIFICATIONSERVICE_H
