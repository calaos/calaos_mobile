/*
 * Unit tests for CalaosConnection::detectHttpApiV2() (T18).
 *
 * The rule tested here is not new, it moved. RoomModel::load() used to read the
 * shape of every room it parsed and push the answer back into the transport
 * with connection->updateHttpApiV2(true/false): the model configured the
 * connection, which made the CalaosConnection <-> RoomModel dependency
 * bidirectional. The detection now lives next to the flag it sets, and runs
 * once on the whole get_home payload before homeLoaded() is emitted.
 *
 * What must not change:
 *   - a room whose "items" is a map carrying "inputs" means the v2 HTTP API,
 *   - a room whose "items" is a flat list means the older one,
 *   - a payload that says nothing (demo mode emits an empty home) must leave
 *     the current value alone, it must NOT reset it to false.
 *
 * Link perimeter: see tst_httpapiversion.pro - the header only, no transport.
 */

#include <QtTest>
#include <QVariantList>
#include <QVariantMap>

#include "CalaosConnection.h"

namespace
{

//A room in the v2 shape: items is a map with separate inputs/outputs.
QVariantMap roomV2()
{
    QVariantMap io;
    io[QStringLiteral("id")] = QStringLiteral("io_light");
    io[QStringLiteral("gui_type")] = QStringLiteral("light");

    QVariantMap items;
    items[QStringLiteral("inputs")] = QVariantList();
    items[QStringLiteral("outputs")] = QVariantList{ io };

    QVariantMap room;
    room[QStringLiteral("name")] = QStringLiteral("Salon");
    room[QStringLiteral("type")] = QStringLiteral("living");
    room[QStringLiteral("items")] = items;
    return room;
}

//The older shape: items is a single flat list.
QVariantMap roomV1()
{
    QVariantMap io;
    io[QStringLiteral("id")] = QStringLiteral("io_light");
    io[QStringLiteral("gui_type")] = QStringLiteral("light");

    QVariantMap room;
    room[QStringLiteral("name")] = QStringLiteral("Salon");
    room[QStringLiteral("type")] = QStringLiteral("living");
    room[QStringLiteral("items")] = QVariantList{ io };
    return room;
}

QVariantMap home(const QVariantList &rooms)
{
    QVariantMap h;
    h[QStringLiteral("home")] = rooms;
    return h;
}

} //namespace

class TstHttpApiVersion: public QObject
{
    Q_OBJECT

private slots:
    void inputsMapMeansV2();
    void flatItemsListMeansV1();
    void roomWithoutItemsMeansV1();
    void oneV2RoomIsEnough();
    void emptyPayloadKeepsCurrentValue();
    void emptyPayloadKeepsCurrentValue_data();
};

//The v2 marker: "items" is a map holding an "inputs" entry.
void TstHttpApiVersion::inputsMapMeansV2()
{
    bool v2 = false;
    QVERIFY(CalaosConnection::detectHttpApiV2(home({ roomV2() }), v2));
    QCOMPARE(v2, true);
}

//Before v2 a room carried one flat "items" list, read as both inputs and
//outputs by RoomModel::load().
void TstHttpApiVersion::flatItemsListMeansV1()
{
    bool v2 = true;
    QVERIFY(CalaosConnection::detectHttpApiV2(home({ roomV1() }), v2));
    QCOMPARE(v2, false);
}

//A room with no "items" at all is not the v2 shape either - same reading as
//RoomModel::load(), which fell into its else branch for it.
void TstHttpApiVersion::roomWithoutItemsMeansV1()
{
    QVariantMap room;
    room[QStringLiteral("name")] = QStringLiteral("Empty");

    bool v2 = true;
    QVERIFY(CalaosConnection::detectHttpApiV2(home({ room }), v2));
    QCOMPARE(v2, false);
}

//A single answer never mixes both shapes, but the decision must not depend on
//which room happens to come last: one v2 room settles it.
void TstHttpApiVersion::oneV2RoomIsEnough()
{
    QVariantMap noItems;
    noItems[QStringLiteral("name")] = QStringLiteral("Empty");

    bool v2 = false;
    QVERIFY(CalaosConnection::detectHttpApiV2(home({ roomV2(), noItems }), v2));
    QCOMPARE(v2, true);

    v2 = false;
    QVERIFY(CalaosConnection::detectHttpApiV2(home({ noItems, roomV2() }), v2));
    QCOMPARE(v2, true);
}

void TstHttpApiVersion::emptyPayloadKeepsCurrentValue_data()
{
    QTest::addColumn<QVariantMap>("payload");
    QTest::addColumn<bool>("current");

    //Demo mode emits homeLoaded({}).
    QTest::newRow("empty map, currently v2") << QVariantMap() << true;
    QTest::newRow("empty map, currently v1") << QVariantMap() << false;
    QTest::newRow("no room, currently v2") << home({}) << true;
    QTest::newRow("no room, currently v1") << home({}) << false;
    QTest::newRow("only empty rooms") << home({ QVariantMap() }) << true;
}

//Nothing to read must mean "keep what we have", never "assume v1": an empty
//home used to leave the flag untouched because RoomModel::load() was simply
//never called for it.
void TstHttpApiVersion::emptyPayloadKeepsCurrentValue()
{
    QFETCH(QVariantMap, payload);
    QFETCH(bool, current);

    bool v2 = current;
    QVERIFY(!CalaosConnection::detectHttpApiV2(payload, v2));
    QCOMPARE(v2, current);
}

QTEST_APPLESS_MAIN(TstHttpApiVersion)
#include "tst_httpapiversion.moc"
