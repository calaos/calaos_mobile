#include <QtTest>
#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimeZone>
#include <utility>
#include "BoxTime.h"
#include "BoxTimeManager.h"
#include "CalaosOsAPI.h"
#include "FakeTimeDaemon.h"

using namespace BoxTime;

Q_DECLARE_METATYPE(BoxTime::LocalStatus)

namespace {

const QString TIME = QStringLiteral("/api/v2/system/time");
const QString ZONES = QStringLiteral("/api/v2/system/timezones");
const QString RESTART = QStringLiteral("/api/v2/system/services/restart");
const QByteArray TOKEN(64, 'k');

QStringList s_logLines;
QtMessageHandler s_previousHandler = nullptr;

void captureMessages(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    s_logLines << msg;
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}

} //namespace

class TstBoxTime: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void fromLocalOffset_data();
    void fromLocalOffset();
    void fromLocalRefused_data();
    void fromLocalRefused();
    void filter_data();
    void filter();

    void routeBodies_data();
    void routeBodies();
    void timezonesRoute();

    void ntpStates_data();
    void ntpStates();
    void clockInTheBoxZone();
    void clockOfAnUnknownZoneSaysUtc();
    void readsAtEachOpening();
    void aReadThatFailsLeavesNoValue_data();
    void aReadThatFailsLeavesNoValue();

    void writeIsFollowedByARead();
    void shownValuesComeFromTheRead();
    void busyUntilTheReadAnswers();
    void noWriteBeforeARead();
    void aReadOlderThanAWriteIsIgnored();

    void manualTimeBody_data();
    void manualTimeBody();
    void manualTimeOffset_data();
    void manualTimeOffset();
    void manualConfirmation_data();
    void manualConfirmation();
    void manualProblems_data();
    void manualProblems();

    void routesMissing_data();
    void routesMissing();
    void routesMissingOnWrite();
    void refused_data();
    void refused();
    void upgradeInProgress();
    void partialFailure_data();
    void partialFailure();
    void boxUnreachable();
    void noAnswer_data();
    void noAnswer();

    void restartRequired();
    void noRestartWithoutAZoneChange();
    void restartServer();
    void restartServerRefused_data();
    void restartServerRefused();

    void zoneList();
    void currentZoneMissingFromTheList();
    void zoneListFailsThenRetries();

private:
    void readBox();
    void waitIdle();

    QTemporaryDir m_dir;
    QString m_tokenPath;
    FakeTimeDaemon *m_daemon = nullptr;
    CalaosOsAPI *m_api = nullptr;
    BoxTimeManager *m_mgr = nullptr;
};

void TstBoxTime::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_tokenPath = m_dir.filePath(QStringLiteral("calaos-ct.token"));
    QFile f(m_tokenPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(TOKEN + "\n");
    f.close();
    s_previousHandler = qInstallMessageHandler(captureMessages);
}

void TstBoxTime::init()
{
    s_logLines.clear();
    m_daemon = new FakeTimeDaemon;
    m_daemon->token = QString::fromLatin1(TOKEN);
    QVERIFY(m_daemon->start());
    m_api = new CalaosOsAPI;
    m_api->setBaseAddr(m_daemon->baseAddr());
    m_api->setTokenFile(m_tokenPath);
    m_mgr = new BoxTimeManager(m_api);
    m_mgr->setLanguage(QStringLiteral("en"));
}

void TstBoxTime::cleanup()
{
    delete m_mgr;
    delete m_api;
    delete m_daemon;
    m_mgr = nullptr;
    m_api = nullptr;
    m_daemon = nullptr;

    for (const QString &line: std::as_const(s_logLines))
        QVERIFY2(!line.contains(QString::fromLatin1(TOKEN)), "the token reached the logs");
}

void TstBoxTime::readBox()
{
    m_mgr->activate();
    QTRY_VERIFY(m_mgr->get_loaded() || m_mgr->get_updateRequired() || !m_mgr->get_loadError().isEmpty());
}

//the write and the read that follows it have both answered
void TstBoxTime::waitIdle()
{
    QTRY_VERIFY(!m_mgr->get_busy());
}

