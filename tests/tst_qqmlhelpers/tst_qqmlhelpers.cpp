/*
 * Unit tests for the equality guard added to the model property macros of
 * src/qqmlhelpers.h (QML_READONLY_PROPERTY_MODEL / QML_WRITABLE_PROPERTY_MODEL).
 *
 * The contract under test has two halves, and they pull in opposite
 * directions - which is the whole reason this file exists:
 *
 *   1. A write whose value is unchanged must be silent: no name##Changed
 *      signal, no dataChanged() on the owning model. This is the point of
 *      the ticket - every server poll rewrites every IO property, and the
 *      redundant notifications used to re-evaluate QML bindings for nothing.
 *
 *   2. A write must NEVER be silent when it actually carries information.
 *      Two cases matter here and both have dedicated tests:
 *
 *      a. The FIRST write always goes through, even when the incoming value
 *         equals the default-constructed member. setData() is what creates
 *         the role on the QStandardItem; a role that was never written reads
 *         back as an INVALID QVariant, which QML sees as "undefined" instead
 *         of "" / false / 0. The production code populates items with default
 *         values before appendRow() all over the place (RoomItem's
 *         constructor, UpdatePackagesModel::load, ControlPanelModel::load),
 *         so a guard comparing values alone would drop those roles for good.
 *         -> firstWriteAtDefaultValueStillPrimesRole()
 *
 *      b. Floating point values are compared exactly, not fuzzily. A fuzzy
 *         compare would swallow the small steady increments of the audio
 *         playback position.
 *         -> doubleUsesExactComparison() / audioPositionKeepsScrolling()
 *
 * The probe classes below deliberately use the macros exactly the way the
 * production models do (QObject + QStandardItem, roles declared on the owning
 * QStandardItemModel), so the test exercises the generated code and not a
 * paraphrase of it.
 */

#include <QtTest>
#include <QSignalSpy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>

#include "Common.h"
#include "qqmlhelpers.h"

class GuardProbeModel: public QStandardItemModel
{
    Q_OBJECT

public:
    enum
    {
        RoleText = Qt::UserRole + 1,
        RoleFlag,
        RoleCount,
        RoleElapsed,
        RoleList,
        RoleStatus,
        RoleWritable,
    };

    explicit GuardProbeModel(QObject *parent = nullptr): QStandardItemModel(parent)
    {
        QHash<int, QByteArray> roles;
        roles[RoleText] = "probeText";
        roles[RoleFlag] = "probeFlag";
        roles[RoleCount] = "probeCount";
        roles[RoleElapsed] = "probeElapsed";
        roles[RoleList] = "probeList";
        roles[RoleStatus] = "probeStatus";
        roles[RoleWritable] = "probeWritable";
        setItemRoleNames(roles);
    }
};

class GuardProbeItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, probeText, GuardProbeModel::RoleText)
    QML_READONLY_PROPERTY_MODEL(bool, probeFlag, GuardProbeModel::RoleFlag)
    QML_READONLY_PROPERTY_MODEL(int, probeCount, GuardProbeModel::RoleCount)
    QML_READONLY_PROPERTY_MODEL(double, probeElapsed, GuardProbeModel::RoleElapsed)
    QML_READONLY_PROPERTY_MODEL(QStringList, probeList, GuardProbeModel::RoleList)
    QML_READONLY_PROPERTY_MODEL(Common::AudioStatusType, probeStatus, GuardProbeModel::RoleStatus)
    QML_WRITABLE_PROPERTY_MODEL(bool, probeWritable, GuardProbeModel::RoleWritable)

public:
    GuardProbeItem(): QStandardItem() {}
};

class TstQQmlHelpers: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void identicalWriteEmitsNothing();
    void differentWriteStillEmits();
    void identicalWriteEmitsNoDataChanged();

    void firstWriteAtDefaultValueStillPrimesRole();
    void firstWriteAtDefaultValueEmitsOnce();

    void doubleUsesExactComparison();
    void audioPositionKeepsScrolling();

    void enumIsGuarded();
    void stringListIsGuarded();

    void writablePropertyIsGuarded();

    void guardIsPerInstance();
};

void TstQQmlHelpers::initTestCase()
{
    //QSignalSpy needs to know how to copy the enum carried by probeStatusChanged.
    qRegisterMetaType<Common::AudioStatusType>("Common::AudioStatusType");
}

