#ifndef BOXTIME_H
#define BOXTIME_H

#include <QString>
#include <QStringList>

//Wall-clock and time zone rules of the box time page, free of any UI
namespace BoxTime
{

constexpr int FIRST_YEAR = 2020;
constexpr int LAST_YEAR = 2100;

enum class LocalStatus
{
    Ok,
    Invalid,     //no such date or time, the 31st of February
    Range,       //the daemon only takes 2020..2100, in UTC as well
    Nonexistent, //skipped by the spring clock change in this zone
    UnknownZone, //the zone database of this machine does not have it
};

struct LocalTime
{
    LocalStatus status = LocalStatus::Invalid;
    qint64 ms = 0;   //since the epoch
    QString rfc3339; //with the offset of the zone at that instant
};

//An hour repeated by the autumn clock change is its first occurrence.
LocalTime fromLocal(int year, int month, int day, int hour, int minute, int second, const QString &zone);

//Ignores case, spaces and underscores: "new york" finds America/New_York
QStringList filterZones(const QStringList &zones, const QString &query);

}

#endif // BOXTIME_H
