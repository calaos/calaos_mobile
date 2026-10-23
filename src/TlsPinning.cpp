#include "TlsPinning.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QHostAddress>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QSettings>

static const char *const PinsKey = "tls/pins";

namespace TlsPinning
{

QString hostKey(const QString &host, int port)
{
    QString h = host.toLower();
    if (h.contains(QLatin1Char(':')) && !h.startsWith(QLatin1Char('[')))
        h = QStringLiteral("[%1]").arg(h);
    return QStringLiteral("%1:%2").arg(h).arg(port);
}

QString hostKey(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    const int defaultPort = (scheme == QLatin1String("http") || scheme == QLatin1String("ws")) ? 80 : 443;
    //ACE form: an internationalised name is keyed the same way by the
    //platform code (Java IDN.toASCII, NSURLProtectionSpace).
    return hostKey(url.host(QUrl::FullyEncoded), url.port(defaultPort));
}

//One DER TLV header at pos. A certificate never needs more than 3 length
//bytes; the bounds are written so that nothing can overflow.
static bool readTlv(const QByteArray &d, qsizetype pos, int &tag, qsizetype &hdr, qsizetype &len)
{
    if (pos < 0 || pos >= d.size() || d.size() - pos < 2)
        return false;

    tag = uchar(d.at(pos));
    qsizetype l = uchar(d.at(pos + 1));
    hdr = 2;
    if (l & 0x80)
    {
        const int n = int(l & 0x7f);
        if (n == 0 || n > 3 || d.size() - pos - 2 < n)
            return false;
        l = 0;
        for (int i = 0; i < n; i++)
            l = (l << 8) | uchar(d.at(pos + 2 + i));
        hdr += n;
    }
    if (l > d.size() - pos - hdr)
        return false;
    len = l;
    return true;
}

QByteArray subjectPublicKeyInfo(const QByteArray &der)
{
    int tag;
    qsizetype hdr, len;

    //Certificate ::= SEQUENCE { tbsCertificate SEQUENCE { ... } ... }
    if (!readTlv(der, 0, tag, hdr, len) || tag != 0x30)
        return QByteArray();
    qsizetype pos = hdr;
    if (!readTlv(der, pos, tag, hdr, len) || tag != 0x30)
        return QByteArray();
    pos += hdr;
    const qsizetype tbsEnd = pos + len;

    //[0] version is optional (absent in v1 certificates)
    if (!readTlv(der, pos, tag, hdr, len))
        return QByteArray();
    if (tag == 0xa0)
    {
        pos += hdr + len;
        if (!readTlv(der, pos, tag, hdr, len))
            return QByteArray();
    }

    //serialNumber, signature, issuer, validity, subject
    static const int skipped[] = { 0x02, 0x30, 0x30, 0x30, 0x30 };
    for (int expected: skipped)
    {
        if (!readTlv(der, pos, tag, hdr, len) || tag != expected)
            return QByteArray();
        pos += hdr + len;
    }

    if (!readTlv(der, pos, tag, hdr, len) || tag != 0x30 || pos + hdr + len > tbsEnd)
        return QByteArray();
    return der.mid(pos, hdr + len);
}

QString spkiSha256(const QSslCertificate &cert)
{
    if (cert.isNull())
        return QString();
    const QByteArray spki = subjectPublicKeyInfo(cert.toDer());
    if (spki.isEmpty())
        return QString();
    return QString::fromLatin1(QCryptographicHash::hash(spki, QCryptographicHash::Sha256).toHex());
}

QString certSha256(const QSslCertificate &cert)
{
    if (cert.isNull())
        return QString();
    return QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex());
}

QString formatFingerprint(const QString &hex)
{
    QStringList pairs;
    for (qsizetype i = 0; i + 1 < hex.size(); i += 2)
        pairs << hex.mid(i, 2).toUpper();
    return pairs.join(QLatin1Char(':'));
}

bool isOverridable(QSslError::SslError error)
{
    //Allow-list: what a self-signed box, or a certificate reached through
    //another name (LAN IP, overlay IP), produces. Anything else (revoked,
    //forged signature, no certificate, unknown) is never acceptable,
    //whatever the user once confirmed. Expiry is overridable: the pin is on
    //the key, and many boxes run an old self-signed certificate.
    switch (error)
    {
    case QSslError::SelfSignedCertificate:
    case QSslError::SelfSignedCertificateInChain:
    case QSslError::UnableToGetIssuerCertificate:
    case QSslError::UnableToGetLocalIssuerCertificate:
    case QSslError::UnableToVerifyFirstCertificate:
    case QSslError::CertificateUntrusted:
    case QSslError::InvalidCaCertificate:
    case QSslError::InvalidPurpose:
    case QSslError::HostNameMismatch:
    case QSslError::CertificateExpired:
    case QSslError::CertificateNotYetValid:
        return true;
    default:
        return false;
    }
}