void TstBoxTime::fromLocalOffset_data()
{
    QTest::addColumn<QString>("zone");
    QTest::addColumn<QList<int>>("local");
    QTest::addColumn<QString>("expected");

    const QString paris = QStringLiteral("Europe/Paris");
    const QString ny = QStringLiteral("America/New_York");
    QTest::newRow("paris winter") << paris << QList<int>{ 2026, 1, 15, 8, 5, 9 } << "2026-01-15T08:05:09+01:00";
    QTest::newRow("paris summer") << paris << QList<int>{ 2026, 7, 15, 8, 5, 9 } << "2026-07-15T08:05:09+02:00";
    QTest::newRow("paris before the gap") << paris << QList<int>{ 2026, 3, 29, 1, 59, 59 } << "2026-03-29T01:59:59+01:00";
    QTest::newRow("paris after the gap") << paris << QList<int>{ 2026, 3, 29, 3, 0, 0 } << "2026-03-29T03:00:00+02:00";
    QTest::newRow("paris repeated hour") << paris << QList<int>{ 2026, 10, 25, 2, 30, 0 } << "2026-10-25T02:30:00+02:00";
    QTest::newRow("paris after the repeated hour") << paris << QList<int>{ 2026, 10, 25, 3, 0, 0 } << "2026-10-25T03:00:00+01:00";
    QTest::newRow("new york winter") << ny << QList<int>{ 2026, 1, 15, 8, 0, 0 } << "2026-01-15T08:00:00-05:00";
    QTest::newRow("new york summer") << ny << QList<int>{ 2026, 7, 15, 8, 0, 0 } << "2026-07-15T08:00:00-04:00";
    QTest::newRow("half hour east") << "Asia/Kolkata" << QList<int>{ 2026, 7, 15, 8, 0, 0 } << "2026-07-15T08:00:00+05:30";
    QTest::newRow("half hour west") << "America/St_Johns" << QList<int>{ 2026, 1, 15, 8, 0, 0 } << "2026-01-15T08:00:00-03:30";
    QTest::newRow("utc") << "UTC" << QList<int>{ 2026, 1, 15, 8, 0, 0 } << "2026-01-15T08:00:00+00:00";
    QTest::newRow("first second") << "UTC" << QList<int>{ 2020, 1, 1, 0, 0, 0 } << "2020-01-01T00:00:00+00:00";
    QTest::newRow("last second") << "UTC" << QList<int>{ 2100, 12, 31, 23, 59, 59 } << "2100-12-31T23:59:59+00:00";
}

void TstBoxTime::fromLocalOffset()
{
    QFETCH(QString, zone);
    QFETCH(QList<int>, local);
    QFETCH(QString, expected);

    const LocalTime res = fromLocal(local[0], local[1], local[2], local[3], local[4], local[5], zone);
    QCOMPARE(res.status, LocalStatus::Ok);
    QCOMPARE(res.rfc3339, expected);

    //what the daemon will parse is the instant the page compares to the box clock
    QCOMPARE(QDateTime::fromString(res.rfc3339, Qt::ISODate).toMSecsSinceEpoch(), res.ms);
}

void TstBoxTime::fromLocalRefused_data()
{
    QTest::addColumn<QString>("zone");
    QTest::addColumn<QList<int>>("local");
    QTest::addColumn<LocalStatus>("expected");

    const QString paris = QStringLiteral("Europe/Paris");
    QTest::newRow("skipped hour") << paris << QList<int>{ 2026, 3, 29, 2, 30, 0 } << LocalStatus::Nonexistent;
    QTest::newRow("first skipped second") << paris << QList<int>{ 2026, 3, 29, 2, 0, 0 } << LocalStatus::Nonexistent;
    QTest::newRow("last skipped second") << paris << QList<int>{ 2026, 3, 29, 2, 59, 59 } << LocalStatus::Nonexistent;
    QTest::newRow("skipped hour new york") << "America/New_York" << QList<int>{ 2026, 3, 8, 2, 30, 0 } << LocalStatus::Nonexistent;
    QTest::newRow("31 february") << paris << QList<int>{ 2026, 2, 31, 12, 0, 0 } << LocalStatus::Invalid;
    QTest::newRow("29 february") << paris << QList<int>{ 2026, 2, 29, 12, 0, 0 } << LocalStatus::Invalid;
    QTest::newRow("hour 24") << paris << QList<int>{ 2026, 1, 1, 24, 0, 0 } << LocalStatus::Invalid;
    QTest::newRow("minute 60") << paris << QList<int>{ 2026, 1, 1, 12, 60, 0 } << LocalStatus::Invalid;
    QTest::newRow("before 2020") << "UTC" << QList<int>{ 2019, 12, 31, 23, 59, 59 } << LocalStatus::Range;
    QTest::newRow("after 2100") << "UTC" << QList<int>{ 2101, 1, 1, 0, 0, 0 } << LocalStatus::Range;
    //the daemon checks the year in UTC
    QTest::newRow("2020 here, 2019 in utc") << paris << QList<int>{ 2020, 1, 1, 0, 30, 0 } << LocalStatus::Range;
    QTest::newRow("2100 here, 2101 in utc") << "America/New_York" << QList<int>{ 2100, 12, 31, 23, 30, 0 } << LocalStatus::Range;
    QTest::newRow("unknown zone") << "Mars/Olympus" << QList<int>{ 2026, 1, 1, 12, 0, 0 } << LocalStatus::UnknownZone;
}

