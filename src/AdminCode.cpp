#include "AdminCode.h"
#include "HardwareUtils.h"
#include <QClipboard>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QGuiApplication>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <algorithm>
#include <time.h>

namespace AdminCode
{

namespace
{

//Version 1 of the format: a new one changes the labels and the payload keys.
const char kEncLabel[] = "calaos-admin-code/enc";
const char kMacLabel[] = "calaos-admin-code/mac";

//Stash keys, shared with the platform writers (CalaosMessagingService.java,
//NotificationService.m, HardwareUtils.mm).
const QLatin1StringView kCodeId("code_id");
const QLatin1StringView kAction("action");
const QLatin1StringView kExpires("expires");
const QLatin1StringView kNonce("nonce");
const QLatin1StringView kCt("ct");
const QLatin1StringView kTag("tag");
const QLatin1StringView kReceivedAt("received_at");
const QLatin1StringView kReceivedMono("received_mono");
const QLatin1StringView kNotifId("notif_id");

bool isLowerHex(const QString &s, int length)
{
    if (s.size() != length)
        return false;
    for (const QChar c: s)
    {
        const char16_t u = c.unicode();
        if (!((u >= '0' && u <= '9') || (u >= 'a' && u <= 'f')))
            return false;
    }
    return true;
}

QByteArray hmac(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

qint64 toMs(const QJsonValue &v, qint64 fallback)
{
    //JSON numbers are doubles; milliseconds since 1970 fit exactly.
    return v.isDouble() ? qint64(v.toDouble()) : fallback;
}

} //namespace

Entry Entry::fromJson(const QJsonObject &o)
{
    Entry e;
    e.codeId = o.value(kCodeId).toString();
    e.action = o.value(kAction).toString();
    e.expires = o.value(kExpires).toString();
    e.nonce = o.value(kNonce).toString();
    e.ct = o.value(kCt).toString();
    e.tag = o.value(kTag).toString();
    e.receivedAt = toMs(o.value(kReceivedAt), 0);
    e.receivedMono = toMs(o.value(kReceivedMono), -1);
    e.notifId = o.value(kNotifId).toString();
    return e;
}

QJsonObject Entry::toJson() const
{
    QJsonObject o = {
        { kCodeId, codeId }, { kAction, action }, { kExpires, expires },
        { kNonce, nonce }, { kCt, ct }, { kTag, tag },
        { kReceivedAt, double(receivedAt) },
    };
    if (receivedMono >= 0)
        o[kReceivedMono] = double(receivedMono);
    if (!notifId.isEmpty())
        o[kNotifId] = notifId;
    return o;
}

QJsonObject entryFromPush(const QJsonObject &data, qint64 nowWall, qint64 nowMono, const QString &notifId)
{
    Entry e;
    e.codeId = data.value(QLatin1StringView("admin_code_id")).toString();
    e.action = data.value(QLatin1StringView("admin_code_action")).toString();
    e.expires = data.value(QLatin1StringView("admin_code_expires")).toString();
    e.nonce = data.value(QLatin1StringView("admin_code_nonce")).toString();
    e.ct = data.value(QLatin1StringView("admin_code_ct")).toString();
    e.tag = data.value(QLatin1StringView("admin_code_tag")).toString();
    e.receivedAt = nowWall;
    e.receivedMono = nowMono;
    e.notifId = notifId;
    return e.toJson();
}

bool isKnownAction(const QString &action)
{
    static const QStringList actions = {
        QStringLiteral("console_open"), QStringLiteral("admin_password"),
        QStringLiteral("approver_enroll"), QStringLiteral("approver_remove"),
        QStringLiteral("approver_confirm"), QStringLiteral("network_config"),
    };
    return actions.contains(action);
}

bool isWellFormed(const Entry &e)
{
    //action || expires has no length prefix: unambiguous only in this form.
    //Anchored: "$" alone would accept a trailing newline.
    static const QRegularExpression expiresRe(QRegularExpression::anchoredPattern(
        QStringLiteral("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z")));
    return isKnownAction(e.action) &&
           expiresRe.match(e.expires).hasMatch() &&
           isLowerHex(e.codeId, 16) &&
           isLowerHex(e.nonce, 32) &&
           isLowerHex(e.ct, 12) &&
           isLowerHex(e.tag, 32);
}

QByteArray keystream(const QByteArray &key, const QByteArray &codeId, const QByteArray &nonce)
{
    return hmac(key, QByteArray(kEncLabel) + codeId + nonce).left(6);
}

QByteArray tag(const QByteArray &key, const QByteArray &codeId, const QByteArray &nonce,
               const QByteArray &action, const QByteArray &expires, const QByteArray &ct)
{
    return hmac(key, QByteArray(kMacLabel) + codeId + nonce + action + expires + ct).left(16);
}

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    //Lengths are public (fixed by the format).
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); i++)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

