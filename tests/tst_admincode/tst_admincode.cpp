/*
 * Encrypted admin code push (calaos_base docs/08_http_api.md, "Code push
 * chiffré").
 *
 * The three golden vectors of the contract are replayed byte for byte, every
 * case the contract lists as "to refuse" is refused, and the display time is
 * counted from the reception on the phone, never from the box's expires.
 * AdminCodeModel runs over the in-memory stash of the base HardwareUtils with
 * clocks driven by the test.
 */

#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>

#include "AdminCode.h"
#include "HardwareUtils.h"

using namespace AdminCode;

namespace
{

QStringList s_logLines;
QtMessageHandler s_previousHandler = nullptr;

void captureMessages(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    s_logLines << msg;
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}

struct Vector
{
    const char *key;
    const char *codeId;
    const char *nonce;
    const char *code;
    const char *action;
    const char *expires;
    const char *ks6;
    const char *ct;
    const char *tag;
};

//docs/08_http_api.md, "Vecteurs de test".
const Vector kVectors[] = {
    { "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "0123456789abcdef",
      "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", "482913", "console_open", "2026-10-25T09:13:03Z",
      "b10aebd59d39", "8532d9ecac0a", "9e72bd26d3926652bc157819dd26d995" },
    { "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", "fedcba9876543210",
      "00112233445566778899aabbccddeeff", "007351", "network_config", "2026-10-25T14:00:59Z",
      "87e90db85a5c", "b7d93a8b6f6d", "f1bc83f4bdfbe0e9b6a47a65cb29248d" },
    { "a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5", "5d0c8e1f2a3b4c6d",
      "ffeeddccbbaa99887766554433221100", "999999", "approver_confirm", "2026-12-31T23:59:59Z",
      "f5e5fbf8e244", "ccdcc2c1db7d", "0a8b6d66ac8eb1bac575ad2505da6083" },
};

QByteArray keyOf(int v) { return QByteArray::fromHex(kVectors[v].key); }

//The data of vector 1 exactly as the contract shows it pushed.
const char kVector1Data[] =
    R"({"admin_code_id": "0123456789abcdef", "admin_code_action": "console_open",
 "admin_code_expires": "2026-10-25T09:13:03Z", "admin_code_nonce": "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
 "admin_code_ct": "8532d9ecac0a", "admin_code_tag": "9e72bd26d3926652bc157819dd26d995"})";

Entry entryOf(int v, qint64 receivedAt = 1000, qint64 receivedMono = 500)
{
    const Vector &x = kVectors[v];
    Entry e;
    e.codeId = QString::fromLatin1(x.codeId);
    e.action = QString::fromLatin1(x.action);
    e.expires = QString::fromLatin1(x.expires);
    e.nonce = QString::fromLatin1(x.nonce);
    e.ct = QString::fromLatin1(x.ct);
    e.tag = QString::fromLatin1(x.tag);
    e.receivedAt = receivedAt;
    e.receivedMono = receivedMono;
    return e;
}

//Flips one bit of the byte at index of a hex string.
QString flipBit(const QString &hex, int index)
{
    QByteArray b = QByteArray::fromHex(hex.toLatin1());
    b[index] = char(b.at(index) ^ 0x01);
    return QString::fromLatin1(b.toHex());
}

//A platform whose key store answers only once it is ready (iOS before the
//first unlock, a Keystore error).
class LateKeyHw: public HardwareUtils
{
public:
    LateKeyHw(): HardwareUtils(nullptr) {}
    void loadPushKey() override
    {
        loads++;
        if (storeReady)
            setPushKey(keyOf(0));
    }
    bool storeReady = false;
    int loads = 0;
};

} //namespace

class TstAdminCode: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    //Construction.
    void goldenVectors_data();
    void goldenVectors();
    void vector1AsPushedOpens();
    void tamperedVector1IsRefused_data();
    void tamperedVector1IsRefused();
    void malformedIsRefusedBeforeTheTag_data();
    void malformedIsRefusedBeforeTheTag();
    void wrongKeySizeIsRefused();
    void constantTimeEquals();

    //Expiry from the reception.
    void expiryCountsFromReception();
    void phoneClockFarAheadOfTheBoxStillShows();
    void wallClockSetBackDrops();
    void rebootDrops();
    void withoutBootClockUsesTheWallClock();
    void entryRoundTrip();

    //AdminCodeModel.
    void modelWaitsUntilReady();
    void modelShowsNewestFirst();
    void modelDropsExpiredAndForged();
    void modelCountdownThenExpired();
    void modelCloseConsumes();
    void modelRefreshOnNotification();
    void modelWithoutKeyKeepsTheStash();
    void modelLoadsTheKeyWhenTheStoreIsBack();
    void copyClearsTheClipboardAfterItsTime();
    void copyLeavesSomethingElseAlone();

    void cleanupTestCase();

