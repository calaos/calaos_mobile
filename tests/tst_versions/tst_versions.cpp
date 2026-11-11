#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include "CalaosOsAPI.h"
#include "FakeTimeDaemon.h"
#include "VersionsModel.h"
#include "VersionsReport.h"

using namespace VersionsReport;

namespace {

const QString VERSIONS = QStringLiteral("/system/versions");
const QByteArray TOKEN(64, 'k');
const QDateTime NOW(QDate(2026, 11, 11), QTime(23, 30), QTimeZone::UTC);

QJsonObject systemData()
{
    return QJsonObject{
        { "calaos_os", "v4.4.0" },
        { "debian", "13.1" },
        { "kernel", "6.12.48-amd64" },
        { "container", "1.9.1" },
        { "packages", QJsonArray{
              QJsonObject{{ "name", "calaos-container" }, { "version", "1.9.1" }},
              QJsonObject{{ "name", "calaos-server" }, { "version", "4.8.0" }},
              QJsonObject{{ "name", "calaos-web-app" }, { "version", "" }},
          }},
    };
}

QString text(const Report &r)
{
    return formatText(r, QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"),
                      QStringLiteral("6.12.0"), NOW);
}

//A fetcher the test answers by hand, in any order
struct ManualFetcher
{
    QList<VersionsModel::Done> pending;
    VersionsModel::Fetcher fetcher()
    {
        return [this](VersionsModel::Done done) { pending << done; };
    }
};

} //namespace

class TstVersions: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void relayNominal();
    void relaySystemNull_data();
    void relaySystemNull();
    void relayErrors_data();
    void relayErrors();
    void unreadableFieldsAreEmpty();
    void daemonAnswers_data();
    void daemonAnswers();

    void textNominal();
    void textWithoutSystem();
    void textOmitsEmptyFields();
    void textDateIsUtc();

    void modelFillsFromTheReport();
    void modelMessagePerFailure_data();
    void modelMessagePerFailure();
    void modelKeepsOnlyTheLastAnswer();
    void modelReloadKeepsThePreviousValuesUntilTheAnswer();
    void modelCopiesTheText();

    void kioskRoute_data();
    void kioskRoute();

private:
    QTemporaryDir m_dir;
    QString m_tokenPath;
};

void TstVersions::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_tokenPath = m_dir.filePath(QStringLiteral("calaos-ct.token"));
    QFile f(m_tokenPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(TOKEN + "\n");
}

void TstVersions::relayNominal()
{
    const Report r = fromRelayResult(QJsonObject{
        { "server", "4.8.0" }, { "system", systemData() }, { "error", "" }});
    QCOMPARE(r.server, QStringLiteral("4.8.0"));
    QVERIFY(r.hasSystem);
    QCOMPARE(r.failure, Failure::None);
    QCOMPARE(r.calaosOs, QStringLiteral("v4.4.0"));
    QCOMPARE(r.debian, QStringLiteral("13.1"));
    QCOMPARE(r.kernel, QStringLiteral("6.12.48-amd64"));
    QCOMPARE(r.container, QStringLiteral("1.9.1"));
    QCOMPARE(r.packages.size(), 3);
    QCOMPARE(r.packages[1].name, QStringLiteral("calaos-server"));
    QCOMPARE(r.packages[1].version, QStringLiteral("4.8.0"));
    QVERIFY(r.error.isEmpty());
}

void TstVersions::relaySystemNull_data()
{
    QTest::addColumn<QString>("error");
    QTest::addColumn<int>("failure");
    QTest::newRow("unreachable") << "daemon_unreachable" << int(Failure::Unreachable);
    QTest::newRow("too old") << "daemon_too_old" << int(Failure::DaemonTooOld);
    QTest::newRow("internal") << "internal" << int(Failure::Unreachable);
    QTest::newRow("no reason") << "" << int(Failure::Unreachable);
}

void TstVersions::relaySystemNull()
{
    QFETCH(QString, error);
    QFETCH(int, failure);
    const Report r = fromRelayResult(QJsonObject{
        { "server", "4.8.0" }, { "system", QJsonValue::Null }, { "error", error }});
    QCOMPARE(r.server, QStringLiteral("4.8.0"));
    QVERIFY(!r.hasSystem);
    QCOMPARE(int(r.failure), failure);
    QCOMPARE(r.error, error);
    QVERIFY(r.packages.isEmpty());
}

