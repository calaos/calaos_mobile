//Unit tests for CalaosOsProtocol: the wire rules of the calaos-container
//daemon API (docs/API.md of calaos-container), without any network.
//
//The rules that matter most:
// - a plain 404 (no JSON envelope) on /api/v2 means "old daemon, use the
//   legacy routes"; a 404 that carries an error envelope with a code
//   (not_found, package_not_found) comes from a daemon that serves /api/v2;
// - only the terminal state of the job we follow ends an upgrade.

#include <QtTest>
#include "CalaosOsProtocol.h"

using namespace CalaosOsProtocol;

static QJsonDocument json(const char *s)
{
    return QJsonDocument::fromJson(QByteArray(s));
}

static Job job(const QString &id, const QString &status, bool reboot = false)
{
    QJsonObject o = {{ "id", id }, { "kind", "upgrade_all" }, { "status", status },
                     { "reboot_required", reboot }, { "daemon_restart", false }};
    return parseJob(o);
}

class TstCalaosOsProtocol: public QObject
{
    Q_OBJECT

private slots:
    void envelope_data();
    void envelope();

    void legacyDaemonReply_data();
    void legacyDaemonReply();

    void upgradeRouteMissing_data();
    void upgradeRouteMissing();

    void upgradeInProgress_data();
    void upgradeInProgress();
    void packageNotFound_data();
    void packageNotFound();

    void idleProgress_data();
    void idleProgress();

    void authorizationHeader();

    void jobParsing();
    void jobTerminalStates_data();
    void jobTerminalStates();

    void updateStatusV2();
    void updateStatusV2WithoutJobs();
    void updateStatusLegacy();

    void trackerFinishesOnlyOnTerminalStateOfItsJob();
    void trackerTerminalEventBeforeThe202();
    void trackerIgnoresStatusWhilePending();
    void trackerStatusFinishesTheJob();
    void trackerStatusLosesTheJob_data();
    void trackerStatusLosesTheJob();
    void trackerAdoptsRunningJobs();
    void trackerAdoptedRunWithoutIdEndsOnLiveTerminalEvent();
    void trackerIgnoresInvalidJobs();
    void trackerIgnoresStatusOfDaemonWithoutJobs();
};

void TstCalaosOsProtocol::envelope_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("isEnvelope");
    QTest::addColumn<bool>("error");
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("msg");

    QTest::newRow("success") << QByteArray(R"({"error":false,"msg":"ok","data":{"a":1}})") << true << false << "" << "ok";
    QTest::newRow("success null data") << QByteArray(R"({"error":false,"msg":"ok","data":null})") << true << false << "" << "ok";
    QTest::newRow("error with code") << QByteArray(R"({"error":true,"code":"upgrade_in_progress","msg":"busy","data":{"job_id":"x"}})") << true << true << "upgrade_in_progress" << "busy";
    QTest::newRow("legacy system ok") << QByteArray(R"({"error":false,"msg":"ok","output":{}})") << false << false << "" << "ok";
    QTest::newRow("legacy error") << QByteArray(R"({"error":true,"msg":"wrong token"})") << false << true << "" << "wrong token";
    QTest::newRow("legacy raw update") << QByteArray(R"({"calaos-server":{"version":"1"}})") << false << false << "" << "";
    QTest::newRow("error not bool") << QByteArray(R"({"error":"yes","code":"x","data":1})") << false << false << "" << "";
    QTest::newRow("array") << QByteArray(R"([1,2])") << false << false << "" << "";
    QTest::newRow("plain text") << QByteArray("Cannot GET /api/v2/update/check") << false << false << "" << "";
    QTest::newRow("empty") << QByteArray() << false << false << "" << "";
}

void TstCalaosOsProtocol::envelope()
{
    QFETCH(QByteArray, body);
    QFETCH(bool, isEnvelope);
    QFETCH(bool, error);
    QFETCH(QString, code);
    QFETCH(QString, msg);

    const Envelope env = parseEnvelope(QJsonDocument::fromJson(body));
    QCOMPARE(env.isEnvelope, isEnvelope);
    QCOMPARE(env.error, error);
    QCOMPARE(env.code, code);
    QCOMPARE(env.msg, msg);
}

void TstCalaosOsProtocol::legacyDaemonReply_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("legacy");

    QTest::newRow("plain 404 (fiber)") << 404 << QByteArray("Cannot GET /api/v2/update/status") << true;
    QTest::newRow("empty 404") << 404 << QByteArray() << true;
    QTest::newRow("json 404 without code") << 404 << QByteArray(R"({"error":true,"msg":"not here"})") << true;
    QTest::newRow("enveloped not_found") << 404 << QByteArray(R"({"error":true,"code":"not_found","msg":"no route"})") << false;
    QTest::newRow("enveloped package_not_found") << 404 << QByteArray(R"({"error":true,"code":"package_not_found","msg":"no update"})") << false;
    QTest::newRow("200 envelope") << 200 << QByteArray(R"({"error":false,"msg":"ok","data":{}})") << false;
    QTest::newRow("legacy 401") << 401 << QByteArray(R"({"error":true,"msg":"wrong token"})") << false;
    QTest::newRow("500 plain") << 500 << QByteArray("boom") << false;
    QTest::newRow("transport") << 0 << QByteArray() << false;
}

