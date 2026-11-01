#include "AdminBiometrics.h"
#include "AdminRelay.h"
#include "AdminTransport.h"
#include "AdminUpdates.h"
#include "CalaosConnection.h"
#include "HardwareUtils.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QSettings>
#include <QUrl>

static const QString SettingsGroup = QStringLiteral("admin_biometrics");
static const QString SettingsSeen = QStringLiteral("admin_biometrics_seen");
static const QString BadPassword = QStringLiteral("bad_password");

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
    update_stored(false);

    update_name(hw->biometricName());

    //The iOS keychain outlives the app: what a removed install left must not
    //come back with this one. Marked only once done, or it is tried again.
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (!settings.value(SettingsSeen, false).toBool() && hw->biometricRemoveAll())
    {
        settings.setValue(SettingsSeen, true);
        settings.sync();
    }

    m_holdTimer = new QTimer(this);
    m_holdTimer->setSingleShot(true);
    connect(m_holdTimer, &QTimer::timeout, this, &AdminBiometrics::dropHeld);

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
            dropPending();
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

    updates->setUnlockObserver([this](bool ok, const QString &code)
    {
        unlockAnswered(ok, code);
    });

    if (qGuiApp)
    {
        connect(qGuiApp, &QGuiApplication::applicationStateChanged,
                this, &AdminBiometrics::applicationStateChanged);
    }
}

AdminBiometrics::~AdminBiometrics()
{
    if (m_updates)
        m_updates->setUnlockObserver(nullptr);
}

void AdminBiometrics::bindConnection(CalaosConnection *con)
{
    //A new key may be a new machine behind the same name: it must prove
    //itself with a typed password.
    connect(con, &CalaosConnection::tlsTrustAccepted, this, &AdminBiometrics::forgetBox);
    connect(con, &CalaosConnection::tlsTrustRequested, this, &AdminBiometrics::dropHeld);
}

void AdminBiometrics::setHoldDelay(int holdMs)
{
    m_holdMs = holdMs;
}

void AdminBiometrics::applicationStateChanged(Qt::ApplicationState state)
{
    if (state != Qt::ApplicationSuspended && state != Qt::ApplicationHidden)
        return;
    dropHeld();
    dropPending();
    //an answer delivered after leaving is no recent gesture: the field remains
    m_reading = false;
    m_readKey.clear();
    cancelStore();
}

void AdminBiometrics::cancelStore()
{
    if (m_storingKey.isEmpty())
        return;
    m_storingKey.clear();
    m_hw->biometricCancelStore();
}

bool AdminBiometrics::isStored(const QString &key) const
{
    if (key.isEmpty())
        return false;
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return settings.value(settingsKey(key), false).toBool();
}

void AdminBiometrics::setStored(const QString &key, bool stored)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (stored)
        settings.setValue(settingsKey(key), true);
    else
        settings.remove(settingsKey(key));
    settings.sync();
}

void AdminBiometrics::refresh()
{
    const QString key = m_transport->adminHostKey();
    update_available(!key.isEmpty() && m_hw->hasBiometrics());
    //Whatever the biometrics became, what is stored must stay removable.
    update_stored(isStored(key));
}

bool AdminBiometrics::canUnlockNow() const
{
    return m_updates && m_updates->get_busy().isEmpty() && m_updates->get_retryAfter() <= 0;
}

void AdminBiometrics::maybeAsk()
{
    if (!m_updates || m_asked || m_reading || !m_heldSecret.isEmpty() || !m_updates->isActive())
        return;
    if (m_relay->gate() != AdminRelay::GateLocked || !canUnlockNow())
        return;
    if (m_transport->hasPendingTlsTrust())
        return;
    const QString key = m_transport->adminHostKey();
    if (key.isEmpty() || !m_hw->hasBiometrics() || !isStored(key))
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

    if (result == HardwareUtils::BiometricInvalidated)
    {
        forgetBox(key);
        return;
    }
    if (result != HardwareUtils::BiometricOk || secret.isEmpty() || !m_updates || !m_updates->isActive())
        return;

    m_heldSecret = secret;
    m_heldKey = key;
    m_heldUntil = QDateTime::currentMSecsSinceEpoch() + m_holdMs;
    m_holdTimer->start(m_holdMs);
    trySend();
}

void AdminBiometrics::trySend()
{
    if (m_heldSecret.isEmpty())
        return;
    if (QDateTime::currentMSecsSinceEpoch() >= m_heldUntil || m_transport->hasPendingTlsTrust())
    {
        dropHeld();
        return;
    }

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
    if (!canUnlockNow())
        return;

    const QString secret = std::move(m_heldSecret);
    dropHeld();
    if (!m_updates)
        return;
    m_sendingStored = true;
    m_sentKey = key;
    m_updates->unlock(secret);
}

void AdminBiometrics::unlockAndKeep(const QString &password)
{
    if (!m_updates)
        return;
    const QString key = m_transport->adminHostKey();
    //refused below when busy: there would be no answer to end the wait
    if (canUnlockNow() && !key.isEmpty() && m_hw->hasBiometrics() && !isStored(key))
    {
        m_pendingSecret = password;
        m_pendingKey = key;
    }
    m_updates->unlock(password);
}

void AdminBiometrics::unlockAnswered(bool ok, const QString &code)
{
    if (m_sendingStored)
    {
        m_sendingStored = false;
        //the box does not take it any more: back to the field for good
        if (code == BadPassword)
            forgetBox(m_sentKey);
        return;
    }

    const QString secret = std::move(m_pendingSecret);
    const QString key = std::move(m_pendingKey);
    dropPending();
    //still the box it was typed for: the answer may come from a late socket
    if (!ok || secret.isEmpty() || key != m_transport->adminHostKey())
        return;

    m_storingKey = key;
    m_hw->biometricStore(key, secret, tr("Keep the admin password of this box"));
}

void AdminBiometrics::storeDone(const QString &key, int result)
{
    if (key != m_storingKey)
    {
        //what was erased meanwhile must not come back
        if (result == HardwareUtils::BiometricOk)
            m_hw->biometricRemove(key);
        return;
    }
    m_storingKey.clear();
    if (result == HardwareUtils::BiometricOk)
        setStored(key, true);
    refresh();
}

void AdminBiometrics::disable()
{
    const QString key = m_transport->adminHostKey();
    if (!key.isEmpty())
        forgetBox(key);
}

void AdminBiometrics::forgetBox(const QString &hostKey)
{
    if (m_heldKey == hostKey)
        dropHeld();
    if (m_pendingKey == hostKey)
        dropPending();
    if (m_readKey == hostKey)
    {
        m_reading = false;
        m_readKey.clear();
    }
    if (m_storingKey == hostKey)
        cancelStore();
    m_hw->biometricRemove(hostKey);
    setStored(hostKey, false);
    refresh();
}

void AdminBiometrics::forgetAll()
{
    dropHeld();
    dropPending();
    m_reading = false;
    m_readKey.clear();
    cancelStore();
    m_hw->biometricRemoveAll();
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove(SettingsGroup);
    settings.sync();
    refresh();
}

void AdminBiometrics::dropHeld()
{
    m_holdTimer->stop();
    m_heldSecret.clear();
    m_heldKey.clear();
}

void AdminBiometrics::dropPending()
{
    m_pendingSecret.clear();
    m_pendingKey.clear();
}