void TstVersions::relayErrors_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("failure");
    QTest::newRow("server without the action") << "invalid_request" << int(Failure::OldServer);
    QTest::newRow("relay server without the action") << "not_allowed" << int(Failure::OldServer);
    QTest::newRow("server without the relay") << "old_server" << int(Failure::OldServer);
    QTest::newRow("timeout") << "timeout" << int(Failure::Unreachable);
    QTest::newRow("https_required") << "https_required" << int(Failure::Unreachable);
    QTest::newRow("transport lost") << "not_connected" << int(Failure::Unreachable);
}

void TstVersions::relayErrors()
{
    QFETCH(QString, code);
    QFETCH(int, failure);
    const Report r = fromRelayError(code, QStringLiteral("4.7.2"));
    QCOMPARE(r.server, QStringLiteral("4.7.2"));
    QVERIFY(!r.hasSystem);
    QCOMPARE(int(r.failure), failure);
    QCOMPARE(r.error, code);
}

void TstVersions::unreadableFieldsAreEmpty()
{
    const Report r = fromRelayResult(QJsonObject{
        { "server", 48 },
        { "system", QJsonObject{
              { "calaos_os", QJsonValue::Null },
              { "kernel", 6 },
              { "packages", QJsonArray{
                    QJsonObject{{ "name", "" }, { "version", "1" }},
                    QJsonObject{{ "version", "2" }},
                    QJsonValue("calaos-server"),
                    QJsonObject{{ "name", "calaos-home" }, { "version", 3 }},
                }},
          }},
    });
    QVERIFY(r.server.isEmpty());
    QVERIFY(r.hasSystem);
    QVERIFY(r.calaosOs.isEmpty());
    QVERIFY(r.debian.isEmpty());
    QVERIFY(r.kernel.isEmpty());
    QVERIFY(r.container.isEmpty());
    QCOMPARE(r.packages.size(), 1);
    QCOMPARE(r.packages[0].name, QStringLiteral("calaos-home"));
    QVERIFY(r.packages[0].version.isEmpty());
}

void TstVersions::daemonAnswers_data()
{
    QTest::addColumn<bool>("ok");
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("hasSystem");
    QTest::addColumn<int>("failure");
    QTest::addColumn<QString>("error");
    QTest::newRow("answered") << true << 200 << true << int(Failure::None) << "";
    QTest::newRow("no route") << false << 404 << false << int(Failure::DaemonTooOld) << "daemon_too_old";
    QTest::newRow("no answer") << false << 0 << false << int(Failure::Unreachable) << "daemon_unreachable";
    QTest::newRow("token refused") << false << 401 << false << int(Failure::Unauthorized)
                                   << "authentication error (http 401)";
    QTest::newRow("server error") << false << 500 << false << int(Failure::Unreachable) << "http 500";
}

void TstVersions::daemonAnswers()
{
    QFETCH(bool, ok);
    QFETCH(int, status);
    QFETCH(bool, hasSystem);
    QFETCH(int, failure);
    QFETCH(QString, error);
    const Report r = fromDaemon(ok, status, ok ? QJsonValue(systemData()) : QJsonValue(), QStringLiteral("4.8.0"));
    QCOMPARE(r.server, QStringLiteral("4.8.0"));
    QCOMPARE(r.hasSystem, hasSystem);
    QCOMPARE(int(r.failure), failure);
    QCOMPARE(r.error, error);
}

void TstVersions::textNominal()
{
    const Report r = fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }});
    QCOMPARE(text(r), QStringLiteral(
        "Calaos versions (2026-11-11)\n"
        "client: calaos-home kiosk 4.5.0 (Qt 6.12.0)\n"
        "calaos-server 4.8.0\n"
        "calaos-container 1.9.1\n"
        "calaos-os v4.4.0 · debian 13.1 · kernel 6.12.48-amd64\n"
        "calaos-container 1.9.1\n"
        "calaos-server 4.8.0\n"
        "calaos-web-app"));
}

void TstVersions::textWithoutSystem()
{
    Report r = fromRelayResult(QJsonObject{
        { "server", "4.8.0" }, { "system", QJsonValue::Null }, { "error", "daemon_unreachable" }});
    QCOMPARE(text(r), QStringLiteral(
        "Calaos versions (2026-11-11)\n"
        "client: calaos-home kiosk 4.5.0 (Qt 6.12.0)\n"
        "calaos-server 4.8.0\n"
        "calaos-container unreachable (daemon_unreachable)"));

    r = fromRelayError(QString(), QString());
    QCOMPARE(text(r), QStringLiteral(
        "Calaos versions (2026-11-11)\n"
        "client: calaos-home kiosk 4.5.0 (Qt 6.12.0)\n"
        "calaos-server unavailable\n"
        "calaos-container unreachable"));
}

