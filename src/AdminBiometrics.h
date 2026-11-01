#ifndef ADMINBIOMETRICS_H
#define ADMINBIOMETRICS_H

#include <QObject>
#include <QString>
#include <QTimer>
#include "qqmlhelpers.h"

class AdminRelay;
class AdminTransport;
class AdminUpdates;
class CalaosConnection;
class HardwareUtils;

/*
 * Unlocks the admin mode of a box with the biometrics of the phone.
 *
 * The admin session cannot be kept (15 min, bound to the socket), so the admin
 * password itself goes behind the biometrics, in the platform's protected
 * storage (HardwareUtils::biometric*). This class only decides: when to offer,
 * when to ask, when to send, when to forget. QSettings holds the state of each
 * box, never the secret.
 *
 * iOS drops the socket when the biometric prompt resigns the app, so a secret
 * that was read waits in memory, for a bounded time and for the same box,
 * until the box answers on the new socket.
 */
class AdminBiometrics: public QObject
{
    Q_OBJECT

    //Biometrics can be used for the box of the current socket
    QML_READONLY_PROPERTY(bool, available)
    //The password of that box is stored
    QML_READONLY_PROPERTY(bool, enabled)
    //Face ID, Touch ID, or the generic word
    QML_READONLY_PROPERTY(QString, name)
    //The password was just accepted: ask whether to keep it
    QML_READONLY_PROPERTY(bool, offerPending)

public:
    //What a read of the secret ends with, see HardwareUtils.
    enum Result { Ok, Canceled, Failed, Invalidated };

    static constexpr int HoldMs = 30000;
    static constexpr int OfferMs = 60000;

    AdminBiometrics(HardwareUtils *hw, AdminRelay *relay, AdminUpdates *updates,
                    AdminTransport *transport, QObject *parent = nullptr);
    ~AdminBiometrics() override;

    void bindConnection(CalaosConnection *con);

    Q_INVOKABLE void acceptOffer();
    Q_INVOKABLE void declineOffer();
    //Settings: this box stops using the biometrics, and is not offered again.
    Q_INVOKABLE void disable();

    //The certificate of this box was forgotten or replaced.
    void forgetBox(const QString &hostKey);
    //User logout, reset of all data.
    void forgetAll();

    //Tests shorten the delays.
    void setDelays(int holdMs, int offerMs);

private:
    enum BoxState { Unknown, Stored, NotOffered };

    void refresh();
    bool canUnlockNow() const;
    void maybeAsk();
    void trySend();
    void passwordAnswered(const QString &password, bool ok, const QString &code);
    void storeDone(const QString &key, int result);
    void readDone(const QString &key, int result, const QString &secret);
    void dropHeld();
    void dropOffer();

    BoxState boxState(const QString &key) const;
    void setBoxState(const QString &key, BoxState state);

    HardwareUtils *m_hw;
    AdminRelay *m_relay;
    AdminUpdates *m_updates;
    AdminTransport *m_transport;

    QTimer *m_holdTimer;
    QTimer *m_offerTimer;
    QTimer *m_askTimer;
    //Asked once per opening of the unlock card: a cancel leaves the field.
    bool m_asked = false;
    bool m_reading = false;
    QString m_readKey;
    //The unlock in flight carries the stored password, not a typed one.
    bool m_sendingStored = false;
    QString m_sentKey;
    QString m_heldSecret;
    QString m_heldKey;
    QString m_offerSecret;
    QString m_offerKey;
    QString m_storingKey;
};

#endif // ADMINBIOMETRICS_H