QString open(const QByteArray &key, const Entry &e)
{
    if (key.size() != KeySize || !isWellFormed(e))
        return QString();

    const QByteArray codeId = e.codeId.toLatin1();
    const QByteArray nonce = QByteArray::fromHex(e.nonce.toLatin1());
    const QByteArray ct = QByteArray::fromHex(e.ct.toLatin1());
    //Recomputed from the strings as received: expires is not reformatted.
    const QByteArray expected = tag(key, codeId, nonce, e.action.toLatin1(), e.expires.toLatin1(), ct);
    if (!constantTimeEquals(expected, QByteArray::fromHex(e.tag.toLatin1())))
        return QString();

    QByteArray ks = keystream(key, codeId, nonce);
    QString code;
    for (int i = 0; i < 6; i++)
    {
        const char c = char(ct.at(i) ^ ks.at(i));
        if (c < '0' || c > '9')
        {
            code.clear();
            break;
        }
        code += QLatin1Char(c);
    }
    ks.fill('\0');
    return code;
}

qint64 remainingMs(const Entry &e, qint64 nowWall, qint64 nowMono)
{
    if (e.receivedAt <= 0)
        return 0;

    const qint64 wallAge = nowWall - e.receivedAt;
    if (wallAge < -ClockSlackMs)
        return 0;
    qint64 age = std::max<qint64>(wallAge, 0);

    if (e.receivedMono >= 0)
    {
        //Negative: the phone rebooted since. A reboot that leaves it positive
        //is caught by the wall clock, which then says more than a minute.
        const qint64 monoAge = nowMono - e.receivedMono;
        if (monoAge < 0)
            return 0;
        age = std::max(age, monoAge);
    }
    return LifetimeMs - age;
}

qint64 wallNowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 monotonicNowMs()
{
#if defined(Q_OS_DARWIN)
    return qint64(clock_gettime_nsec_np(CLOCK_MONOTONIC) / 1000000);
#elif defined(Q_OS_LINUX)
    //Android's SystemClock.elapsedRealtime(), which the messaging service stamps.
    timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return qint64(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#else
    return QDeadlineTimer::current().deadline();
#endif
}

} //namespace AdminCode

using namespace AdminCode;

AdminCodeModel::AdminCodeModel(HardwareUtils *hw, QObject *parent):
    QObject(parent),
    m_hw(hw),
    m_wall(&AdminCode::wallNowMs),
    m_mono(&AdminCode::monotonicNowMs)
{
    m_tick.setSingleShot(true);
    connect(&m_tick, &QTimer::timeout, this, &AdminCodeModel::tick);

    m_clipTimer.setSingleShot(true);
    connect(&m_clipTimer, &QTimer::timeout, this, &AdminCodeModel::expireClipboard);

    connect(m_hw, &HardwareUtils::adminCodeNotified, this, [this]()
    {
        if (!m_ready)
            return;
        m_hw->takePendingAdminCode();
        refresh();
    });

    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    {
        connect(app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state)
        {
            if (state != Qt::ApplicationActive)
                return;
            expireClipboard();
            refresh();
        });
    }
}

AdminCodeModel::~AdminCodeModel()
{
    for (Shown &s: m_shown)
        wipe(s.code);
    wipe(m_clipText);
}

void AdminCodeModel::setClocks(Clock wall, Clock mono)
{
    m_wall = wall ? wall : Clock(&AdminCode::wallNowMs);
    m_mono = mono ? mono : Clock(&AdminCode::monotonicNowMs);
}

void AdminCodeModel::setReady()
{
    m_ready = true;
    m_hw->takePendingAdminCode();
    refresh();
}

void AdminCodeModel::wipe(QString &s)
{
    s.fill(QLatin1Char('0'));
    s.clear();
}

QVariantList AdminCodeModel::codes() const
{
    const qint64 wall = m_wall(), mono = m_mono();
    QVariantList list;
    for (const Shown &s: m_shown)
    {
        const qint64 left = s.expired ? 0 : std::max<qint64>(remainingMs(s.entry, wall, mono), 0);
        list.append(QVariantMap {
            { QStringLiteral("codeId"), s.entry.codeId },
            { QStringLiteral("action"), s.entry.action },
            { QStringLiteral("code"), s.expired ? QString() : s.code },
            { QStringLiteral("expired"), s.expired },
            { QStringLiteral("remaining"), int((left + 999) / 1000) },
            { QStringLiteral("fraction"), double(left) / double(LifetimeMs) },
        });
    }
    return list;
}