void TstBoxTime::fromLocalRefused()
{
    QFETCH(QString, zone);
    QFETCH(QList<int>, local);
    QFETCH(LocalStatus, expected);

    const LocalTime res = fromLocal(local[0], local[1], local[2], local[3], local[4], local[5], zone);
    QCOMPARE(res.status, expected);
    QVERIFY(res.rfc3339.isEmpty());
}

void TstBoxTime::filter_data()
{
    QTest::addColumn<QString>("query");
    QTest::addColumn<QStringList>("expected");

    const QStringList ny{ "America/New_York" };
    QTest::newRow("empty") << "" << QStringList{ "America/New_York", "Asia/Tokyo", "Europe/Paris" };
    QTest::newRow("only spaces") << "  " << QStringList{ "America/New_York", "Asia/Tokyo", "Europe/Paris" };
    QTest::newRow("space for underscore") << "new york" << ny;
    QTest::newRow("underscore") << "new_york" << ny;
    QTest::newRow("no separator") << "newyork" << ny;
    QTest::newRow("case") << "NEW YORK" << ny;
    QTest::newRow("region") << "asia/" << QStringList{ "Asia/Tokyo" };
    QTest::newRow("several") << "o" << QStringList{ "America/New_York", "Asia/Tokyo", "Europe/Paris" };
    QTest::newRow("nothing") << "atlantis" << QStringList{};
}

void TstBoxTime::filter()
{
    QFETCH(QString, query);
    QFETCH(QStringList, expected);

    QCOMPARE(filterZones({ "America/New_York", "Asia/Tokyo", "Europe/Paris" }, query), expected);
}

void TstBoxTime::routeBodies_data()
{
    QTest::addColumn<QJsonObject>("change");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("timezone") << QJsonObject{{ "timezone", "Asia/Tokyo" }} << QByteArray(R"({"timezone":"Asia/Tokyo"})");
    QTest::newRow("ntp on") << QJsonObject{{ "ntp", true }} << QByteArray(R"({"ntp":true})");
    QTest::newRow("ntp off") << QJsonObject{{ "ntp", false }} << QByteArray(R"({"ntp":false})");
    QTest::newRow("time and ntp off")
        << QJsonObject{{ "ntp", false }, { "time", "2026-07-15T08:00:00+02:00" }}
        << QByteArray(R"({"ntp":false,"time":"2026-07-15T08:00:00+02:00"})");
}

void TstBoxTime::routeBodies()
{
    QFETCH(QJsonObject, change);
    QFETCH(QByteArray, expected);

    bool answered = false;
    CalaosOsReply got;
    m_api->setBoxTime(change, [&](const CalaosOsReply &reply) { got = reply; answered = true; });
    QTRY_VERIFY(answered);

    QVERIFY(got.ok);
    const auto posts = m_daemon->requestsTo("POST", TIME);
    QCOMPARE(posts.size(), 1);
    QCOMPARE(posts.first().body, expected);
    QCOMPARE(posts.first().authorization, "Bearer " + TOKEN);
    QVERIFY(posts.first().query.isEmpty());
}

void TstBoxTime::timezonesRoute()
{
    bool answered = false;
    QJsonValue data;
    m_api->getTimezones([&](const CalaosOsReply &reply) { data = reply.data; answered = true; });
    QTRY_VERIFY(answered);

    QCOMPARE(m_daemon->requestsTo("GET", ZONES).size(), 1);
    QCOMPARE(data.toObject().value("timezones").toArray().size(), m_daemon->zones.size());
}

void TstBoxTime::ntpStates_data()
{
    QTest::addColumn<bool>("ntp");
    QTest::addColumn<bool>("synchronized");
    QTest::addColumn<QString>("expected");

    QTest::newRow("synchronized") << true << true << "synchronized";
    QTest::newRow("not synchronized") << true << false << "notSynchronized";
    QTest::newRow("manual") << false << false << "manual";
    QTest::newRow("manual, clock still in sync") << false << true << "manual";
}

void TstBoxTime::ntpStates()
{
    QFETCH(bool, ntp);
    QFETCH(bool, synchronized);
    QFETCH(QString, expected);

    m_daemon->box["ntp"] = ntp;
    m_daemon->box["synchronized"] = synchronized;
    readBox();

    QCOMPARE(m_mgr->get_ntp(), ntp);
    QCOMPARE(m_mgr->get_ntpState(), expected);
}

