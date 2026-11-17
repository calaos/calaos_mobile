#include <QtTest>
#include <QJsonArray>
#include <QSignalSpy>

#include "ProgramClient.h"
#include "CalaosConnection.h"

class FakeTransport: public QObject, public ProgramTransport
{
    Q_OBJECT
public:
    QString sendTrackedRequest(const QString &action, QJsonObject data) override
    {
        if (down)
            return QString();
        const QString id = QStringLiteral("req:%1").arg(++seq);
        sent << QVariantMap{ { "id", id }, { "action", action }, { "data", data.toVariantMap() } };
        return id;
    }
    void cancelTrackedRequest(const QString &requestId) override { cancelled << requestId; }

    bool down = false;
    int seq = 0;
    QVariantList sent;
    QStringList cancelled;

signals:
    void trackedRequestFinished(const QString &requestId, bool transportOk, const QVariantMap &answer);
    void eventTimeRangeChange(QString id);
};

class TstProgramClient: public QObject
{
    Q_OBJECT

private slots:
    void requestShapes();
    void answers_data();
    void answers();
    void notSentWithoutSession();
    void timeoutFinishesOnceAndCancels();
    void lateAndForeignAnswersAreDropped();
    void timeRangeChangeIsForwarded();
    void connectionWithoutSessionSendsNothing();
};

void TstProgramClient::requestShapes()
{
    FakeTransport t;
    ProgramClient c(&t, &t);

    const QVariantList ranges{ QVariantMap{ { "day", "1" }, { "start_hour", "6" } } };
    QVERIFY(!c.setTimeRange("h1", ranges).isEmpty());
    c.getTimeRange("h1");
    c.getStates({ "a", "b" });
    c.setState("sp", "20.5");

    QCOMPARE(t.sent.size(), 4);
    QVariantMap s = t.sent.at(0).toMap();
    QCOMPARE(s["action"].toString(), QStringLiteral("set_timerange"));
    QVariantMap d = s["data"].toMap();
    QCOMPARE(d["id"].toString(), QStringLiteral("h1"));
    QCOMPARE(d["months"].toString(), QStringLiteral("111111111111"));
    QCOMPARE(d["ranges"].toList(), ranges);

    QCOMPARE(t.sent.at(1).toMap()["action"].toString(), QStringLiteral("get_timerange"));
    d = t.sent.at(2).toMap()["data"].toMap();
    QCOMPARE(t.sent.at(2).toMap()["action"].toString(), QStringLiteral("get_state"));
    QCOMPARE(d["items"].toStringList(), QStringList({ "a", "b" }));
    d = t.sent.at(3).toMap()["data"].toMap();
    QCOMPARE(t.sent.at(3).toMap()["action"].toString(), QStringLiteral("set_state"));
    QCOMPARE(d["value"].toString(), QStringLiteral("20.5"));

    //An empty list is an explicit "clear everything", it must be sent.
    c.setTimeRange("h2", QVariantList());
    QVERIFY(t.sent.last().toMap()["data"].toMap().contains("ranges"));
}

void TstProgramClient::answers_data()
{
    QTest::addColumn<QString>("call");
    QTest::addColumn<bool>("transportOk");
    QTest::addColumn<QVariantMap>("answer");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QString>("error");

    const QVariantMap ranges{ { "months", "111111111111" }, { "ranges", QVariantList() } };
    QTest::newRow("get ok") << "get" << true << ranges << true << "";
    QTest::newRow("get wrong input") << "get" << true << QVariantMap{ { "error", "wrong input" } } << false << "wrong input";
    QTest::newRow("get without ranges") << "get" << true << QVariantMap{ { "months", "1" } } << false << "invalid ranges";
    QTest::newRow("get transport") << "get" << false << QVariantMap() << false << "transport";
    QTest::newRow("set ok") << "set" << true << QVariantMap{ { "success", "true" } } << true << "";
    QTest::newRow("set invalid range") << "set" << true << QVariantMap{ { "error", "invalid range" } } << false << "invalid range";
    QTest::newRow("set not written") << "set" << true << QVariantMap{ { "error", "config_not_written" } } << false << "config_not_written";
    QTest::newRow("set scope denied") << "set" << true << QVariantMap{ { "error", "scope denied" } } << false << "scope denied";
    QTest::newRow("set_state false") << "state" << true << QVariantMap{ { "success", "false" } } << false << "failed";
    QTest::newRow("set_state ok") << "state" << true << QVariantMap{ { "success", "true" } } << true << "";
    QTest::newRow("get_state ok") << "states" << true << QVariantMap{ { "sp1", "21" } } << true << "";
}