private:
    static HardwareUtils *hw() { return HardwareUtils::Instance(); }
    void clearStash();
    void stash(const Entry &e) { hw()->stashAdminCode(e.toJson()); }
    QStringList stashedIds();

    qint64 wall = 0;
    qint64 mono = 0;
};

void TstAdminCode::initTestCase()
{
    s_previousHandler = qInstallMessageHandler(captureMessages);
    QCoreApplication::setOrganizationName(QStringLiteral("CalaosTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_admincode"));
}

void TstAdminCode::init()
{
    clearStash();
    hw()->setPushKey(keyOf(0));
    //Any date: only the phone's own clocks matter.
    wall = 1'800'000'000'000;
    mono = 3'600'000;
}

void TstAdminCode::clearStash()
{
    QStringList ids = stashedIds();
    hw()->removeAdminCodes(ids);
}

QStringList TstAdminCode::stashedIds()
{
    QStringList ids;
    for (const QJsonObject &o: hw()->adminCodeStash())
        ids << o.value(QStringLiteral("code_id")).toString();
    ids.sort();
    return ids;
}

// ---------------------------------------------------------------- construction

void TstAdminCode::goldenVectors_data()
{
    QTest::addColumn<int>("v");
    QTest::newRow("vector 1") << 0;
    QTest::newRow("vector 2") << 1;
    QTest::newRow("vector 3") << 2;
}

void TstAdminCode::goldenVectors()
{
    QFETCH(int, v);
    const Vector &x = kVectors[v];
    const QByteArray key = keyOf(v);
    const QByteArray nonce = QByteArray::fromHex(x.nonce);

    const QByteArray ks = keystream(key, x.codeId, nonce);
    QCOMPARE(ks.toHex(), QByteArray(x.ks6));

    QByteArray ct = QByteArray(x.code);
    for (int i = 0; i < 6; i++)
        ct[i] = char(ct.at(i) ^ ks.at(i));
    QCOMPARE(ct.toHex(), QByteArray(x.ct));

    QCOMPARE(tag(key, x.codeId, nonce, x.action, x.expires, ct).toHex(), QByteArray(x.tag));

    const Entry e = entryOf(v);
    QVERIFY(isWellFormed(e));
    QCOMPARE(open(key, e), QString::fromLatin1(x.code));
}

void TstAdminCode::vector1AsPushedOpens()
{
    QJsonParseError err;
    const QJsonObject data = QJsonDocument::fromJson(kVector1Data, &err).object();
    QCOMPARE(err.error, QJsonParseError::NoError);

    const QJsonObject stashed = entryFromPush(data, wall, mono, QStringLiteral("req-1"));
    const Entry e = Entry::fromJson(stashed);
    QCOMPARE(e.codeId, QStringLiteral("0123456789abcdef"));
    QCOMPARE(e.receivedAt, wall);
    QCOMPARE(e.receivedMono, mono);
    QCOMPARE(e.notifId, QStringLiteral("req-1"));
    QCOMPARE(open(keyOf(0), e), QStringLiteral("482913"));
}

void TstAdminCode::tamperedVector1IsRefused_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("value");
    QTest::addColumn<bool>("otherKey");

    const Entry e = entryOf(0);
    QString tagChar = e.tag;
    tagChar[5] = tagChar.at(5) == QLatin1Char('0') ? QLatin1Char('1') : QLatin1Char('0');

    //The cases docs/08_http_api.md lists, plus the last byte of each field.
    QTest::newRow("one tag character") << "tag" << tagChar << false;
    QTest::newRow("one ct bit") << "ct" << flipBit(e.ct, 0) << false;
    QTest::newRow("last ct bit") << "ct" << flipBit(e.ct, 5) << false;
    QTest::newRow("nonce") << "nonce" << flipBit(e.nonce, 15) << false;
    QTest::newRow("code_id") << "code_id" << QStringLiteral("0123456789abcdee") << false;
    QTest::newRow("action") << "action" << QStringLiteral("network_config") << false;
    QTest::newRow("expires") << "expires" << QStringLiteral("2026-10-25T09:13:04Z") << false;
    QTest::newRow("key of vector 3") << "" << QString() << true;
}