void TstBoxTime::clockInTheBoxZone()
{
    readBox();

    QCOMPARE(m_mgr->get_timezone(), QStringLiteral("Europe/Paris"));
    QVERIFY2(m_mgr->get_timeText().startsWith(QStringLiteral("10:12:0")), qPrintable(m_mgr->get_timeText()));
    QVERIFY2(m_mgr->get_dateText().contains(QStringLiteral("2026")), qPrintable(m_mgr->get_dateText()));

    const QVariantMap now = m_mgr->localNow();
    QCOMPARE(now.value("hour").toInt(), 10);
    QCOMPARE(now.value("day").toInt(), 25);
}

void TstBoxTime::clockOfAnUnknownZoneSaysUtc()
{
    m_daemon->box["timezone"] = QStringLiteral("Mars/Olympus");
    readBox();

    QVERIFY(m_mgr->get_loaded());
    QCOMPARE(m_mgr->get_timezone(), QStringLiteral("Mars/Olympus"));
    QVERIFY2(m_mgr->get_timeText().startsWith(QStringLiteral("09:12:0")), qPrintable(m_mgr->get_timeText()));
    QVERIFY2(m_mgr->get_timeText().endsWith(QStringLiteral(" UTC")), qPrintable(m_mgr->get_timeText()));
}

void TstBoxTime::readsAtEachOpening()
{
    readBox();
    m_mgr->deactivate();

    m_daemon->box["timezone"] = QStringLiteral("Asia/Tokyo");
    m_mgr->activate();

    //nothing of the previous opening is shown meanwhile
    QVERIFY(!m_mgr->get_loaded());
    QVERIFY(m_mgr->get_timezone().isEmpty());
    QTRY_COMPARE(m_mgr->get_timezone(), QStringLiteral("Asia/Tokyo"));
}

void TstBoxTime::aReadThatFailsLeavesNoValue_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");

    QTest::newRow("500") << 500 << FakeTimeDaemon::failure("internal", "no bus");
    QTest::newRow("no ntp field") << 200 << FakeTimeDaemon::success({{ "now", "2026-10-25T09:12:03Z" }, { "timezone", "UTC" }});
    QTest::newRow("no zone field") << 200 << FakeTimeDaemon::success({{ "now", "2026-10-25T09:12:03Z" }, { "ntp", true }});
    QTest::newRow("bad date") << 200 << FakeTimeDaemon::success({{ "now", "yesterday" }, { "timezone", "UTC" }, { "ntp", true }});
    QTest::newRow("empty object") << 200 << FakeTimeDaemon::success({{ "x", 1 }});
}

void TstBoxTime::aReadThatFailsLeavesNoValue()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);

    readBox();
    QVERIFY(m_mgr->get_loaded());

    m_daemon->script("GET", "/system/time", status, body);
    m_mgr->refresh();
    QTRY_VERIFY(!m_mgr->get_loadError().isEmpty());

    QVERIFY(!m_mgr->get_loaded());
    QVERIFY(m_mgr->get_timeText().isEmpty());
    QVERIFY(m_mgr->get_dateText().isEmpty());
    QVERIFY(m_mgr->get_timezone().isEmpty());
    QVERIFY(m_mgr->get_ntpState().isEmpty());
    QVERIFY(!m_mgr->get_ntp());
    QVERIFY(!m_mgr->get_updateRequired());

    m_mgr->refresh();
    QTRY_VERIFY(m_mgr->get_loaded());
    QVERIFY(m_mgr->get_loadError().isEmpty());
}

void TstBoxTime::writeIsFollowedByARead()
{
    readBox();
    const int readsBefore = m_daemon->requestsTo("GET", TIME).size();

    m_mgr->setNtp(false);
    QVERIFY(m_mgr->get_busy());
    waitIdle();

    QCOMPARE(m_daemon->requestsTo("POST", TIME).size(), 1);
    QCOMPARE(m_daemon->requestsTo("GET", TIME).size(), readsBefore + 1);
    QCOMPARE(m_daemon->requests.constLast().method, QByteArray("GET"));
    QCOMPARE(m_mgr->get_ntpState(), QStringLiteral("manual"));
}

void TstBoxTime::shownValuesComeFromTheRead()
{
    readBox();

    //a 200 that did not stick: the page keeps saying what the box says
    m_daemon->script("POST", "/system/time", 200, FakeTimeDaemon::success(m_daemon->box));
    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    waitIdle();

    QCOMPARE(m_mgr->get_timezone(), QStringLiteral("Europe/Paris"));
}