//A rewrite with an unchanged value must not notify anyone. This is the
//behaviour the ticket is about: the poll loop rewrites everything every time.
void TstQQmlHelpers::identicalWriteEmitsNothing()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeTextChanged);
    QVERIFY(spy.isValid());

    item->update_probeText(QStringLiteral("Living room"));
    QCOMPARE(spy.count(), 1);

    //Same value, five more times: still exactly one emission in total.
    for (int i = 0; i < 5; i++)
        item->update_probeText(QStringLiteral("Living room"));
    QCOMPARE(spy.count(), 1);

    //...and the stored value is of course untouched.
    QCOMPARE(item->get_probeText(), QStringLiteral("Living room"));
}

//The other half of the contract: a real change must always come through.
void TstQQmlHelpers::differentWriteStillEmits()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeTextChanged);

    item->update_probeText(QStringLiteral("Kitchen"));
    item->update_probeText(QStringLiteral("Kitchen"));   //silent
    item->update_probeText(QStringLiteral("Bathroom"));  //real change
    item->update_probeText(QStringLiteral("Bathroom"));  //silent
    item->update_probeText(QStringLiteral("Kitchen"));   //real change, back again

    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Kitchen"));
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("Bathroom"));
    QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("Kitchen"));
    QCOMPARE(item->get_probeText(), QStringLiteral("Kitchen"));
}

//The view side: no dataChanged() either, so delegates are not re-evaluated.
void TstQQmlHelpers::identicalWriteEmitsNoDataChanged()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(dataSpy.isValid());

    item->update_probeCount(3);
    const int afterFirst = dataSpy.count();
    QVERIFY(afterFirst > 0);

    item->update_probeCount(3);
    item->update_probeCount(3);
    QCOMPARE(dataSpy.count(), afterFirst);

    item->update_probeCount(4);
    QVERIFY(dataSpy.count() > afterFirst);
}

//Regression guard for the dangerous case. Production code builds an item,
//writes default values into it, and only then calls appendRow(). If the
//equality guard compared values alone, none of these writes would reach
//setData() and the roles would read back as undefined in QML forever.
void TstQQmlHelpers::firstWriteAtDefaultValueStillPrimesRole()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();

    //Written BEFORE appendRow, exactly like RoomItem::RoomItem() and friends,
    //and every single value is the default-constructed one.
    item->update_probeText(QString());
    item->update_probeFlag(false);
    item->update_probeCount(0);
    item->update_probeElapsed(0.0);
    item->update_probeList(QStringList());
    item->set_probeWritable(false);

    model.appendRow(item);

    const QModelIndex idx = model.index(0, 0);

    //The whole point: valid QVariants, not undefined.
    QVERIFY2(model.data(idx, GuardProbeModel::RoleText).isValid(),
             "RoleText was never written to the item: QML would see undefined");
    QVERIFY2(model.data(idx, GuardProbeModel::RoleFlag).isValid(),
             "RoleFlag was never written to the item: QML would see undefined");
    QVERIFY2(model.data(idx, GuardProbeModel::RoleCount).isValid(),
             "RoleCount was never written to the item: QML would see undefined");
    QVERIFY2(model.data(idx, GuardProbeModel::RoleElapsed).isValid(),
             "RoleElapsed was never written to the item: QML would see undefined");
    QVERIFY2(model.data(idx, GuardProbeModel::RoleList).isValid(),
             "RoleList was never written to the item: QML would see undefined");
    QVERIFY2(model.data(idx, GuardProbeModel::RoleWritable).isValid(),
             "RoleWritable was never written to the item: QML would see undefined");

    //And they carry the expected default payload.
    QCOMPARE(model.data(idx, GuardProbeModel::RoleText).toString(), QString());
    QCOMPARE(model.data(idx, GuardProbeModel::RoleFlag).toBool(), false);
    QCOMPARE(model.data(idx, GuardProbeModel::RoleCount).toInt(), 0);
    QCOMPARE(model.data(idx, GuardProbeModel::RoleWritable).toBool(), false);
}

//Same situation seen from the signal side: the priming write notifies once,
//so a binding established on the property is correct from the start.
void TstQQmlHelpers::firstWriteAtDefaultValueEmitsOnce()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeFlagChanged);

    item->update_probeFlag(false);   //default value, but first write
    QCOMPARE(spy.count(), 1);

    item->update_probeFlag(false);   //now genuinely redundant
    QCOMPARE(spy.count(), 1);

    item->update_probeFlag(true);
    QCOMPARE(spy.count(), 2);
}

//Doubles are compared with ==, not qFuzzyCompare. A fuzzy compare would treat
//a small genuine step as "no change" and freeze the audio progress bar.
void TstQQmlHelpers::doubleUsesExactComparison()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeElapsedChanged);

    item->update_probeElapsed(12.5);
    QCOMPARE(spy.count(), 1);

    item->update_probeElapsed(12.5);
    QCOMPARE(spy.count(), 1);

    //A difference far below qFuzzyCompare's tolerance must still notify.
    item->update_probeElapsed(12.5 + 1e-12);
    QVERIFY2(spy.count() == 2,
             "a tiny but real change was swallowed: the comparison is not exact");
}