void TstAdminCode::tamperedVector1IsRefused()
{
    QFETCH(QString, field);
    QFETCH(QString, value);
    QFETCH(bool, otherKey);

    Entry e = entryOf(0);
    if (field == QLatin1String("tag")) e.tag = value;
    else if (field == QLatin1String("ct")) e.ct = value;
    else if (field == QLatin1String("nonce")) e.nonce = value;
    else if (field == QLatin1String("code_id")) e.codeId = value;
    else if (field == QLatin1String("action")) e.action = value;
    else if (field == QLatin1String("expires")) e.expires = value;

    //Shapes still valid: the tag is what refuses it.
    QVERIFY(isWellFormed(e));
    QCOMPARE(open(otherKey ? keyOf(2) : keyOf(0), e), QString());
}

void TstAdminCode::malformedIsRefusedBeforeTheTag_data()
{
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("value");

    const Entry e = entryOf(0);
    QTest::newRow("unknown action") << "action" << QStringLiteral("shell_open");
    QTest::newRow("empty action") << "action" << QString();
    QTest::newRow("expires with an offset") << "expires" << QStringLiteral("2026-10-25T09:13:03+00:00");
    QTest::newRow("expires with a fraction") << "expires" << QStringLiteral("2026-10-25T09:13:03.0Z");
    QTest::newRow("expires lowercase z") << "expires" << QStringLiteral("2026-10-25T09:13:03z");
    QTest::newRow("expires trailing") << "expires" << QStringLiteral("2026-10-25T09:13:03Z\n");
    QTest::newRow("uppercase code_id") << "code_id" << e.codeId.toUpper();
    QTest::newRow("short code_id") << "code_id" << e.codeId.left(15);
    QTest::newRow("uppercase nonce") << "nonce" << e.nonce.toUpper();
    QTest::newRow("long nonce") << "nonce" << e.nonce + QStringLiteral("00");
    QTest::newRow("uppercase ct") << "ct" << e.ct.toUpper();
    QTest::newRow("short ct") << "ct" << e.ct.left(10);
    QTest::newRow("non-hex tag") << "tag" << QString(e.tag).replace(0, 1, QStringLiteral("g"));
    QTest::newRow("uppercase tag") << "tag" << e.tag.toUpper();
}

void TstAdminCode::malformedIsRefusedBeforeTheTag()
{
    QFETCH(QString, field);
    QFETCH(QString, value);

    Entry e = entryOf(0);
    if (field == QLatin1String("tag")) e.tag = value;
    else if (field == QLatin1String("ct")) e.ct = value;
    else if (field == QLatin1String("nonce")) e.nonce = value;
    else if (field == QLatin1String("code_id")) e.codeId = value;
    else if (field == QLatin1String("action")) e.action = value;
    else if (field == QLatin1String("expires")) e.expires = value;

    QVERIFY(!isWellFormed(e));
    QCOMPARE(open(keyOf(0), e), QString());
}

void TstAdminCode::wrongKeySizeIsRefused()
{
    QCOMPARE(open(QByteArray(), entryOf(0)), QString());
    QCOMPARE(open(keyOf(0).left(31), entryOf(0)), QString());
    QCOMPARE(open(keyOf(0) + '\0', entryOf(0)), QString());
}

void TstAdminCode::constantTimeEquals()
{
    QVERIFY(AdminCode::constantTimeEquals(QByteArray("abc"), QByteArray("abc")));
    QVERIFY(!AdminCode::constantTimeEquals(QByteArray("abc"), QByteArray("abd")));
    QVERIFY(!AdminCode::constantTimeEquals(QByteArray("abc"), QByteArray("ab")));
    QVERIFY(AdminCode::constantTimeEquals(QByteArray(), QByteArray()));
}

// ---------------------------------------------------------------- expiry