void TstBoxTime::busyUntilTheReadAnswers()
{
    readBox();

    m_daemon->box["ntp"] = false;
    m_daemon->scriptHold("GET", "/system/time");
    m_mgr->setNtp(false);
    QTRY_COMPARE(m_daemon->requestsTo("GET", TIME).size(), 2);

    QVERIFY(m_mgr->get_busy());
    m_mgr->setNtp(true); //judged on stale values, refused
    QTest::qWait(50);
    QCOMPARE(m_daemon->requestsTo("POST", TIME).size(), 1);

    m_daemon->release();
    waitIdle();
    QVERIFY(!m_mgr->get_ntp());
}

void TstBoxTime::noWriteBeforeARead()
{
    m_mgr->setNtp(true);
    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    m_mgr->applyManualTime(QStringLiteral("2026-07-15T08:00:00+02:00"));
    QTest::qWait(50);

    QVERIFY(m_daemon->requests.isEmpty());
    QVERIFY(!m_mgr->get_busy());
}

void TstBoxTime::aReadOlderThanAWriteIsIgnored()
{
    readBox();

    m_daemon->scriptHold("GET", "/system/time"); //holds the state before the write
    m_mgr->refresh();
    QTRY_COMPARE(m_daemon->requestsTo("GET", TIME).size(), 2);

    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    QTRY_COMPARE(m_mgr->get_timezone(), QStringLiteral("Asia/Tokyo"));
    waitIdle();

    m_daemon->release();
    QTest::qWait(50);
    QCOMPARE(m_mgr->get_timezone(), QStringLiteral("Asia/Tokyo"));
}

void TstBoxTime::manualTimeBody_data()
{
    QTest::addColumn<bool>("ntp");
    QTest::addColumn<QByteArray>("expected");

    QTest::newRow("ntp on") << true << QByteArray(R"({"ntp":false,"time":"2026-07-15T08:00:00+02:00"})");
    QTest::newRow("ntp off") << false << QByteArray(R"({"time":"2026-07-15T08:00:00+02:00"})");
}

void TstBoxTime::manualTimeBody()
{
    QFETCH(bool, ntp);
    QFETCH(QByteArray, expected);

    m_daemon->box["ntp"] = ntp;
    readBox();

    m_mgr->applyManualTime(QStringLiteral("2026-07-15T08:00:00+02:00"));
    waitIdle();

    const auto posts = m_daemon->requestsTo("POST", TIME);
    QCOMPARE(posts.size(), 1);
    QCOMPARE(posts.first().body, expected);
    QVERIFY(m_mgr->get_message().isEmpty());
    QVERIFY(!m_mgr->get_ntp());
}

void TstBoxTime::manualTimeOffset_data()
{
    QTest::addColumn<QString>("zone");
    QTest::addColumn<QList<int>>("local");
    QTest::addColumn<QString>("expected");

    QTest::newRow("winter") << "Europe/Paris" << QList<int>{ 2026, 12, 1, 8, 0, 0 } << "2026-12-01T08:00:00+01:00";
    QTest::newRow("summer") << "Europe/Paris" << QList<int>{ 2026, 7, 1, 8, 0, 0 } << "2026-07-01T08:00:00+02:00";
    QTest::newRow("other zone than the screen") << "Asia/Tokyo" << QList<int>{ 2026, 7, 1, 8, 0, 0 } << "2026-07-01T08:00:00+09:00";
}

void TstBoxTime::manualTimeOffset()
{
    QFETCH(QString, zone);
    QFETCH(QList<int>, local);
    QFETCH(QString, expected);

    m_daemon->box["timezone"] = zone;
    readBox();

    const QVariantMap res = m_mgr->checkManualTime(local[0], local[1], local[2], local[3], local[4], local[5]);
    QVERIFY(res.value("ok").toBool());
    QCOMPARE(res.value("time").toString(), expected);
}

void TstBoxTime::manualConfirmation_data()
{
    QTest::addColumn<bool>("ntp");
    QTest::addColumn<int>("hour");
    QTest::addColumn<int>("minute");
    QTest::addColumn<bool>("needsConfirm");
    QTest::addColumn<QStringList>("says");
    QTest::addColumn<QStringList>("doesNotSay");

    //the box says 10:12:03 in Paris
    const QString ntpOff = QStringLiteral("Automatic time (NTP) will be turned off");
    const QString step = QStringLiteral("will move by");
    QTest::newRow("small step") << false << 10 << 30 << false << QStringList{} << QStringList{ ntpOff, step };
    QTest::newRow("just under an hour") << false << 11 << 10 << false << QStringList{} << QStringList{ ntpOff, step };
    QTest::newRow("just over an hour") << false << 11 << 13 << true << QStringList{ step, "61 min" } << QStringList{ ntpOff };
    QTest::newRow("back more than an hour") << false << 9 << 10 << true << QStringList{ step, "62 min" } << QStringList{ ntpOff };
    QTest::newRow("hours") << false << 15 << 12 << true << QStringList{ "5 h" } << QStringList{ ntpOff };
    QTest::newRow("ntp on, small step") << true << 10 << 30 << true << QStringList{ ntpOff } << QStringList{ step };
    QTest::newRow("ntp on, big step") << true << 15 << 12 << true << QStringList{ ntpOff, step } << QStringList{};
}

