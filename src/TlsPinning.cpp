#include "TlsPinning.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
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
    return hostKey(url.host(), url.port(defaultPort));
}

//One DER TLV header at pos. Lengths over 4 bytes cannot occur in a
//certificate we could handle and are refused.
static bool readTlv(const QByteArray &d, qsizetype pos, int &tag, qsizetype &hdr, qsizetype &len)
{
    if (pos < 0 || pos + 2 > d.size())
        return false;

    tag = uchar(d.at(pos));
    qsizetype l = uchar(d.at(pos + 1));
    hdr = 2;
    if (l & 0x80)
    {
        const int n = int(l & 0x7f);
        if (n == 0 || n > 4 || pos + 2 + n > d.size())
            return false;
        l = 0;
        for (int i = 0; i < n; i++)
            l = (l << 8) | uchar(d.at(pos + 2 + i));
        hdr += n;
    }
    if (l < 0 || pos + hdr + l > d.size())
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
    //What a self-signed or wrong-name certificate produces may be overridden
    //by a pin. A revoked or forged certificate, or no certificate at all, is
    //never acceptable, whatever the user once confirmed.
    switch (error)
    {
    case QSslError::CertificateSignatureFailed:
    case QSslError::UnableToDecryptCertificateSignature:
    case QSslError::CertificateRevoked:
    case QSslError::CertificateBlacklisted:
    case QSslError::NoPeerCertificate:
    case QSslError::NoSslSupport:
        return false;
    default:
        return true;
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
    if (!fromConfig.isNull())
        return fromConfig;
    for (const QSslError &e: errors)
    {
        if (!e.certificate().isNull())
            return e.certificate();
    }
    return QSslCertificate();
}

bool acceptIfPinned(QNetworkReply *reply, const QList<QSslError> &errors)
{
    if (!reply)
        return false;

    const QString key = hostKey(reply->url());
    const QString peer = spkiSha256(peerCertificate(reply->sslConfiguration().peerCertificate(), errors));
    if (decide(errors, TlsPinStore::instance()->pin(key), peer) == Decision::PinMatch)
    {
        reply->ignoreSslErrors();
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