void TstCalaosOsProtocol::legacyDaemonReply()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(bool, legacy);

    QCOMPARE(isLegacyDaemonReply(status, QJsonDocument::fromJson(body)), legacy);
}

void TstCalaosOsProtocol::upgradeRouteMissing_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("missing");

    QTest::newRow("v2 without jobs") << 404 << QByteArray(R"({"error":true,"code":"not_found","msg":"no route"})") << true;
    QTest::newRow("package_not_found") << 404 << QByteArray(R"({"error":true,"code":"package_not_found","msg":"no update"})") << false;
    QTest::newRow("plain 404") << 404 << QByteArray("Cannot POST") << false;
    QTest::newRow("202") << 202 << QByteArray(R"({"error":false,"msg":"accepted","data":{"id":"a","status":"running"}})") << false;
}

void TstCalaosOsProtocol::upgradeRouteMissing()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(bool, missing);

    QCOMPARE(isUpgradeRouteMissing(status, parseEnvelope(QJsonDocument::fromJson(body))), missing);
}

void TstCalaosOsProtocol::upgradeInProgress_data()
{
    QTest::addColumn<int>("api");
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("legacyMsg");
    QTest::addColumn<bool>("busy");

    const int v2 = int(ApiVersion::V2);
    const int legacy = int(ApiVersion::Legacy);

    QTest::newRow("v2 409 code") << v2 << 409 << QByteArray(R"({"error":true,"code":"upgrade_in_progress","msg":"x"})") << QString() << true;
    //v2 never matches on the message
    QTest::newRow("v2 message only") << v2 << 500 << QByteArray(R"({"error":true,"code":"internal","msg":"upgrade in progress"})") << QString() << false;
    QTest::newRow("v2 other 409") << v2 << 409 << QByteArray(R"({"error":true,"code":"admin_already_set","msg":"x"})") << QString() << false;
    QTest::newRow("legacy check 500") << legacy << 500 << QByteArray() << "upgrade already in progress" << true;
    QTest::newRow("legacy reboot 409") << legacy << 409 << QByteArray() << "whatever" << true;
    QTest::newRow("legacy other") << legacy << 500 << QByteArray() << "apt failed" << false;
}

void TstCalaosOsProtocol::upgradeInProgress()
{
    QFETCH(int, api);
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(QString, legacyMsg);
    QFETCH(bool, busy);

    QCOMPARE(isUpgradeInProgress(ApiVersion(api), status, parseEnvelope(QJsonDocument::fromJson(body)), legacyMsg), busy);
}

void TstCalaosOsProtocol::packageNotFound_data()
{
    QTest::addColumn<int>("api");
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("legacyMsg");
    QTest::addColumn<bool>("notFound");

    const int v2 = int(ApiVersion::V2);
    const int legacy = int(ApiVersion::Legacy);

    QTest::newRow("v2 code") << v2 << 404 << QByteArray(R"({"error":true,"code":"package_not_found","msg":"x"})") << QString() << true;
    QTest::newRow("v2 not_found route") << v2 << 404 << QByteArray(R"({"error":true,"code":"not_found","msg":"x"})") << QString() << false;
    QTest::newRow("legacy message") << legacy << 500 << QByteArray() << "package not found" << true;
    QTest::newRow("legacy other") << legacy << 500 << QByteArray() << "apt failed" << false;
}

void TstCalaosOsProtocol::packageNotFound()
{
    QFETCH(int, api);
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(QString, legacyMsg);
    QFETCH(bool, notFound);

    QCOMPARE(isPackageNotFound(ApiVersion(api), status, parseEnvelope(QJsonDocument::fromJson(body)), legacyMsg), notFound);
}

void TstCalaosOsProtocol::idleProgress_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("idle");

    QTest::newRow("idle") << QByteArray(R"({"status":"idle"})") << true;
    QTest::newRow("empty (daemons 1.5 to 1.7)") << QByteArray(R"({"status":""})") << true;
    QTest::newRow("missing status") << QByteArray(R"({"total_percent":3})") << false;
    QTest::newRow("upgrading") << QByteArray(R"({"status":"upgrading"})") << false;
    QTest::newRow("done") << QByteArray(R"({"status":"done"})") << false;
    QTest::newRow("error") << QByteArray(R"({"status":"error"})") << false;
}