void TstBoxTime::manualConfirmation()
{
    QFETCH(bool, ntp);
    QFETCH(int, hour);
    QFETCH(int, minute);
    QFETCH(bool, needsConfirm);
    QFETCH(QStringList, says);
    QFETCH(QStringList, doesNotSay);

    m_daemon->box["ntp"] = ntp;
    readBox();

    const QVariantMap res = m_mgr->checkManualTime(2026, 10, 25, hour, minute, 3);
    QVERIFY(res.value("ok").toBool());
    QCOMPARE(res.value("needsConfirm").toBool(), needsConfirm);

    const QString text = res.value("confirmText").toString();
    QVERIFY2(text.contains(QStringLiteral("Europe/Paris")), qPrintable(text));
    for (const QString &s: std::as_const(says))
        QVERIFY2(text.contains(s), qPrintable(text));
    for (const QString &s: std::as_const(doesNotSay))
        QVERIFY2(!text.contains(s), qPrintable(text));

    //checking changes nothing on the box
    QVERIFY(m_daemon->requestsTo("POST", TIME).isEmpty());
}

void TstBoxTime::manualProblems_data()
{
    QTest::addColumn<QString>("zone");
    QTest::addColumn<QList<int>>("local");
    QTest::addColumn<QString>("expected");

    QTest::newRow("skipped hour") << "Europe/Paris" << QList<int>{ 2026, 3, 29, 2, 30, 0 } << "does not exist in Europe/Paris";
    QTest::newRow("no such day") << "Europe/Paris" << QList<int>{ 2026, 2, 30, 12, 0, 0 } << "complete date and time";
    QTest::newRow("year") << "Europe/Paris" << QList<int>{ 2020, 1, 1, 0, 30, 0 } << "between 2020 and 2100";
    QTest::newRow("zone unknown here") << "Mars/Olympus" << QList<int>{ 2026, 1, 1, 12, 0, 0 } << "does not know the time zone Mars/Olympus";
}

void TstBoxTime::manualProblems()
{
    QFETCH(QString, zone);
    QFETCH(QList<int>, local);
    QFETCH(QString, expected);

    m_daemon->box["timezone"] = zone;
    readBox();

    const QVariantMap res = m_mgr->checkManualTime(local[0], local[1], local[2], local[3], local[4], local[5]);
    QVERIFY(!res.value("ok").toBool());
    QVERIFY2(res.value("problem").toString().contains(expected), qPrintable(res.value("problem").toString()));
    QVERIFY(res.value("time").toString().isEmpty());
}

void TstBoxTime::routesMissing_data()
{
    QTest::addColumn<bool>("oldDaemon");

    QTest::newRow("old daemon, plain text") << true;
    QTest::newRow("no such route, enveloped") << false;
}

void TstBoxTime::routesMissing()
{
    QFETCH(bool, oldDaemon);

    m_daemon->oldDaemon = oldDaemon;
    m_daemon->noTimeRoutes = !oldDaemon;
    readBox();

    QVERIFY(m_mgr->get_updateRequired());
    QVERIFY(m_mgr->get_loadError().isEmpty());
    QVERIFY(!m_mgr->get_loaded());
    QVERIFY(m_mgr->get_timezone().isEmpty());
    QTRY_VERIFY(!m_mgr->get_zonesError().isEmpty());

    //no fallback on a legacy route
    for (const auto &r: std::as_const(m_daemon->requests))
        QVERIFY2(r.path.startsWith(QStringLiteral("/api/v2/")), qPrintable(r.path));
}

void TstBoxTime::routesMissingOnWrite()
{
    readBox();

    //the daemon was replaced while the page was open
    m_daemon->noTimeRoutes = true;
    m_mgr->setNtp(false);
    QTRY_VERIFY(m_mgr->get_updateRequired());
    waitIdle();

    QVERIFY(!m_mgr->get_loaded());
    QVERIFY(m_mgr->get_timezone().isEmpty());
    QVERIFY(!m_mgr->get_messageIsError());
}

void TstBoxTime::refused_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("expected");

    QTest::newRow("time") << "time" << "refused this date and time";
    QTest::newRow("zone") << "zone" << "does not know this time zone";
    QTest::newRow("ntp") << "ntp" << "refused this change";
}

