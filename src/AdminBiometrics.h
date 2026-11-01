#ifndef ADMINBIOMETRICS_H
#define ADMINBIOMETRICS_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include "qqmlhelpers.h"

class AdminRelay;
class AdminTransport;
class AdminUpdates;
class CalaosConnection;
class HardwareUtils;

/*
 * Decides when the admin password of a box goes behind the biometrics, and
 * when it comes back: the platforms only store (HardwareUtils::biometric*),
 * QSettings only says which box has one. A password read waits in memory for a
 * new socket, as the iOS prompt drops the current one.
 */
class AdminBiometrics: public QObject
{
    Q_OBJECT

    //Biometrics can be used for the box of the current socket
    QML_READONLY_PROPERTY(bool, available)
    //The password of that box is stored
    QML_READONLY_PROPERTY(bool, stored)
    //Whole sentences: the proper name reads in them, the generic word does not
    QML_READONLY_PROPERTY(QString, keepText)
    QML_READONLY_PROPERTY(QString, turnOffText)

public:
    static constexpr int HoldMs = 30000;

    AdminBiometrics(HardwareUtils *hw, AdminRelay *relay, AdminUpdates *updates,
                    AdminTransport *transport, QObject *parent = nullptr);
    ~AdminBiometrics() override;

    void bindConnection(CalaosConnection *con);

    //The typed password, stored once the box has accepted it.
    Q_INVOKABLE void unlockAndKeep(const QString &password);
    //Settings: this box stops using the biometrics.
    Q_INVOKABLE void disable();

    //The certificate of this box was forgotten or replaced.
    void forgetBox(const QString &hostKey);
    //User logout, reset of all data.
    void forgetAll();

    //Tests shorten the delay.
    void setHoldDelay(int holdMs);

public slots:
    //Timers stop with the app: leaving ends the wait. Inactive does not, the
    //iOS prompt goes through it.
    void applicationStateChanged(Qt::ApplicationState state);

private:
    bool isStored(const QString &key) const;
    void setStored(const QString &key, bool stored);
    void refresh();
    bool canUnlockNow() const;
    void maybeAsk();
    void trySend();
    void unlockAnswered(bool ok, const QString &code);
    void storeDone(const QString &key, int result);
    void readDone(const QString &key, int result, const QString &secret);
    void dropHeld();
    void dropPending();

    HardwareUtils *m_hw;
    AdminRelay *m_relay;
    //Goes first when the application closes.
    QPointer<AdminUpdates> m_updates;
    AdminTransport *m_transport;

    QTimer *m_holdTimer;
    QTimer *m_askTimer;
    int m_holdMs = HoldMs;
    //Asked once per opening of the unlock card: a cancel leaves the field.
    bool m_asked = false;
    bool m_reading = false;
    QString m_readKey;
    //The unlock in flight carries the stored password, not a typed one.
    bool m_sendingStored = false;
    QString m_sentKey;
    QString m_heldSecret;
    QString m_heldKey;
    //Checked out of the timer too: suspended, it does not count.
    qint64 m_heldUntil = 0;
    //The typed password, until the box answers.
    QString m_pendingSecret;
    QString m_pendingKey;
    QString m_storingKey;
};

#endif // ADMINBIOMETRICS_H