void TstProgramClient::answers()
{
    QFETCH(QString, call);
    QFETCH(bool, transportOk);
    QFETCH(QVariantMap, answer);
    QFETCH(bool, ok);
    QFETCH(QString, error);

    FakeTransport t;
    ProgramClient c(&t, &t);
    QSignalSpy spy(&c, &ProgramClient::finished);

    QString id;
    if (call == "get") id = c.getTimeRange("h1");
    else if (call == "set") id = c.setTimeRange("h1", QVariantList());
    else if (call == "state") id = c.setState("sp", "21");
    else id = c.getStates({ "sp1" });

    emit t.trackedRequestFinished(id, transportOk, answer);
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), id);
    QCOMPARE(spy.at(0).at(1).toBool(), ok);
    QCOMPARE(spy.at(0).at(3).toString(), error);
    if (ok)
        QCOMPARE(spy.at(0).at(2).toMap(), answer);
    QCOMPARE(c.pendingRequests(), 0);
}

void TstProgramClient::notSentWithoutSession()
{
    FakeTransport t;
    t.down = true;
    ProgramClient c(&t, &t);
    QSignalSpy spy(&c, &ProgramClient::finished);
    QVERIFY(c.setTimeRange("h1", QVariantList()).isEmpty());
    QCOMPARE(c.pendingRequests(), 0);
    QCOMPARE(spy.size(), 0);
}

void TstProgramClient::timeoutFinishesOnceAndCancels()
{
    FakeTransport t;
    ProgramClient c(&t, &t);
    c.setTimeoutMs(20);
    QSignalSpy spy(&c, &ProgramClient::finished);
    const QString id = c.getTimeRange("h1");
    QTRY_COMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(3).toString(), QStringLiteral("timeout"));
    QCOMPARE(t.cancelled, QStringList{ id });

    emit t.trackedRequestFinished(id, true, QVariantMap{ { "ranges", QVariantList() } });
    QCOMPARE(spy.size(), 1);
}

void TstProgramClient::lateAndForeignAnswersAreDropped()
{
    FakeTransport t;
    ProgramClient c(&t, &t);
    QSignalSpy spy(&c, &ProgramClient::finished);
    const QString id = c.setTimeRange("h1", QVariantList());
    emit t.trackedRequestFinished(QStringLiteral("req:999"), true, QVariantMap{ { "success", "true" } });
    QCOMPARE(spy.size(), 0);
    emit t.trackedRequestFinished(id, true, QVariantMap{ { "success", "true" } });
    emit t.trackedRequestFinished(id, true, QVariantMap{ { "success", "true" } });
    QCOMPARE(spy.size(), 1);
}

void TstProgramClient::timeRangeChangeIsForwarded()
{
    FakeTransport t;
    ProgramClient c(&t, &t);
    QSignalSpy spy(&c, &ProgramClient::timeRangeChanged);
    emit t.eventTimeRangeChange(QStringLiteral("cool_level2"));
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("cool_level2"));
}

void TstProgramClient::connectionWithoutSessionSendsNothing()
{
    CalaosConnection con;
    ProgramClient c(&con, &con);
    QVERIFY(c.getTimeRange("h1").isEmpty());
    QCOMPARE(con.pendingTrackedRequests(), 0);
}

QTEST_MAIN(TstProgramClient)
#include "tst_programclient.moc"