void TstBoxTime::refused()
{
    QFETCH(QString, kind);
    QFETCH(QString, expected);

    readBox();
    const int readsBefore = m_daemon->requestsTo("GET", TIME).size();

    m_daemon->script("POST", "/system/time", 400, FakeTimeDaemon::failure("invalid_request", "raw daemon text"));
    if (kind == QLatin1String("time"))
        m_mgr->applyManualTime(QStringLiteral("2026-07-15T08:00:00+02:00"));
    else if (kind == QLatin1String("zone"))
        m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    else
        m_mgr->setNtp(false);
    waitIdle();

    QVERIFY2(m_mgr->get_message().contains(expected), qPrintable(m_mgr->get_message()));
    QVERIFY(!m_mgr->get_message().contains(QStringLiteral("raw daemon text")));
    QVERIFY(m_mgr->get_messageIsError());
    QCOMPARE(m_daemon->requestsTo("GET", TIME).size(), readsBefore + 1);
    QCOMPARE(m_mgr->get_timezone(), QStringLiteral("Europe/Paris"));
    QVERIFY(!m_mgr->get_restartRequired());
}

void TstBoxTime::upgradeInProgress()
{
    readBox();

    m_daemon->script("POST", "/system/time", 409, FakeTimeDaemon::failure("upgrade_in_progress", "busy"));
    m_mgr->setNtp(false);
    waitIdle();

    QVERIFY2(m_mgr->get_message().contains(QStringLiteral("update is being installed")), qPrintable(m_mgr->get_message()));
    QVERIFY(m_mgr->get_ntp());
}

void TstBoxTime::partialFailure_data()
{
    QTest::addColumn<QStringList>("applied");
    QTest::addColumn<QString>("failed");
    QTest::addColumn<QString>("expected");
    QTest::addColumn<bool>("restart");

    QTest::newRow("zone done, ntp failed")
        << QStringList{ "timezone" } << "ntp" << "Changed: time zone. Not changed: automatic time." << true;
    QTest::newRow("ntp done, time failed")
        << QStringList{ "ntp" } << "time" << "Changed: automatic time. Not changed: time." << false;
    QTest::newRow("first step failed") << QStringList{} << "timezone" << "Not changed: time zone." << false;
    QTest::newRow("done, reading back failed")
        << QStringList{ "timezone", "ntp" } << "" << "could not read it back" << true;
}

void TstBoxTime::partialFailure()
{
    QFETCH(QStringList, applied);
    QFETCH(QString, failed);
    QFETCH(QString, expected);
    QFETCH(bool, restart);

    readBox();
    const int readsBefore = m_daemon->requestsTo("GET", TIME).size();

    QJsonObject data{{ "applied", QJsonArray::fromStringList(applied) }};
    if (!failed.isEmpty())
        data["failed"] = failed;
    m_daemon->script("POST", "/system/time", 500, FakeTimeDaemon::failure("internal", "timedated", data));

    m_mgr->setNtp(false);
    waitIdle();

    QVERIFY2(m_mgr->get_message().contains(expected), qPrintable(m_mgr->get_message()));
    QVERIFY(m_mgr->get_messageIsError());
    QCOMPARE(m_mgr->get_restartRequired(), restart);
    QCOMPARE(m_daemon->requestsTo("GET", TIME).size(), readsBefore + 1);
}

void TstBoxTime::boxUnreachable()
{
    readBox();

    m_daemon->stop();
    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    QTRY_VERIFY(!m_mgr->get_busy());

    QVERIFY2(m_mgr->get_message().contains(QStringLiteral("Nothing was changed")), qPrintable(m_mgr->get_message()));
    QVERIFY(!m_mgr->get_unconfirmed());
    QVERIFY(!m_mgr->get_restartRequired());
    //the read that follows failed as well: nothing stale stays on screen
    QVERIFY(!m_mgr->get_loaded());
    QVERIFY(!m_mgr->get_loadError().isEmpty());
}

void TstBoxTime::noAnswer_data()
{
    QTest::addColumn<bool>("zoneLanded");

    QTest::newRow("the change went through") << true;
    QTest::newRow("the change did not") << false;
}

void TstBoxTime::noAnswer()
{
    QFETCH(bool, zoneLanded);

    readBox();

    m_daemon->scriptDrop("POST", "/system/time");
    if (zoneLanded)
        m_daemon->box["timezone"] = QStringLiteral("Asia/Tokyo");
    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    waitIdle();

    QVERIFY(m_mgr->get_unconfirmed());
    QVERIFY2(m_mgr->get_message().contains(QStringLiteral("may have been made")), qPrintable(m_mgr->get_message()));
    QCOMPARE(m_mgr->get_timezone(), zoneLanded ? QStringLiteral("Asia/Tokyo") : QStringLiteral("Europe/Paris"));
    QCOMPARE(m_mgr->get_restartRequired(), zoneLanded);
}

