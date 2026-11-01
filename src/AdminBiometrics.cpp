#include "AdminBiometrics.h"
#include "AdminRelay.h"
#include "AdminTransport.h"
#include "AdminUpdates.h"
#include "CalaosConnection.h"
#include "HardwareUtils.h"
#include <QCoreApplication>
#include <QSettings>
#include <QUrl>

static const QString SettingsGroup = QStringLiteral("admin_biometrics");
static const QString BadPassword = QStringLiteral("bad_password");

static void wipe(QString &secret)
{
    secret.fill(QChar(u'\0'));
    secret.clear();
}

static QString settingsKey(const QString &hostKey)
{
    return SettingsGroup + QLatin1Char('/') + QString::fromLatin1(QUrl::toPercentEncoding(hostKey));
}

AdminBiometrics::AdminBiometrics(HardwareUtils *hw, AdminRelay *relay, AdminUpdates *updates,
                                 AdminTransport *transport, QObject *parent):
    QObject(parent),
    m_hw(hw),
    m_relay(relay),
    m_updates(updates),
    m_transport(transport)
{
    update_available(false);
    update_enabled(false);
    update_offerPending(false);
    update_name(hw->biometricName());

    m_holdTimer = new QTimer(this);
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(HoldMs);
    connect(m_holdTimer, &QTimer::timeout, this, &AdminBiometrics::dropHeld);

    m_offerTimer = new QTimer(this);
    m_offerTimer->setSingleShot(true);
    m_offerTimer->setInterval(OfferMs);
    connect(m_offerTimer, &QTimer::timeout, this, &AdminBiometrics::dropOffer);

    //A socket that drops ends the session first: the gate is locked for an
    //instant, which must not bring a prompt up over an app going away.
    m_askTimer = new QTimer(this);
    m_askTimer->setSingleShot(true);
    m_askTimer->setInterval(0);
    connect(m_askTimer, &QTimer::timeout, this, &AdminBiometrics::maybeAsk);

    connect(hw, &HardwareUtils::biometricStoreDone, this, &AdminBiometrics::storeDone);
    connect(hw, &HardwareUtils::biometricReadDone, this, &AdminBiometrics::readDone);

    connect(relay, &AdminRelay::gateChanged, this, [this]()
    {
        refresh();
        if (m_relay->gate() != AdminRelay::GateUnlocked)
            dropOffer();
        trySend();
        m_askTimer->start();
    });
    connect(relay, &AdminRelay::transportUp, this, &AdminBiometrics::refresh);
    connect(relay, &AdminRelay::sessionChanged, this, [this]()
    {
        //A session that ended is a new unlock to open.
        if (!m_relay->isUnlocked())
            m_asked = false;
    });

    connect(updates, &AdminUpdates::activeChanged, this, [this]()
    {
        m_asked = false;
        if (!m_updates->isActive())
        {
            dropHeld();
            dropOffer();
            return;
        }
        refresh();
        m_askTimer->start();
    });
    connect(updates, &AdminUpdates::busyChanged, this, [this]()
    {
        trySend();
        m_askTimer->start();
    });

    updates->setUnlockObserver([this](const QString &password, bool ok, const QString &code)
    {
        passwordAnswered(password, ok, code);
    });
}

AdminBiometrics::~AdminBiometrics()
{
    m_updates->setUnlockObserver(nullptr);
    wipe(m_heldSecret);
    wipe(m_offerSecret);
}

void AdminBiometrics::bindConnection(CalaosConnection *con)
{
    //A new key may be a new machine behind the same name: it must prove
    //itself with a typed password.
    connect(con, &CalaosConnection::tlsTrustAccepted, this, &AdminBiometrics::forgetBox);
}

void AdminBiometrics::setDelays(int holdMs, int offerMs)
{
    m_holdTimer->setInterval(holdMs);
    m_offerTimer->setInterval(offerMs);
}

AdminBiometrics::BoxState AdminBiometrics::boxState(const QString &key) const
{
    if (key.isEmpty())
        return Unknown;
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return BoxState(settings.value(settingsKey(key), int(Unknown)).toInt());
}

void AdminBiometrics::setBoxState(const QString &key, BoxState state)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (state == Unknown)
        settings.remove(settingsKey(key));
    else
        settings.setValue(settingsKey(key), int(state));
    settings.sync();
}

void AdminBiometrics::refresh()
{
    const QString key = m_transport->adminHostKey();
    const bool usable = !key.isEmpty() && m_hw->hasBiometrics();
    update_available(usable);
    update_enabled(usable && boxState(key) == Stored);
}

