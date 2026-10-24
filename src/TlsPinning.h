#ifndef TLSPINNING_H
#define TLSPINNING_H

#include <QObject>
#include <QList>
#include <QMutex>
#include <QSslCertificate>
#include <QSslError>
#include <QUrl>
#include <QVariantMap>

class QNetworkReply;
class TlsPinStore;

//Trust on first use for the TLS certificate of a Calaos server.
//
//A certificate Qt validates (public CA, matching host name) is used as is and
//never pinned. Anything else (self-signed LAN box, Let's Encrypt certificate
//reached through its LAN or overlay IP) is accepted only when the SHA-256 of
//its public key matches the pin stored for that host:port; a first contact or
//a changed key needs an explicit confirmation from the user.
//
//The pin is the SubjectPublicKeyInfo, not the whole certificate: a renewal
//that keeps the key (self-signed regenerated with the same key, certbot
//--reuse-key) does not prompt again, a new key always does. The same bytes are
//hashed by the Android push picture worker (PublicKey.getEncoded()) and the
//iOS notification extension (its own DER walk), so one pin serves all three.
namespace TlsPinning
{

enum class Decision
{
    Trusted,        //Qt validation passed: no override, no pin involved
    PinMatch,       //untrusted for Qt, the key matches the stored pin
    AskFirstUse,    //untrusted, nothing stored for this host
    AskKeyChanged,  //untrusted, the key differs from the stored pin
    Refuse,         //an error no pin may override (revoked, bad signature...)
};

//"host:port" the pin is stored under: host in ACE form and lowercased, IPv6
//in brackets, the default port of the scheme made explicit (wss:// and
//https:// share 443). A host spelled another way gets another key: the
//question comes back, it never matches the wrong pin.
QString hostKey(const QUrl &url);
QString hostKey(const QString &host, int port);

//The SubjectPublicKeyInfo element of a DER certificate, empty when the bytes
//are not a certificate. Hand-parsed so every TLS backend (OpenSSL, Secure
//Transport) hashes exactly the same bytes.
QByteArray subjectPublicKeyInfo(const QByteArray &certDer);

//Lowercase hex SHA-256, empty for a null certificate or an unparsable one.
QString spkiSha256(const QSslCertificate &cert);
QString certSha256(const QSslCertificate &cert);

//"AB:CD:..." for display.
QString formatFingerprint(const QString &hex);

bool isOverridable(QSslError::SslError error);

//RSA under 2048 bits: refused by OpenSSL's default security level, the fix
//is a new certificate on the box rather than a pin.
bool isWeakRsaKey(const QSslCertificate &cert);

Decision decide(const QList<QSslError> &errors, const QString &storedPin, const QString &peerPin);

//Leaf certificate of the peer (the connection's peer certificate); null
//when the backend did not report it, which refuses the handshake.
QSslCertificate peerCertificate(const QSslCertificate &fromConfig, const QList<QSslError> &errors);

//A host given by DNS name (not an IP, not .local): a Calaos box reached that
//way normally has a valid certificate, so an untrusted one is suspicious.
bool isDnsName(const QString &host);

//For the paths that cannot ask the user (QML images, desktop daemon client):
//ignores exactly these errors on a pin match, returns whether it did.
//store: default TlsPinStore::instance().
bool acceptIfPinned(QNetworkReply *reply, const QList<QSslError> &errors, TlsPinStore *store = nullptr);

//For QNetworkAccessManager::encrypted, which comes before any request data:
//a valid certificate raises no sslErrors, so a pinned host whose key changed
//behind a CA-valid certificate is caught here. Aborts the reply on a
//mismatch, returns whether the key is acceptable.
bool keyMatchesPin(QNetworkReply *reply, TlsPinStore *store = nullptr);

}

//The pins, persisted in QSettings ("tls/pins": host:port -> SPKI hex).
//Thread-safe: QML image loads check pins from their own thread.
class TlsPinStore: public QObject
{
    Q_OBJECT
public:
    //The application store (default QSettings of the app).
    static TlsPinStore *instance();

    //Tests: an INI file of their own; empty = the default QSettings.
    explicit TlsPinStore(const QString &iniFile = QString(), QObject *parent = nullptr);

    QString pin(const QString &hostKey) const;
    void setPin(const QString &hostKey, const QString &spkiHex);
    bool forget(const QString &hostKey);
    void clear();
    //host:port -> SPKI hex
    QVariantMap pins() const;

signals:
    void pinsChanged();

private:
    QVariantMap load() const;
    void save(const QVariantMap &pins);

    QString iniFile;
    mutable QMutex mutex;
};

#endif // TLSPINNING_H
