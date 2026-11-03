#ifndef LOGREDACT_H
#define LOGREDACT_H

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include "JsonKeys.h"

//Everything printed to a log that may carry a credential goes through here.
//Header only so that small units (NetworkRequest, WeatherInfo) can use it
//without linking CalaosConnection.
namespace LogRedact {

//Query parameters and JSON keys that carry credentials and must never be logged
inline const QStringList &credentialKeys()
{
    //"u" and "p" are not JSON keys but the short query parameters of the
    //legacy v1 camera url, so they stay literals here (see JsonKeys.h).
    //JsonKeys::Token is the push token sent by register_push: a per-device
    //credential as well (FIX-3), and JsonKeys::PushKey the key admin codes
    //are sealed with. The admin keys belong to the admin relay.
    static const QStringList keys = { JsonKeys::CnUser,
                                      JsonKeys::CnPass,
                                      JsonKeys::Token,
                                      JsonKeys::CameraToken,
                                      JsonKeys::PushKey,
                                      QStringLiteral("u"),
                                      QStringLiteral("p"),
                                      QStringLiteral("APPID"),
                                      QStringLiteral("admin_pass"),
                                      QStringLiteral("admin_session"),
                                      QStringLiteral("session"),
                                      QStringLiteral("password"),
                                      QStringLiteral("new_pass"),
                                      JsonKeys::OldPw,
                                      JsonKeys::NewPw };
    return keys;
}

//Masks the credential pairs of a query string. changed tells whether any was found.
inline QUrlQuery maskQuery(const QUrlQuery &query, bool &changed)
{
    QUrlQuery redacted;
    for (const auto &item: query.queryItems(QUrl::FullyEncoded))
    {
        if (credentialKeys().contains(item.first))
        {
            redacted.addQueryItem(item.first, QStringLiteral("***"));
            changed = true;
        }
        else
        {
            redacted.addQueryItem(item.first, item.second);
        }
    }
    return redacted;
}

inline QString redactUrl(const QUrl &url)
{
    //A server-supplied suffix can push "&u=..&p=.." into the fragment.
    bool changed = false;
    QUrl u(url);
    if (url.hasQuery())
        u.setQuery(maskQuery(QUrlQuery(url), changed));
    if (url.hasFragment())
    {
        bool fragChanged = false;
        const QUrlQuery frag = maskQuery(QUrlQuery(url.fragment(QUrl::FullyEncoded)), fragChanged);
        if (fragChanged)
        {
            u.setFragment(frag.toString(QUrl::FullyEncoded), QUrl::StrictMode);
            changed = true;
        }
    }
    return changed ? u.toString() : url.toString();
}

inline QJsonValue redactValue(const QJsonValue &v)
{
    if (v.isObject())
    {
        QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it)
            it.value() = credentialKeys().contains(it.key()) ? QJsonValue(QStringLiteral("***"))
                                                              : redactValue(it.value());
        return o;
    }
    if (v.isArray())
    {
        QJsonArray a = v.toArray();
        for (auto it = a.begin(); it != a.end(); ++it)
            *it = redactValue(*it);
        return a;
    }
    return v;
}

inline QByteArray redactJson(const QJsonObject &obj)
{
    return QJsonDocument(redactValue(obj).toObject()).toJson();
}

inline QByteArray redactJson(const QJsonDocument &doc)
{
    if (doc.isObject())
        return redactJson(doc.object());
    return QJsonDocument(redactValue(doc.array()).toArray()).toJson();
}

//A response body is only ever printed as redacted JSON; anything else is
//reduced to its size, since free text cannot be masked reliably.
inline QString redactBody(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isNull())
        return QStringLiteral("<%1 bytes, not JSON>").arg(body.size());
    return QString::fromUtf8(redactJson(doc));
}

inline QString redactError(QString err, const QUrl &url)
{
    //Qt copies the request url, credentials included, into its error texts.
    const QString safe = redactUrl(url);
    for (const QString &raw: { url.toString(), url.toString(QUrl::FullyEncoded) })
    {
        if (!raw.isEmpty() && err.contains(raw))
            err.replace(raw, safe);
    }

    //Always, even once the url is replaced: the text may repeat the pairs
    //elsewhere. Only "key=value" pairs, a short value (p=1) would otherwise
    //eat pieces of the host or the path.
    QList<QUrlQuery> queries = { QUrlQuery(url), QUrlQuery(url.fragment(QUrl::FullyEncoded)) };
    for (const QUrlQuery &query: queries)
    {
        for (const QString &key: credentialKeys())
        {
            for (auto fmt: { QUrl::FullyEncoded, QUrl::PrettyDecoded, QUrl::FullyDecoded })
            {
                for (const QString &value: query.allQueryItemValues(key, fmt))
                {
                    if (!value.isEmpty())
                        err.replace(key + QLatin1Char('=') + value, key + QStringLiteral("=***"));
                }
            }
        }
    }
    return err;
}

}

#endif