void TstVersions::textOmitsEmptyFields()
{
    const Report r = fromDaemon(true, 200, QJsonObject{{ "debian", "13.1" }}, QStringLiteral("4.8.0"));
    QCOMPARE(formatText(r, QStringLiteral("calaos-home ios"), QStringLiteral("4.5.0"), QString(), NOW),
             QStringLiteral(
                 "Calaos versions (2026-11-11)\n"
                 "client: calaos-home ios 4.5.0\n"
                 "calaos-server 4.8.0\n"
                 "debian 13.1"));
}

void TstVersions::textDateIsUtc()
{
    //already the 12th in Paris
    const QDateTime late(QDate(2026, 11, 11), QTime(23, 30), QTimeZone("Europe/Paris"));
    const QDateTime early(QDate(2026, 11, 12), QTime(0, 30), QTimeZone("Europe/Paris"));
    const Report r;
    QVERIFY(formatText(r, "c", "1", "", late).startsWith(QStringLiteral("Calaos versions (2026-11-11)")));
    QVERIFY(formatText(r, "c", "1", "", early).startsWith(QStringLiteral("Calaos versions (2026-11-11)")));
}

void TstVersions::modelFillsFromTheReport()
{
    ManualFetcher f;
    VersionsModel model(QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"));
    QCOMPARE(model.get_qtVersion(), QString::fromLatin1(qVersion()));
    QVERIFY(!model.get_loading());
    model.refresh();
    QVERIFY(!model.get_loading());

    model.setFetcher(f.fetcher());
    model.refresh();
    QVERIFY(model.get_loading());
    QVERIFY(!model.get_loaded());
    QCOMPARE(f.pending.size(), 1);

    f.pending[0](fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }}));
    QVERIFY(!model.get_loading());
    QVERIFY(model.get_loaded());
    QCOMPARE(model.get_serverVersion(), QStringLiteral("4.8.0"));
    QVERIFY(model.get_systemAvailable());
    QCOMPARE(model.get_containerVersion(), QStringLiteral("1.9.1"));
    QCOMPARE(model.get_calaosOs(), QStringLiteral("v4.4.0"));
    QCOMPARE(model.get_debian(), QStringLiteral("13.1"));
    QCOMPARE(model.get_kernel(), QStringLiteral("6.12.48-amd64"));
    QCOMPARE(model.get_packages().size(), 3);
    QCOMPARE(model.get_packages()[0].toMap().value("name").toString(), QStringLiteral("calaos-container"));
    QCOMPARE(model.get_packages()[0].toMap().value("version").toString(), QStringLiteral("1.9.1"));
    QVERIFY(model.get_failure().isEmpty());
    QVERIFY(model.get_message().isEmpty());
}

void TstVersions::modelMessagePerFailure_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("failure");
    QTest::addColumn<QString>("words");
    QTest::newRow("old server") << "invalid_request" << "old_server" << "update calaos-server";
    QTest::newRow("old server, not_allowed") << "not_allowed" << "old_server" << "update calaos-server";
    QTest::newRow("unreachable") << "timeout" << "unreachable" << "did not answer";
}

void TstVersions::modelMessagePerFailure()
{
    QFETCH(QString, code);
    QFETCH(QString, failure);
    QFETCH(QString, words);
    ManualFetcher f;
    VersionsModel model(QStringLiteral("calaos-home android"), QStringLiteral("4.5.0"));
    model.setFetcher(f.fetcher());
    model.refresh();
    f.pending[0](fromRelayError(code, QStringLiteral("4.7.2")));
    QCOMPARE(model.get_failure(), failure);
    QVERIFY2(model.get_message().contains(words), qPrintable(model.get_message()));
    QCOMPARE(model.get_serverVersion(), QStringLiteral("4.7.2"));
    QVERIFY(!model.get_systemAvailable());

    model.refresh();
    f.pending[1](fromDaemon(false, 404, QJsonValue(), QStringLiteral("4.8.0")));
    QCOMPARE(model.get_failure(), QStringLiteral("daemon_too_old"));
    QVERIFY(model.get_message().contains(QStringLiteral("update calaos-container")));

    model.refresh();
    f.pending[2](fromDaemon(false, 401, QJsonValue(), QStringLiteral("4.8.0")));
    QCOMPARE(model.get_failure(), QStringLiteral("unauthorized"));
    QVERIFY(model.get_message().contains(QStringLiteral("authentication error")));
    QVERIFY(!model.get_message().contains(QStringLiteral("did not answer")));
}

