#ifndef ADMINCODE_H
#define ADMINCODE_H

#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>

class HardwareUtils;

//Admin 2FA code pushed sealed with this device's push key
//(HardwareUtils::pushKey()), as specified by calaos_base
//docs/08_http_api.md, "Code push chiffré":
//
//  ks  = HMAC-SHA256(key, "calaos-admin-code/enc" || code_id || nonce)
//  ct  = code_ascii[6] XOR ks[0..6)
//  tag = HMAC-SHA256(key, "calaos-admin-code/mac" || code_id || nonce || action || expires || ct)[0..16)
//
//The platform stashes the payload as received with the reception time
//(no cryptography outside this file); the app checks the shapes, then the tag
//in constant time, then decrypts. The code is shown for Lifetime from the
//reception on the phone: expires comes from the box clock and is never
//compared to the phone clock.
namespace AdminCode
{

inline constexpr int KeySize = 32;
inline constexpr qint64 LifetimeMs = 60000;
//Wall clock steps back this much at most (NTP) before an entry counts as
//unreadable.
inline constexpr qint64 ClockSlackMs = 2000;

//One stashed payload: the six strings exactly as pushed, the reception time
//by the phone's wall clock (ms since the epoch) and by its boot clock (ms,
//CLOCK_BOOTTIME on Android, CLOCK_MONOTONIC on Darwin: both keep counting
//during sleep and are shared by every process; -1 when unknown).
struct Entry
{
    QString codeId;
    QString action;
    QString expires;
    QString nonce;
    QString ct;
    QString tag;
    qint64 receivedAt = 0;
    qint64 receivedMono = -1;
    //iOS notification request identifier, to withdraw it once consumed.
    QString notifId;

    static Entry fromJson(const QJsonObject &o);
    QJsonObject toJson() const;
};

//Stash entry for a push payload (the admin_code_* keys) received now.
QJsonObject entryFromPush(const QJsonObject &data, qint64 nowWall, qint64 nowMono,
                          const QString &notifId = QString());

bool isKnownAction(const QString &action);
//Step 1 of the reception: action in the table, expires in the strict
//RFC 3339 form, hex fields of the right length and lowercase.
bool isWellFormed(const Entry &e);

//The first 6 bytes of ks.
QByteArray keystream(const QByteArray &key, const QByteArray &codeId, const QByteArray &nonce);
//16 bytes.
QByteArray tag(const QByteArray &key, const QByteArray &codeId, const QByteArray &nonce,
               const QByteArray &action, const QByteArray &expires, const QByteArray &ct);
bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

//The 6 digits, or an empty string when the entry is malformed, the key is
//not 32 bytes or the tag does not match.
QString open(const QByteArray &key, const Entry &e);

//Display time left, <= 0 once expired, or when the clocks show the entry
//cannot be dated (reboot, wall clock set back).
qint64 remainingMs(const Entry &e, qint64 nowWall, qint64 nowMono);

qint64 wallNowMs();
qint64 monotonicNowMs();

} //namespace AdminCode

//The admin codes the code sheet (qml/mobile/AdminCodeSheet.qml) shows,
//newest first. Reads the stash when told a code arrived or was tapped
//(HardwareUtils::adminCodeNotified()), when the app comes to the foreground
//and once the UI is up; drops expired and rejected entries from the stash,
//and the shown ones when the sheet is closed.
class AdminCodeModel: public QObject
{
    Q_OBJECT
    //[{codeId, action, code, expired, remaining (s), fraction (0..1)}]
    Q_PROPERTY(QVariantList codes READ codes NOTIFY codesChanged)
    //A code arrived but could not be read on this phone (wrong key, forged,
    //malformed): the approver has to recover the key.
    Q_PROPERTY(bool unreadable READ unreadable NOTIFY codesChanged)
    Q_PROPERTY(int lifetime READ lifetime CONSTANT)

public:
    using Clock = std::function<qint64()>;

    explicit AdminCodeModel(HardwareUtils *hw, QObject *parent = nullptr);
    ~AdminCodeModel() override;

    //Tests drive time.
    void setClocks(Clock wall, Clock mono);

    QVariantList codes() const;
    bool unreadable() const { return m_unreadable; }
    int lifetime() const { return int(AdminCode::LifetimeMs / 1000); }

    //The UI is up: from now on a code found opens the sheet.
    void setReady();

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copy(const QString &codeId);
    //The sheet was closed: every code shown is consumed.
    Q_INVOKABLE void close();
    //Re-evaluates the countdowns now (also run by the timer).
    Q_INVOKABLE void tick();
    //The sheet is on screen: keep it out of screenshots.
    Q_INVOKABLE void setSheetVisible(bool visible);

    //Clears the clipboard if it still holds the copied code and its time is
    //up (also run on the timer and on return to the foreground).
    void expireClipboard();

signals:
    void codesChanged();
    //New codes to show: open the sheet.
    void showRequested();

private:
    struct Shown
    {
        AdminCode::Entry entry;
        QString code;
        bool expired = false;
    };

    void scheduleTick();
    void wipe(QString &s);

    HardwareUtils *m_hw;
    Clock m_wall;
    Clock m_mono;
    bool m_ready = false;
    bool m_unreadable = false;
    QList<Shown> m_shown;
    QTimer m_tick;

    QString m_clipText;
    qint64 m_clipDeadline = 0;
    QTimer m_clipTimer;
};

#endif // ADMINCODE_H