Decision decide(const QList<QSslError> &errors, const QString &storedPin, const QString &peerPin)
{
    if (errors.isEmpty())
        return Decision::Trusted;

    for (const QSslError &e: errors)
    {
        if (!isOverridable(e.error()))
            return Decision::Refuse;
    }

    if (peerPin.isEmpty())
        return Decision::Refuse;
    if (storedPin.isEmpty())
        return Decision::AskFirstUse;
    if (storedPin.compare(peerPin, Qt::CaseInsensitive) == 0)
        return Decision::PinMatch;
    return Decision::AskKeyChanged;
}

QSslCertificate peerCertificate(const QSslCertificate &fromConfig, const QList<QSslError> &errors)
{
    //Only the leaf: an error may carry an intermediate of the chain, whose
    //key must never be taken for the server's.
    Q_UNUSED(errors);
    return fromConfig;
}

bool isDnsName(const QString &host)
{
    QString h = host.toLower();
    if (h.startsWith(QLatin1Char('[')))
        return false;
    if (!QHostAddress(h).isNull())
        return false;
    if (h.endsWith(QLatin1String(".local")))
        return false;
    return h.contains(QLatin1Char('.'));
}

bool acceptIfPinned(QNetworkReply *reply, const QList<QSslError> &errors, TlsPinStore *store)
{
    if (!reply)
        return false;

    const QString key = hostKey(reply->url());
    const QString peer = spkiSha256(peerCertificate(reply->sslConfiguration().peerCertificate(), errors));
    if (!store)
        store = TlsPinStore::instance();
    if (decide(errors, store->pin(key), peer) == Decision::PinMatch)
    {
        //These errors only, never all: QNAM keeps the flag on its cached
        //channel, and a blanket ignore would let a later handshake with
        //another certificate through.
        reply->ignoreSslErrors(errors);
        return true;
    }

    qWarning() << "TLS: untrusted certificate for" << key << "SPKI sha256" << peer << "- request refused";
    return false;
}

}

TlsPinStore *TlsPinStore::instance()
{
    static TlsPinStore *store = new TlsPinStore(QString(), QCoreApplication::instance());
    return store;
}

TlsPinStore::TlsPinStore(const QString &file, QObject *parent):
    QObject(parent),
    iniFile(file)
{
}

QVariantMap TlsPinStore::load() const
{
    if (!iniFile.isEmpty())
        return QSettings(iniFile, QSettings::IniFormat).value(PinsKey).toMap();

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return settings.value(PinsKey).toMap();
}

void TlsPinStore::save(const QVariantMap &pins)
{
    auto write = [&pins](QSettings &settings)
    {
        if (pins.isEmpty())
            settings.remove(PinsKey);
        else
            settings.setValue(PinsKey, pins);
        settings.sync();
    };

    if (!iniFile.isEmpty())
    {
        QSettings settings(iniFile, QSettings::IniFormat);
        write(settings);
        return;
    }

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    write(settings);
}

QString TlsPinStore::pin(const QString &hostKey) const
{
    QMutexLocker lock(&mutex);
    return load().value(hostKey).toString();
}

void TlsPinStore::setPin(const QString &hostKey, const QString &spkiHex)
{
    {
        QMutexLocker lock(&mutex);
        QVariantMap pins = load();
        const QString pin = spkiHex.toLower();
        if (pins.value(hostKey).toString() == pin)
            return;
        pins[hostKey] = pin;
        save(pins);
    }
    emit pinsChanged();
}

bool TlsPinStore::forget(const QString &hostKey)
{
    {
        QMutexLocker lock(&mutex);
        QVariantMap pins = load();
        if (pins.remove(hostKey) == 0)
            return false;
        save(pins);
    }
    emit pinsChanged();
    return true;
}

void TlsPinStore::clear()
{
    {
        QMutexLocker lock(&mutex);
        if (load().isEmpty())
            return;
        save(QVariantMap());
    }
    emit pinsChanged();
}

QVariantMap TlsPinStore::pins() const
{
    QMutexLocker lock(&mutex);
    return load();
}