void TstVersions::modelKeepsOnlyTheLastAnswer()
{
    ManualFetcher f;
    VersionsModel model(QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"));
    model.setFetcher(f.fetcher());
    model.refresh();
    model.refresh();
    QCOMPARE(f.pending.size(), 2);

    f.pending[1](fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }}));
    f.pending[0](fromRelayError(QStringLiteral("timeout"), QStringLiteral("4.7.0")));
    QCOMPARE(model.get_serverVersion(), QStringLiteral("4.8.0"));
    QVERIFY(model.get_systemAvailable());
    QVERIFY(!model.get_loading());

    //an answer after the model is gone is dropped
    model.refresh();
    VersionsModel::Done late = f.pending[2];
    {
        ManualFetcher g;
        VersionsModel gone(QStringLiteral("c"), QStringLiteral("1"));
        gone.setFetcher(g.fetcher());
        gone.refresh();
        late = g.pending[0];
    }
    late(fromRelayError(QStringLiteral("timeout"), QString()));
}

void TstVersions::modelReloadKeepsThePreviousValuesUntilTheAnswer()
{
    ManualFetcher f;
    VersionsModel model(QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"));
    model.setFetcher(f.fetcher());
    model.refresh();
    f.pending[0](fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }}));

    model.refresh();
    QVERIFY(model.get_loading());
    QVERIFY(model.get_loaded());
    QVERIFY(model.get_systemAvailable());

    f.pending[1](fromRelayError(QStringLiteral("timeout"), QStringLiteral("4.8.0")));
    QVERIFY(!model.get_systemAvailable());
    QVERIFY(model.get_containerVersion().isEmpty());
    QVERIFY(model.get_packages().isEmpty());
    QCOMPARE(model.get_failure(), QStringLiteral("unreachable"));
}

void TstVersions::modelCopiesTheText()
{
    ManualFetcher f;
    VersionsModel model(QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"));
    model.setFetcher(f.fetcher());
    model.refresh();
    f.pending[0](fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }}));

    const QString expected = formatText(fromRelayResult(QJsonObject{{ "server", "4.8.0" }, { "system", systemData() }}),
                                        QStringLiteral("calaos-home kiosk"), QStringLiteral("4.5.0"),
                                        QString::fromLatin1(qVersion()), NOW);
    QCOMPARE(model.reportTextAt(NOW), expected);

    model.copyToClipboard();
    const QString copied = QGuiApplication::clipboard()->text();
    //the date line is today's, the rest is the report
    QCOMPARE(copied.section(QLatin1Char('\n'), 1), expected.section(QLatin1Char('\n'), 1));
    QVERIFY(copied.startsWith(QStringLiteral("Calaos versions (")));
}

void TstVersions::kioskRoute_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<bool>("hasSystem");
    QTest::addColumn<int>("failure");
    QTest::newRow("answered") << 0 << true << int(Failure::None);
    QTest::newRow("daemon without the route") << 1 << false << int(Failure::DaemonTooOld);
    QTest::newRow("daemon without /api/v2") << 2 << false << int(Failure::DaemonTooOld);
    QTest::newRow("connection dropped") << 3 << false << int(Failure::Unreachable);
    QTest::newRow("daemon error") << 4 << false << int(Failure::Unreachable);
}

void TstVersions::kioskRoute()
{
    QFETCH(int, mode);
    QFETCH(bool, hasSystem);
    QFETCH(int, failure);

    FakeTimeDaemon daemon;
    daemon.token = QString::fromLatin1(TOKEN);
    QVERIFY(daemon.start());
    switch (mode)
    {
    case 0: daemon.script("GET", VERSIONS, 200, FakeTimeDaemon::success(systemData())); break;
    case 1: daemon.noTimeRoutes = true; break;
    case 2: daemon.oldDaemon = true; break;
    //Qt silently resends an idempotent GET once on a closed connection
    case 3: daemon.scriptDrop("GET", VERSIONS); daemon.scriptDrop("GET", VERSIONS); break;
    case 4: daemon.script("GET", VERSIONS, 500, FakeTimeDaemon::failure("internal", "dpkg-query failed")); break;
    }

    CalaosOsAPI api;
    api.setBaseAddr(daemon.baseAddr());
    api.setTokenFile(m_tokenPath);

    bool done = false;
    Report report;
    api.getSystemVersions([&](const CalaosOsReply &reply)
    {
        report = fromDaemon(reply.ok, reply.httpStatus, reply.data, QStringLiteral("4.8.0"));
        done = true;
    });
    QTRY_VERIFY(done);
    QCOMPARE(report.hasSystem, hasSystem);
    QCOMPARE(int(report.failure), failure);
    QVERIFY(!daemon.requestsTo("GET", QStringLiteral("/api/v2") + VERSIONS).isEmpty());
    if (hasSystem)
    {
        QCOMPARE(report.container, QStringLiteral("1.9.1"));
        QCOMPARE(report.packages.size(), 3);
    }
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstVersions test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_versions.moc"