void TstBoxTime::restartRequired()
{
    readBox();

    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    waitIdle();
    QVERIFY(m_mgr->get_restartRequired());
    QVERIFY(!m_mgr->get_serverRestartDone());

    //offering it is all the page does
    QVERIFY(m_daemon->requestsTo("POST", RESTART).isEmpty());

    //the page can be left and come back: the box still has to be restarted
    m_mgr->deactivate();
    m_mgr->activate();
    QTRY_VERIFY(m_mgr->get_loaded());
    QVERIFY(m_mgr->get_restartRequired());

    m_mgr->dismissRestart();
    QVERIFY(!m_mgr->get_restartRequired());
}

void TstBoxTime::noRestartWithoutAZoneChange()
{
    readBox();

    m_mgr->setTimezone(QStringLiteral("Europe/Paris"));
    QTest::qWait(50);
    QVERIFY(m_daemon->requestsTo("POST", TIME).isEmpty());

    m_mgr->setNtp(false);
    waitIdle();
    QVERIFY(!m_mgr->get_restartRequired());
}

void TstBoxTime::restartServer()
{
    readBox();
    m_mgr->setTimezone(QStringLiteral("Asia/Tokyo"));
    waitIdle();

    m_mgr->restartServer();
    QTRY_VERIFY(m_mgr->get_serverRestartDone());
    waitIdle();

    const auto posts = m_daemon->requestsTo("POST", RESTART);
    QCOMPARE(posts.size(), 1);
    QCOMPARE(posts.first().body, QByteArray(R"({"unit":"calaos-server"})"));
    QVERIFY(!m_mgr->get_messageIsError());

    //a new zone needs a new restart
    m_mgr->setTimezone(QStringLiteral("Europe/Berlin"));
    waitIdle();
    QVERIFY(!m_mgr->get_serverRestartDone());
}

void TstBoxTime::restartServerRefused_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("expected");

    QTest::newRow("upgrade") << 409 << FakeTimeDaemon::failure("upgrade_in_progress", "busy") << "update is being installed";
    QTest::newRow("unit busy") << 409 << FakeTimeDaemon::failure("service_unavailable", "busy") << "could not be restarted";
    QTest::newRow("route missing") << 404 << FakeTimeDaemon::failure("not_found", "no") << "must be updated";
}

void TstBoxTime::restartServerRefused()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(QString, expected);

    readBox();
    m_daemon->script("POST", "/system/services/restart", status, body);
    m_mgr->restartServer();
    QTRY_VERIFY(!m_mgr->get_message().isEmpty());
    waitIdle();

    QVERIFY2(m_mgr->get_message().contains(expected), qPrintable(m_mgr->get_message()));
    QVERIFY(m_mgr->get_messageIsError());
    QVERIFY(!m_mgr->get_serverRestartDone());
}

void TstBoxTime::zoneList()
{
    readBox();
    QTRY_COMPARE(m_mgr->get_zones(), m_daemon->zones);

    m_mgr->setZoneFilter(QStringLiteral("new york"));
    QCOMPARE(m_mgr->get_zones(), QStringList{ "America/New_York" });

    m_mgr->setZoneFilter(QStringLiteral("atlantis"));
    QVERIFY(m_mgr->get_zones().isEmpty());
    QVERIFY(m_mgr->get_zonesError().isEmpty());

    //a new opening starts from the whole list, without asking the box again
    m_mgr->deactivate();
    m_mgr->activate();
    QCOMPARE(m_mgr->get_zones().size(), m_daemon->zones.size());
    QCOMPARE(m_daemon->requestsTo("GET", ZONES).size(), 1);
}

void TstBoxTime::currentZoneMissingFromTheList()
{
    m_daemon->box["timezone"] = QStringLiteral("Etc/Old_Zone");
    readBox();

    QTRY_COMPARE(m_mgr->get_zones().value(0), QStringLiteral("Etc/Old_Zone"));
    QCOMPARE(m_mgr->get_zones().size(), m_daemon->zones.size() + 1);
}

void TstBoxTime::zoneListFailsThenRetries()
{
    m_daemon->script("GET", "/system/timezones", 500, FakeTimeDaemon::failure("internal", "no"));
    readBox();
    QTRY_VERIFY(!m_mgr->get_zonesError().isEmpty());
    QVERIFY(m_mgr->get_zones().isEmpty());
    //the page itself still works
    QVERIFY(m_mgr->get_loaded());

    m_mgr->loadZones();
    QTRY_COMPARE(m_mgr->get_zones().size(), m_daemon->zones.size());
    QVERIFY(m_mgr->get_zonesError().isEmpty());
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstBoxTime test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_boxtime.moc"