void TstCalaosOsProtocol::idleProgress()
{
    QFETCH(QByteArray, body);
    QFETCH(bool, idle);

    QCOMPARE(isIdleProgress(QJsonDocument::fromJson(body).object()), idle);
}

void TstCalaosOsProtocol::authorizationHeader()
{
    QCOMPARE(CalaosOsProtocol::authorizationHeader(QStringLiteral("abc123")), QStringLiteral("Bearer abc123"));
}

void TstCalaosOsProtocol::jobParsing()
{
    const Job j = parseJob(json(R"({"id":"3f9c2a7e5b1d4c80","kind":"upgrade","package":"calaos-server",
        "status":"error","error":"upgrade failed: calaos-server","reboot_required":true,
        "daemon_restart":true,"started_at":"2026-10-22T09:12:03Z"})").object());
    QVERIFY(j.valid);
    QCOMPARE(j.id, QStringLiteral("3f9c2a7e5b1d4c80"));
    QCOMPARE(j.kind, QStringLiteral("upgrade"));
    QCOMPARE(j.package, QStringLiteral("calaos-server"));
    QCOMPARE(j.status, QStringLiteral("error"));
    QCOMPARE(j.error, QStringLiteral("upgrade failed: calaos-server"));
    QVERIFY(j.rebootRequired);
    QVERIFY(j.daemonRestart);

    QVERIFY(!parseJob(QJsonValue()).valid);
    QVERIFY(!parseJob(QJsonValue(QJsonValue::Null)).valid);
    QVERIFY(!parseJob(json(R"({"status":"running"})").object()).valid);
    QVERIFY(!parseJob(json(R"({"id":"a"})").object()).valid);
}

void TstCalaosOsProtocol::jobTerminalStates_data()
{
    QTest::addColumn<QString>("status");
    QTest::addColumn<bool>("terminal");
    QTest::addColumn<bool>("succeeded");

    QTest::newRow("running") << "running" << false << false;
    QTest::newRow("done") << "done" << true << true;
    QTest::newRow("error") << "error" << true << false;
    QTest::newRow("unknown") << "paused" << false << false;
}

void TstCalaosOsProtocol::jobTerminalStates()
{
    QFETCH(QString, status);
    QFETCH(bool, terminal);
    QFETCH(bool, succeeded);

    const Job j = job("a", status);
    QCOMPARE(j.isTerminal(), terminal);
    QCOMPARE(j.succeeded(), succeeded);
}

void TstCalaosOsProtocol::updateStatusV2()
{
    const QJsonValue data = json(R"({"progress":{"status":"upgrading","total_percent":42},
        "job":{"id":"j1","kind":"upgrade_all","status":"running","reboot_required":false}})").object();
    const UpdateStatus st = parseUpdateStatus(ApiVersion::V2, data);
    QCOMPARE(st.progress["status"].toString(), QStringLiteral("upgrading"));
    QCOMPARE(st.progress["total_percent"].toInt(), 42);
    QVERIFY(st.hasJobField);
    QVERIFY(st.job.valid);
    QCOMPARE(st.job.id, QStringLiteral("j1"));

    const UpdateStatus none = parseUpdateStatus(ApiVersion::V2, json(R"({"progress":{"status":"idle"},"job":null})").object());
    QVERIFY(none.hasJobField);
    QVERIFY(!none.job.valid);
}

void TstCalaosOsProtocol::updateStatusV2WithoutJobs()
{
    const UpdateStatus st = parseUpdateStatus(ApiVersion::V2, json(R"({"progress":{"status":"idle"}})").object());
    QVERIFY(!st.hasJobField);
    QVERIFY(!st.job.valid);
    QCOMPARE(st.progress["status"].toString(), QStringLiteral("idle"));
}

void TstCalaosOsProtocol::updateStatusLegacy()
{
    //the legacy route answers the progress object itself
    const UpdateStatus st = parseUpdateStatus(ApiVersion::Legacy, json(R"({"status":"upgrading","total_percent":7,"job":"ignored"})").object());
    QCOMPARE(st.progress["total_percent"].toInt(), 7);
    QVERIFY(!st.hasJobField);
    QVERIFY(!st.job.valid);
}

