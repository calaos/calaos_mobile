#ifndef CAMERATOKEN_H
#define CAMERATOKEN_H

#include <chrono>
#include <functional>
#include <QString>
#include <algorithm>

//The short-lived token that replaces cn_user/cn_pass on camera/get_picture
//(calaos_base docs/08_http_api.md, "jeton de capture"). Only the lifetime and
//retry rules live here, the messages are CalaosConnection's. Same bounds as
//the webapp store (calaos-web-app stores/cameraToken.ts).
class CameraToken
{
public:
    static constexpr int RenewMarginS = 30;
    static constexpr int MinLifetimeS = 1;
    static constexpr int MaxLifetimeS = 3600;
    static constexpr int RetryMinMs = 1000;
    static constexpr int RetryMaxMs = 30000;

    //Milliseconds on a monotonic clock; tests replace it to move time at will.
    using Clock = std::function<qint64()>;
    void setClock(Clock clock) { m_clock = std::move(clock); }

    //Empty once expired: a caller must then send nothing.
    QString current() const
    {
        return (m_token.isEmpty() || m_expiry <= now()) ? QString() : m_token;
    }

    //The token is valid but close enough to its end that the next frame
    //should ask for the next one. Lazy on purpose: no timer runs while no
    //camera is on screen.
    bool renewDue() const
    {
        return !current().isEmpty() && m_expiry - now() <= m_marginMs;
    }

    //A server-given lifetime is clamped: a hostile or buggy value must not
    //overflow a timer nor make the client renew at round-trip speed.
    void store(const QString &token, qint64 expiresInS)
    {
        const qint64 life = std::clamp<qint64>(expiresInS, MinLifetimeS, MaxLifetimeS) * 1000;
        m_token = token;
        m_expiry = now() + life;
        //short lifetimes still renew halfway, never right on top of expiry
        m_marginMs = std::min<qint64>(RenewMarginS * 1000, life / 2);
        m_retryAt = 0;
    }

    //A picture was accepted with a token: the server works again. A token
    //merely issued proves nothing, a server that refuses every picture
    //issues one per request.
    void accepted(const QString &used)
    {
        if (!used.isEmpty())
            m_retryMs = RetryMinMs;
    }

    //No token came back: no new request before the backoff is over.
    void failed()
    {
        m_retryAt = now() + m_retryMs;
        m_retryMs = std::min(m_retryMs * 2, int(RetryMaxMs));
    }

    bool inBackoff() const { return m_retryAt > now(); }

    //The server refused `used` (HTTP 400). A late answer about a token already
    //replaced must not erase the new one; otherwise forget it and back off, or
    //a server that keeps refusing gets a token request per frame.
    void rejected(const QString &used)
    {
        if (used.isEmpty() || used != m_token)
            return;
        m_token.clear();
        m_expiry = 0;
        m_marginMs = 0;
        failed();
    }

    //The server dropped it (socket closed, credentials changed): forget all.
    void clear()
    {
        m_token.clear();
        m_expiry = 0;
        m_marginMs = 0;
        m_retryMs = RetryMinMs;
        m_retryAt = 0;
    }

private:
    qint64 now() const
    {
        return m_clock ? m_clock() : std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    Clock m_clock;
    QString m_token;
    qint64 m_expiry = 0;
    qint64 m_marginMs = 0;
    int m_retryMs = RetryMinMs;
    qint64 m_retryAt = 0;
};

#endif // CAMERATOKEN_H