bool AdminBiometrics::canUnlockNow() const
{
    return m_updates->get_busy().isEmpty() && m_updates->get_retryAfter() <= 0;
}

void AdminBiometrics::maybeAsk()
{
    if (m_asked || m_reading || !m_heldSecret.isEmpty() || !m_updates->isActive())
        return;
    if (m_relay->gate() != AdminRelay::GateLocked || !canUnlockNow())
        return;
    if (m_transport->hasPendingTlsTrust())
        return;
    const QString key = m_transport->adminHostKey();
    if (key.isEmpty() || !m_hw->hasBiometrics() || boxState(key) != Stored)
        return;

    m_asked = true;
    m_reading = true;
    m_readKey = key;
    m_hw->biometricRead(key, tr("Unlock the administration of the box"));
}

void AdminBiometrics::readDone(const QString &key, int result, const QString &secret)
{
    if (!m_reading || key != m_readKey)
        return;
    m_reading = false;

    if (result == Invalidated)
    {
        forgetBox(key);
        return;
    }
    if (result != Ok || secret.isEmpty() || !m_updates->isActive())
        return;

    m_heldSecret = secret;
    m_heldKey = key;
    m_holdTimer->start();
    trySend();
}

void AdminBiometrics::trySend()
{
    if (m_heldSecret.isEmpty())
        return;

    const QString gate = m_relay->gate();
    //the socket is being reopened after the prompt: wait for the box
    if (gate == AdminRelay::GateIdle || gate == AdminRelay::GateProbing)
        return;
    if (gate != AdminRelay::GateLocked)
    {
        dropHeld();
        return;
    }
    const QString key = m_transport->adminHostKey();
    if (key.isEmpty())
        return;
    //never to another box than the one it was stored for
    if (key != m_heldKey)
    {
        dropHeld();
        return;
    }
    if (m_transport->hasPendingTlsTrust() || !canUnlockNow())
        return;

    QString secret = m_heldSecret;
    dropHeld();
    m_sendingStored = true;
    m_sentKey = key;
    m_updates->unlock(secret);
    wipe(secret);
}

void AdminBiometrics::passwordAnswered(const QString &password, bool ok, const QString &code)
{
    const bool stored = m_sendingStored;
    m_sendingStored = false;

    if (stored)
    {
        //the box does not take it any more: back to the field for good
        if (code == BadPassword)
            forgetBox(m_sentKey);
        return;
    }

    if (!ok)
        return;
    const QString key = m_transport->adminHostKey();
    if (key.isEmpty() || !m_hw->hasBiometrics() || boxState(key) != Unknown)
        return;

    //once per box, whatever the answer
    setBoxState(key, NotOffered);
    m_offerSecret = password;
    m_offerKey = key;
    m_offerTimer->start();
    update_offerPending(true);
}

void AdminBiometrics::acceptOffer()
{
    if (m_offerSecret.isEmpty())
        return;
    m_storingKey = m_offerKey;
    m_hw->biometricStore(m_offerKey, m_offerSecret, tr("Keep the admin password of this box"));
    dropOffer();
}

void AdminBiometrics::declineOffer()
{
    dropOffer();
}

void AdminBiometrics::storeDone(const QString &key, int result)
{
    if (key != m_storingKey)
        return;
    m_storingKey.clear();
    if (result == Ok)
        setBoxState(key, Stored);
    refresh();
}

void AdminBiometrics::disable()
{
    const QString key = m_transport->adminHostKey();
    if (key.isEmpty())
        return;
    dropHeld();
    m_hw->biometricRemove(key);
    setBoxState(key, NotOffered);
    refresh();
}

void AdminBiometrics::forgetBox(const QString &hostKey)
{
    if (m_heldKey == hostKey)
        dropHeld();
    if (m_offerKey == hostKey)
        dropOffer();
    m_hw->biometricRemove(hostKey);
    setBoxState(hostKey, Unknown);
    refresh();
}

void AdminBiometrics::forgetAll()
{
    dropHeld();
    dropOffer();
    m_hw->biometricRemoveAll();
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove(SettingsGroup);
    settings.sync();
    refresh();
}

void AdminBiometrics::dropHeld()
{
    m_holdTimer->stop();
    wipe(m_heldSecret);
    m_heldKey.clear();
}

void AdminBiometrics::dropOffer()
{
    m_offerTimer->stop();
    wipe(m_offerSecret);
    m_offerKey.clear();
    update_offerPending(false);
}