void TstCalaosOsProtocol::trackerFinishesOnlyOnTerminalStateOfItsJob()
{
    UpgradeJobTracker t;
    t.startPending();
    QVERIFY(t.isPending());
    QCOMPARE(t.follow("mine"), UpgradeJobTracker::Action::None);
    QVERIFY(t.isFollowing());
    QVERIFY(!t.isPending());

    QCOMPARE(t.onEvent(job("mine", "running"), false), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onEvent(job("other", "done"), false), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onEvent(job("other", "running"), false), UpgradeJobTracker::Action::None);
    QVERIFY(t.isFollowing());

    QCOMPARE(t.onEvent(job("mine", "error", true), false), UpgradeJobTracker::Action::Finish);
    QCOMPARE(t.finishedJob().id, QStringLiteral("mine"));
    QCOMPARE(t.finishedJob().status, QStringLiteral("error"));
    QVERIFY(t.finishedJob().rebootRequired);
    QVERIFY(!t.isFollowing());

    //a duplicate terminal event (event + status read) finishes only once
    QCOMPARE(t.onEvent(job("mine", "error"), false), UpgradeJobTracker::Action::None);
}

void TstCalaosOsProtocol::trackerTerminalEventBeforeThe202()
{
    UpgradeJobTracker t;
    t.startPending();
    QCOMPARE(t.onEvent(job("mine", "running"), false), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onEvent(job("mine", "done"), false), UpgradeJobTracker::Action::None);

    QCOMPARE(t.follow("mine"), UpgradeJobTracker::Action::Finish);
    QCOMPARE(t.finishedJob().status, QStringLiteral("done"));
    QVERIFY(!t.isFollowing());

    //the cache does not leak into the next POST
    t.startPending();
    QCOMPARE(t.follow("mine"), UpgradeJobTracker::Action::None);
}

void TstCalaosOsProtocol::trackerIgnoresStatusWhilePending()
{
    UpgradeJobTracker t;
    t.startPending();
    //a read sent before the POST reached the daemon shows an older job
    QCOMPARE(t.onStatus(job("older", "done"), true), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onStatus(Job(), true), UpgradeJobTracker::Action::None);
    QVERIFY(t.isPending());
}

void TstCalaosOsProtocol::trackerStatusFinishesTheJob()
{
    UpgradeJobTracker t;
    t.follow("mine");
    QCOMPARE(t.onStatus(job("mine", "running"), true), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onStatus(job("mine", "done", true), true), UpgradeJobTracker::Action::Finish);
    QVERIFY(t.finishedJob().succeeded());
    QVERIFY(t.finishedJob().rebootRequired);
}

void TstCalaosOsProtocol::trackerStatusLosesTheJob_data()
{
    QTest::addColumn<bool>("nullJob");
    QTest::addColumn<QString>("status");

    QTest::newRow("null job") << true << "";
    QTest::newRow("another running job") << false << "running";
    QTest::newRow("another finished job") << false << "done";
}

void TstCalaosOsProtocol::trackerStatusLosesTheJob()
{
    QFETCH(bool, nullJob);
    QFETCH(QString, status);

    UpgradeJobTracker t;
    t.follow("mine");
    const Job other = nullJob ? Job() : job("other", status);
    QCOMPARE(t.onStatus(other, true), UpgradeJobTracker::Action::Lost);
    QVERIFY(!t.isFollowing());
}

void TstCalaosOsProtocol::trackerAdoptsRunningJobs()
{
    UpgradeJobTracker t;
    QCOMPARE(t.onEvent(job("cli", "done"), false), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onEvent(job("cli", "running"), false), UpgradeJobTracker::Action::Adopt);
    QCOMPARE(t.jobId(), QStringLiteral("cli"));
    QCOMPARE(t.onEvent(job("cli", "done"), false), UpgradeJobTracker::Action::Finish);

    t.clear();
    //an older finished job in update/status is not a run to adopt
    QCOMPARE(t.onStatus(job("old", "error"), true), UpgradeJobTracker::Action::None);
    QCOMPARE(t.onStatus(job("web", "running"), true), UpgradeJobTracker::Action::Adopt);
    QCOMPARE(t.jobId(), QStringLiteral("web"));
}

void TstCalaosOsProtocol::trackerAdoptedRunWithoutIdEndsOnLiveTerminalEvent()
{
    UpgradeJobTracker t;
    QCOMPARE(t.onEvent(job("x", "done"), true), UpgradeJobTracker::Action::Finish);
    QCOMPARE(t.finishedJob().id, QStringLiteral("x"));
}

void TstCalaosOsProtocol::trackerIgnoresInvalidJobs()
{
    UpgradeJobTracker t;
    t.follow("mine");
    QCOMPARE(t.onEvent(Job(), false), UpgradeJobTracker::Action::None);
    QVERIFY(t.isFollowing());
}

void TstCalaosOsProtocol::trackerIgnoresStatusOfDaemonWithoutJobs()
{
    UpgradeJobTracker t;
    t.follow("mine");
    QCOMPARE(t.onStatus(Job(), false), UpgradeJobTracker::Action::None);
    QVERIFY(t.isFollowing());
}

QTEST_APPLESS_MAIN(TstCalaosOsProtocol)
#include "tst_calaososprotocol.moc"
