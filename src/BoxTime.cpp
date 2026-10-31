#include "BoxTime.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include <optional>

namespace BoxTime
{

static QString rfc3339Offset(int offsetSecs)
{
    const int minutes = qAbs(offsetSecs) / 60;
    return QStringLiteral("%1%2:%3")
            .arg(offsetSecs < 0 ? QLatin1Char('-') : QLatin1Char('+'))
            .arg(minutes / 60, 2, 10, QLatin1Char('0'))
            .arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

LocalTime fromLocal(int year, int month, int day, int hour, int minute, int second, const QString &zone)
{
    LocalTime res;

    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid() || !time.isValid())
        return res;

    const QTimeZone tz(zone.toUtf8());
    if (!tz.isValid())
    {
        res.status = LocalStatus::UnknownZone;
        return res;
    }

    //QDateTime's own gap and overlap resolution changed across Qt 6: only ask the
    //zone for offsets. A clock change sits between the offsets of the day before
    //and the day after, so one of the two is right for any valid wall clock.
    const QDateTime wall(date, time, QTimeZone::UTC);
    const int offsets[] = { tz.offsetFromUtc(wall.addDays(-1)), tz.offsetFromUtc(wall.addDays(1)) };

    std::optional<QDateTime> utc;
    int offset = 0;
    for (int candidateOffset: offsets)
    {
        const QDateTime candidate = wall.addSecs(-candidateOffset);
        if (tz.offsetFromUtc(candidate) == candidateOffset && (!utc || candidate < *utc))
        {
            utc = candidate;
            offset = candidateOffset;
        }
    }

    if (!utc)
    {
        res.status = LocalStatus::Nonexistent;
        return res;
    }

    if (year < FIRST_YEAR || year > LAST_YEAR ||
        utc->date().year() < FIRST_YEAR || utc->date().year() > LAST_YEAR)
    {
        res.status = LocalStatus::Range;
        return res;
    }

    res.status = LocalStatus::Ok;
    res.ms = utc->toMSecsSinceEpoch();
    res.rfc3339 = date.toString(Qt::ISODate) + QLatin1Char('T') + time.toString(Qt::ISODate) + rfc3339Offset(offset);
    return res;
}

static QString zoneKey(const QString &text)
{
    static const QRegularExpression ignored(QStringLiteral("[\\s_]+"));
    return text.toLower().remove(ignored);
}

QStringList filterZones(const QStringList &zones, const QString &query)
{
    const QString key = zoneKey(query);
    if (key.isEmpty())
        return zones;

    QStringList res;
    for (const QString &zone: zones)
    {
        if (zoneKey(zone).contains(key))
            res << zone;
    }
    return res;
}

}
