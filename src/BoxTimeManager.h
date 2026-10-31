#ifndef BOXTIMEMANAGER_H
#define BOXTIMEMANAGER_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include "qqmlhelpers.h"

class CalaosOsAPI;
struct CalaosOsReply;

//State of the box clock for the kiosk page, and every write to it. Nothing the
//box did not answer is ever shown: when a read fails the values are cleared.
class BoxTimeManager: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString zoneFilter READ zoneFilter WRITE setZoneFilter NOTIFY zoneFilterChanged)

    //a read answered: the values below are the box's
    QML_READONLY_PROPERTY(bool, loaded)
    //the daemon has no time routes
    QML_READONLY_PROPERTY(bool, updateRequired)
    //why the box could not be read, empty when it could
    QML_READONLY_PROPERTY(QString, loadError)
    //a write or a restart runs: no other one may start
    QML_READONLY_PROPERTY(bool, busy)

    QML_READONLY_PROPERTY(QString, dateText)
    QML_READONLY_PROPERTY(QString, timeText)
    QML_READONLY_PROPERTY(QString, timezone)
    QML_READONLY_PROPERTY(bool, ntp)
    //"manual", "synchronized", "notSynchronized", or empty when not loaded
    QML_READONLY_PROPERTY(QString, ntpState)

    QML_READONLY_PROPERTY(bool, restartRequired)
    QML_READONLY_PROPERTY(bool, serverRestartDone)

    //outcome of the last write, for the user
    QML_READONLY_PROPERTY(QString, message)
    QML_READONLY_PROPERTY(bool, messageIsError)
    //the box did not say how the last write ended: offer to read it again
    QML_READONLY_PROPERTY(bool, unconfirmed)

    //the list after zoneFilter, with the zone of the box even if the list lacks it
    QML_READONLY_PROPERTY(QStringList, zones)
    QML_READONLY_PROPERTY(bool, zonesLoading)
    QML_READONLY_PROPERTY(QString, zonesError)

public:
    BoxTimeManager(CalaosOsAPI *api, QObject *parent = nullptr);

    QString zoneFilter() const { return m_zoneFilter; }
    void setZoneFilter(const QString &filter);

    //language of the long date; empty: the one of the system
    void setLanguage(const QString &code);

    //when the page opens and closes: the clock only ticks in between
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadZones();

    Q_INVOKABLE void setTimezone(const QString &zone);
    Q_INVOKABLE void setNtp(bool enabled);

    //box time in its own zone: year, month, day, hour, minute, second
    Q_INVOKABLE QVariantMap localNow() const;

    //Judges a wall clock in the zone of the box, sends nothing. The result has
    //"ok" and "problem" (text), or "time" for applyManualTime, "needsConfirm"
    //and "confirmText": the clock moves over an hour, or NTP will be turned off.
    Q_INVOKABLE QVariantMap checkManualTime(int year, int month, int day, int hour, int minute, int second) const;
    Q_INVOKABLE void applyManualTime(const QString &time);

    Q_INVOKABLE void restartServer();
    Q_INVOKABLE void dismissRestart();

signals:
    void zoneFilterChanged(const QString &zoneFilter);

private:
    void read();
    void readFailed(const CalaosOsReply &reply);
    bool applyState(const QJsonObject &state);
    void clearState();

    void write(const QJsonObject &change);
    void writeAnswered(const QJsonObject &change, const QString &zoneBefore, const CalaosOsReply &reply);
    QString partialText(const QJsonObject &data) const;
    QString refusedText(const QJsonObject &change) const;

    void updateClock();
    void updateZones();
    void setMessage(const QString &text, bool isError);
    qint64 boxNowMs() const;
    QDateTime boxNow() const;
    QString clockZone() const;

    CalaosOsAPI *m_api;
    QTimer m_ticker;

    qint64 m_readAtMs = 0;       //the "now" of the last read
    QElapsedTimer m_sinceRead;   //monotonic: the wall clock is what we may be changing
    QLocale m_locale;

    //only the latest read counts: an older one may predate a write
    quint64 m_readId = 0;
    bool m_writing = false;
    QStringList m_allZones;
    QString m_zoneFilter;

    //zone sent without a clear answer: a later read tells if it landed
    QString m_unconfirmedZone;
    QString m_zoneBeforeWrite;
};

#endif // BOXTIMEMANAGER_H