//The scenario called out as the risk of this ticket: the audio playback
//position is rewritten on every poll. While playing it must keep scrolling;
//while paused it is genuinely unchanged and staying silent is correct.
void TstQQmlHelpers::audioPositionKeepsScrolling()
{
    GuardProbeModel model;
    GuardProbeItem *player = new GuardProbeItem();
    model.appendRow(player);

    QSignalSpy spy(player, &GuardProbeItem::probeElapsedChanged);

    //Playing: ten polls, position advances by one second each time.
    double position = 0.0;
    for (int poll = 0; poll < 10; poll++)
    {
        player->update_probeElapsed(position);
        position += 1.0;
    }
    QCOMPARE(spy.count(), 10);
    QCOMPARE(player->get_probeElapsed(), 9.0);

    //Paused: ten more polls at the very same position, nothing to report.
    for (int poll = 0; poll < 10; poll++)
        player->update_probeElapsed(9.0);
    QCOMPARE(spy.count(), 10);

    //Resumed: it scrolls again.
    player->update_probeElapsed(10.0);
    QCOMPARE(spy.count(), 11);
}

//Enum-typed model properties (ioType, evType, audio status) compare integrally.
void TstQQmlHelpers::enumIsGuarded()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeStatusChanged);

    item->update_probeStatus(Common::StatusPlay);
    QCOMPARE(spy.count(), 1);
    item->update_probeStatus(Common::StatusPlay);
    QCOMPARE(spy.count(), 1);
    item->update_probeStatus(Common::StatusPause);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(item->get_probeStatus(), Common::StatusPause);
}

//QStringList compares element-wise, so a reordered list is a real change.
void TstQQmlHelpers::stringListIsGuarded()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeListChanged);

    const QStringList a{QStringLiteral("calaos-base"), QStringLiteral("calaos-home")};

    item->update_probeList(a);
    QCOMPARE(spy.count(), 1);
    item->update_probeList(a);
    QCOMPARE(spy.count(), 1);

    //Same elements, different order: not equal, must notify.
    item->update_probeList(QStringList{QStringLiteral("calaos-home"), QStringLiteral("calaos-base")});
    QCOMPARE(spy.count(), 2);

    //One more element: must notify.
    item->update_probeList(QStringList{QStringLiteral("calaos-home"),
                                       QStringLiteral("calaos-base"),
                                       QStringLiteral("calaos-gui")});
    QCOMPARE(spy.count(), 3);
}

//The WRITE macro gets the same treatment as the READONLY one.
void TstQQmlHelpers::writablePropertyIsGuarded()
{
    GuardProbeModel model;
    GuardProbeItem *item = new GuardProbeItem();
    model.appendRow(item);

    QSignalSpy spy(item, &GuardProbeItem::probeWritableChanged);
    QSignalSpy dataSpy(&model, &QAbstractItemModel::dataChanged);

    item->set_probeWritable(true);
    QCOMPARE(spy.count(), 1);
    const int afterFirst = dataSpy.count();

    item->set_probeWritable(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(dataSpy.count(), afterFirst);

    item->set_probeWritable(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(dataSpy.count() > afterFirst);

    //Reachable through the Qt property system too, the way QML writes it.
    QVERIFY(item->setProperty("probeWritable", true));
    QCOMPARE(spy.count(), 3);
    QVERIFY(item->setProperty("probeWritable", true));
    QCOMPARE(spy.count(), 3);
}

//The primed flag is per object, so a fresh item (models are rebuilt with
//clear() + new items on every reload) always gets its roles written.
void TstQQmlHelpers::guardIsPerInstance()
{
    GuardProbeModel model;

    GuardProbeItem *first = new GuardProbeItem();
    first->update_probeText(QStringLiteral("Living room"));
    model.appendRow(first);

    //A full reload: clear() destroys the items, new ones take over.
    model.clear();

    GuardProbeItem *second = new GuardProbeItem();
    QSignalSpy spy(second, &GuardProbeItem::probeTextChanged);

    //Same value the previous item held - but this is a new object, so it is
    //a first write and must both notify and prime the role.
    second->update_probeText(QStringLiteral("Living room"));
    QCOMPARE(spy.count(), 1);

    model.appendRow(second);
    QVERIFY(model.data(model.index(0, 0), GuardProbeModel::RoleText).isValid());
    QCOMPARE(model.data(model.index(0, 0), GuardProbeModel::RoleText).toString(),
             QStringLiteral("Living room"));
}

QTEST_MAIN(TstQQmlHelpers)

#include "tst_qqmlhelpers.moc"