void AdminCodeModel::refresh()
{
    if (!m_ready)
        return;

    //No key yet (store unavailable): the stash waits untouched for it.
    if (m_hw->pushKey().isEmpty())
        m_hw->loadPushKey();
    const QByteArray key = m_hw->pushKey();
    if (key.isEmpty())
    {
        tick();
        return;
    }

    const qint64 wall = m_wall(), mono = m_mono();
    QStringList dropped;
    int rejected = 0;
    bool added = false;

    for (const QJsonObject &o: m_hw->adminCodeStash())
    {
        const Entry e = Entry::fromJson(o);
        if (e.codeId.isEmpty())
            continue;
        const bool known = std::any_of(m_shown.cbegin(), m_shown.cend(), [&e](const Shown &s)
        {
            return s.entry.codeId == e.codeId;
        });
        if (known)
            continue;

        if (remainingMs(e, wall, mono) <= 0)
        {
            dropped << e.codeId;
            continue;
        }

        Shown s;
        s.entry = e;
        s.code = AdminCode::open(key, e);
        if (s.code.isEmpty())
        {
            //Malformed, wrong key or forged: dropped silently.
            rejected++;
            dropped << e.codeId;
            continue;
        }
        m_shown.append(s);
        added = true;
    }

    if (rejected > 0)
        qWarning() << "Admin code:" << rejected << "payload(s) rejected (malformed or bad tag)";
    if (!dropped.isEmpty())
        m_hw->removeAdminCodes(dropped);

    if (added)
    {
        std::stable_sort(m_shown.begin(), m_shown.end(), [](const Shown &a, const Shown &b)
        {
            if (a.entry.receivedAt != b.entry.receivedAt)
                return a.entry.receivedAt > b.entry.receivedAt;
            return a.entry.receivedMono > b.entry.receivedMono;
        });
    }

    tick();

    if (added)
        emit showRequested();
}

void AdminCodeModel::setSheetVisible(bool visible)
{
    m_hw->setSecureScreen(visible);
}

void AdminCodeModel::tick()
{
    const qint64 wall = m_wall(), mono = m_mono();
    QStringList expired;
    for (Shown &s: m_shown)
    {
        if (s.expired || remainingMs(s.entry, wall, mono) > 0)
            continue;
        s.expired = true;
        wipe(s.code);
        expired << s.entry.codeId;
    }
    if (!expired.isEmpty())
        m_hw->removeAdminCodes(expired);

    emit codesChanged();
    scheduleTick();
}

void AdminCodeModel::scheduleTick()
{
    const qint64 wall = m_wall(), mono = m_mono();
    qint64 next = -1;
    for (const Shown &s: m_shown)
    {
        if (s.expired)
            continue;
        //Fires when the displayed number of seconds changes.
        const qint64 left = remainingMs(s.entry, wall, mono);
        const qint64 in = left > 0 ? ((left - 1) % 1000) + 1 : 1;
        next = next < 0 ? in : std::min(next, in);
    }
    if (next < 0)
        m_tick.stop();
    else
        m_tick.start(int(next));
}

void AdminCodeModel::copy(const QString &codeId)
{
    for (const Shown &s: std::as_const(m_shown))
    {
        if (s.entry.codeId != codeId || s.expired || s.code.isEmpty())
            continue;

        const int seconds = int(LifetimeMs / 1000);
        const HardwareUtils::SecretCopy done = m_hw->copySecret(s.code, seconds);
        if (done == HardwareUtils::SecretCopy::CopiedWithExpiry)
            return;
        if (done == HardwareUtils::SecretCopy::NotHandled)
        {
            QClipboard *clipboard = QGuiApplication::clipboard();
            if (!clipboard)
                return;
            clipboard->setText(s.code);
        }
        m_clipText = s.code;
        m_clipDeadline = m_mono() + LifetimeMs;
        m_clipTimer.start(int(LifetimeMs));
        return;
    }
}

void AdminCodeModel::expireClipboard()
{
    if (m_clipText.isEmpty())
        return;

    const qint64 left = m_clipDeadline - m_mono();
    if (left > 0)
    {
        m_clipTimer.start(int(left));
        return;
    }

#ifdef Q_OS_ANDROID
    //A background app reads an empty clipboard: retried on return to the
    //foreground.
    if (QGuiApplication::applicationState() != Qt::ApplicationActive)
        return;
#endif

    QClipboard *clipboard = QGuiApplication::clipboard();
    //Left alone if the user copied something else since.
    if (clipboard && clipboard->text() == m_clipText)
        clipboard->clear();
    wipe(m_clipText);
}

void AdminCodeModel::close()
{
    QStringList ids;
    for (Shown &s: m_shown)
    {
        //Expired ones were removed from the stash already.
        if (!s.expired)
            ids << s.entry.codeId;
        wipe(s.code);
    }
    m_shown.clear();
    m_tick.stop();
    if (!ids.isEmpty())
        m_hw->removeAdminCodes(ids);
    emit codesChanged();
}