void TstAdminCode::expiryCountsFromReception()
{
    const Entry e = entryOf(0, wall, mono);
    QCOMPARE(remainingMs(e, wall, mono), LifetimeMs);
    QCOMPARE(remainingMs(e, wall + 59'999, mono + 59'999), qint64(1));
    QVERIFY(remainingMs(e, wall + 60'000, mono + 60'000) <= 0);
    //The longer of the two ages wins.
    QCOMPARE(remainingMs(e, wall + 10'000, mono + 20'000), qint64(40'000));
    QCOMPARE(remainingMs(e, wall + 20'000, mono + 10'000), qint64(40'000));
    //A small NTP step back is tolerated.
    QCOMPARE(remainingMs(e, wall - 1'000, mono + 5'000), qint64(55'000));
}

void TstAdminCode::phoneClockFarAheadOfTheBoxStillShows()
{
    //Vector 1 expires on 2026-10-25T09:13:03Z by the box clock; the phone
    //thinks it is 2030: shown for a minute anyway, then dropped.
    const qint64 in2030 = QDateTime(QDate(2030, 6, 1), QTime(12, 0), QTimeZone::UTC).toMSecsSinceEpoch();
    const Entry e = entryOf(0, in2030, mono);
    QCOMPARE(remainingMs(e, in2030 + 1'000, mono + 1'000), qint64(59'000));
    QVERIFY(remainingMs(e, in2030 + 61'000, mono + 61'000) <= 0);

    //And far behind it (1990): same.
    const qint64 in1990 = QDateTime(QDate(1990, 1, 1), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();
    QCOMPARE(remainingMs(entryOf(0, in1990, mono), in1990 + 30'000, mono + 30'000), qint64(30'000));
}

void TstAdminCode::wallClockSetBackDrops()
{
    const Entry e = entryOf(0, wall, mono);
    QVERIFY(remainingMs(e, wall - 3'600'000, mono + 1'000) <= 0);
}

void TstAdminCode::rebootDrops()
{
    const Entry e = entryOf(0, wall, mono);
    //Boot clock restarted below the stamp.
    QVERIFY(remainingMs(e, wall + 30'000, 1'000) <= 0);
    //Restarted above it, but the wall clock knows more than a minute passed.
    QVERIFY(remainingMs(e, wall + 120'000, mono + 10'000) <= 0);
}

void TstAdminCode::withoutBootClockUsesTheWallClock()
{
    const Entry e = entryOf(0, wall, -1);
    QCOMPARE(remainingMs(e, wall + 15'000, 0), qint64(45'000));
    QVERIFY(remainingMs(e, wall + 60'000, 0) <= 0);
    QVERIFY(remainingMs(entryOf(0, 0, mono), wall, mono) <= 0);
}

void TstAdminCode::entryRoundTrip()
{
    Entry e = entryOf(1, wall, mono);
    e.notifId = QStringLiteral("abc");
    const Entry back = Entry::fromJson(QJsonDocument::fromJson(
        QJsonDocument(e.toJson()).toJson()).object());
    QCOMPARE(back.codeId, e.codeId);
    QCOMPARE(back.expires, e.expires);
    QCOMPARE(back.receivedAt, wall);
    QCOMPARE(back.receivedMono, mono);
    QCOMPARE(back.notifId, e.notifId);
    QCOMPARE(open(keyOf(1), back), QStringLiteral("007351"));
}

// ---------------------------------------------------------------- model

void TstAdminCode::modelWaitsUntilReady()
{
    stash(entryOf(0, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    QSignalSpy show(&m, &AdminCodeModel::showRequested);

    m.refresh();
    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(show.count(), 0);

    m.setReady();
    QCOMPARE(m.codes().size(), 1);
    QCOMPARE(show.count(), 1);
    const QVariantMap c = m.codes().at(0).toMap();
    QCOMPARE(c.value("code").toString(), QStringLiteral("482913"));
    QCOMPARE(c.value("action").toString(), QStringLiteral("console_open"));
    QCOMPARE(c.value("remaining").toInt(), 60);
    QCOMPARE(c.value("expired").toBool(), false);

    //Read again: nothing new, no second opening.
    m.refresh();
    QCOMPARE(m.codes().size(), 1);
    QCOMPARE(show.count(), 1);
    //Still stashed until consumed: a killed app shows it again.
    QCOMPARE(stashedIds(), QStringList { QStringLiteral("0123456789abcdef") });
}

void TstAdminCode::modelShowsNewestFirst()
{
    //Vectors 1 and 2 share the key.
    stash(entryOf(0, wall - 20'000, mono - 20'000));
    stash(entryOf(1, wall - 5'000, mono - 5'000));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();

    const QVariantList codes = m.codes();
    QCOMPARE(codes.size(), 2);
    QCOMPARE(codes.at(0).toMap().value("code").toString(), QStringLiteral("007351"));
    QCOMPARE(codes.at(0).toMap().value("remaining").toInt(), 55);
    QCOMPARE(codes.at(1).toMap().value("code").toString(), QStringLiteral("482913"));
    QCOMPARE(codes.at(1).toMap().value("remaining").toInt(), 40);

    //A third arriving later goes on top.
    Entry third = entryOf(0, wall, mono);
    third.codeId = QStringLiteral("1111111111111111");
    //Forged on purpose (same tag under another id): never shown.
    stash(third);
    QSignalSpy show(&m, &AdminCodeModel::showRequested);
    QVERIFY(!m.unreadable());
    m.refresh();
    QCOMPARE(m.codes().size(), 2);
    //Told, without any value: the sheet says a code could not be read.
    QCOMPARE(show.count(), 1);
    QVERIFY(m.unreadable());
    m.close();
    QVERIFY(!m.unreadable());
}

void TstAdminCode::modelDropsExpiredAndForged()
{
    Entry forged = entryOf(1, wall, mono);
    forged.ct = flipBit(forged.ct, 2);
    Entry malformed = entryOf(0, wall, mono);
    malformed.codeId = QStringLiteral("2222222222222222");
    malformed.action = QStringLiteral("rm_rf");
    stash(entryOf(0, wall - 61'000, mono - 61'000)); //expired
    stash(forged);
    stash(malformed);
    QCOMPARE(stashedIds().size(), 3);

    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    QSignalSpy show(&m, &AdminCodeModel::showRequested);
    m.setReady();

    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(show.count(), 1);
    QVERIFY(m.unreadable());
    QCOMPARE(stashedIds(), QStringList());
}

void TstAdminCode::modelCountdownThenExpired()
{
    stash(entryOf(0, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();
    QSignalSpy changed(&m, &AdminCodeModel::codesChanged);

    wall += 1'000; mono += 1'000;
    m.tick();
    QCOMPARE(m.codes().at(0).toMap().value("remaining").toInt(), 59);
    QVERIFY(qAbs(m.codes().at(0).toMap().value("fraction").toDouble() - 59.0 / 60.0) < 1e-9);

    wall += 58'500; mono += 58'500;
    m.tick();
    QCOMPARE(m.codes().at(0).toMap().value("remaining").toInt(), 1);
    QCOMPARE(m.codes().at(0).toMap().value("expired").toBool(), false);

    wall += 500; mono += 500;
    m.tick();
    const QVariantMap c = m.codes().at(0).toMap();
    QCOMPARE(c.value("expired").toBool(), true);
    QCOMPARE(c.value("code").toString(), QString());
    QCOMPARE(c.value("remaining").toInt(), 0);
    QVERIFY(changed.count() >= 3);
    //Gone from the stash at expiry, still listed until the sheet closes.
    QCOMPARE(stashedIds(), QStringList());
    QCOMPARE(m.codes().size(), 1);
    m.close();
    QCOMPARE(m.codes().size(), 0);
}

void TstAdminCode::modelCloseConsumes()
{
    stash(entryOf(0, wall, mono));
    stash(entryOf(1, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();
    QCOMPARE(m.codes().size(), 2);

    m.close();
    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(stashedIds(), QStringList());

    //Not shown again by a later read.
    QSignalSpy show(&m, &AdminCodeModel::showRequested);
    m.refresh();
    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(show.count(), 0);
}

void TstAdminCode::modelRefreshOnNotification()
{
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });

    //Before the UI is up: kept for later.
    stash(entryOf(0, wall, mono));
    hw()->handleAdminCodeNotified(QStringLiteral("0123456789abcdef"));
    QVERIFY(hw()->hasPendingAdminCode());
    QCOMPARE(m.codes().size(), 0);

    QSignalSpy show(&m, &AdminCodeModel::showRequested);
    m.setReady();
    QVERIFY(!hw()->hasPendingAdminCode());
    QCOMPARE(show.count(), 1);

    //After: read right away.
    stash(entryOf(1, wall, mono));
    hw()->handleAdminCodeNotified(QString());
    QCOMPARE(show.count(), 2);
    QCOMPARE(m.codes().size(), 2);
    QVERIFY(!hw()->hasPendingAdminCode());
    m.close();
}

void TstAdminCode::modelWithoutKeyKeepsTheStash()
{
    //Key not available yet: nothing is decrypted, nothing is thrown away.
    hw()->setPushKey(QByteArray());
    stash(entryOf(0, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    QSignalSpy show(&m, &AdminCodeModel::showRequested);
    m.setReady();
    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(show.count(), 0);
    QVERIFY(!m.unreadable());
    QCOMPARE(stashedIds(), QStringList { QStringLiteral("0123456789abcdef") });

    //The key is back: the next notification shows the code.
    hw()->setPushKey(keyOf(0));
    hw()->handleAdminCodeNotified(QString());
    QCOMPARE(show.count(), 1);
    QCOMPARE(m.codes().at(0).toMap().value("code").toString(), QStringLiteral("482913"));
    m.close();
}

void TstAdminCode::modelLoadsTheKeyWhenTheStoreIsBack()
{
    LateKeyHw late;
    late.stashAdminCode(entryOf(1, wall, mono).toJson());
    AdminCodeModel m(&late);
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();
    QCOMPARE(late.loads, 1);
    QCOMPARE(m.codes().size(), 0);
    QCOMPARE(late.adminCodeStash().size(), 1);

    //Store unlocked; on the next read (foreground, notification) the key is
    //loaded again and the code shown.
    late.storeReady = true;
    late.handleAdminCodeNotified(QString());
    QCOMPARE(late.loads, 2);
    QCOMPARE(m.codes().size(), 1);
    QCOMPARE(m.codes().at(0).toMap().value("code").toString(), QStringLiteral("007351"));

    //Loaded once and for all.
    m.refresh();
    QCOMPARE(late.loads, 2);
    m.close();
}

void TstAdminCode::copyClearsTheClipboardAfterItsTime()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);
    stash(entryOf(0, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();

    m.copy(QStringLiteral("0123456789abcdef"));
    QCOMPARE(clipboard->text(), QStringLiteral("482913"));

    //Not yet.
    mono += 30'000;
    m.expireClipboard();
    QCOMPARE(clipboard->text(), QStringLiteral("482913"));

    mono += 30'000;
    m.expireClipboard();
    QCOMPARE(clipboard->text(), QString());

    //Nothing copied for an unknown or expired code.
    m.copy(QStringLiteral("ffffffffffffffff"));
    QCOMPARE(clipboard->text(), QString());
    m.tick();
    m.copy(QStringLiteral("0123456789abcdef"));
    QCOMPARE(clipboard->text(), QString());
    m.close();
}

void TstAdminCode::copyLeavesSomethingElseAlone()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    stash(entryOf(1, wall, mono));
    AdminCodeModel m(hw());
    m.setClocks([this] { return wall; }, [this] { return mono; });
    m.setReady();

    m.copy(QStringLiteral("fedcba9876543210"));
    QCOMPARE(clipboard->text(), QStringLiteral("007351"));
    clipboard->setText(QStringLiteral("something the user copied"));
    mono += 60'000;
    m.expireClipboard();
    QCOMPARE(clipboard->text(), QStringLiteral("something the user copied"));
    clipboard->clear();
    m.close();
}

void TstAdminCode::cleanupTestCase()
{
    qInstallMessageHandler(s_previousHandler);

    //No code, key, nonce, ct or tag in any log line of the run.
    QStringList secrets;
    for (const Vector &v: kVectors)
        secrets << QLatin1String(v.key) << QLatin1String(v.code) << QLatin1String(v.nonce)
                << QLatin1String(v.ct) << QLatin1String(v.tag);
    for (const QString &line: std::as_const(s_logLines))
        for (const QString &s: std::as_const(secrets))
            QVERIFY2(!line.contains(s), "secret value in a log line");
}

QTEST_MAIN(TstAdminCode)
#include "tst_admincode.moc"
