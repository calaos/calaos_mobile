#include <QtTest>
#include <QUrl>
#import "CalaosQuery.h"

class TstNotifQuery: public QObject
{
    Q_OBJECT

private slots:
    void credentialsReachTheServerIntact_data();
    void credentialsReachTheServerIntact();
    void numericPictureIdIsKept();
    void missingCredentialsAreEmpty();
};

//Read the query like the server: split on '&' in the raw form, decode each value once.
static QMap<QString, QString> serverQuery(NSString *urlString, int *pairCount)
{
    const QUrl url(QString::fromNSString(urlString), QUrl::StrictMode);
    QMap<QString, QString> q;
    const QStringList pairs = url.query(QUrl::FullyEncoded).split(QLatin1Char('&'));
    for (const QString &pair: pairs)
    {
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq > 0)
            q.insert(pair.left(eq), QUrl::fromPercentEncoding(pair.mid(eq + 1).toLatin1()));
    }
    *pairCount = int(pairs.size());
    return q;
}

void TstNotifQuery::credentialsReachTheServerIntact_data()
{
    QTest::addColumn<QString>("user");
    QTest::addColumn<QString>("pass");
    QTest::addColumn<QString>("id");

    //Synthetic credentials only.
    QTest::newRow("reserved") << QStringLiteral("us er&1") << QStringLiteral("p&a#s+s%20 =x") << QStringLiteral("pic&1");
    QTest::newRow("non-ascii") << QStringLiteral("rené") << QStringLiteral("clé€✓") << QStringLiteral("évé");
}

void TstNotifQuery::credentialsReachTheServerIntact()
{
    QFETCH(QString, user);
    QFETCH(QString, pass);
    QFETCH(QString, id);

    for (NSString *action in @[@"eventlog", @"event_picture"])
    {
        NSString *key = [action isEqualToString:@"eventlog"] ? @"uuid" : @"pic_uid";
        NSString *s = CalaosQueryURL(@"https://box.example:443/api.php",
                                     user.toNSString(), pass.toNSString(),
                                     action, key, id.toNSString());
        QVERIFY([NSURL URLWithString:s] != nil);
        const QUrl url(QString::fromNSString(s), QUrl::StrictMode);
        QVERIFY(url.isValid());
        QVERIFY(url.fragment().isEmpty());
        QCOMPARE(url.host(), QStringLiteral("box.example"));
        //Only unreserved ASCII is left unencoded in the query.
        for (const QChar c: url.query(QUrl::FullyEncoded))
            QVERIFY(c.unicode() < 0x80);

        int pairs = 0;
        const auto q = serverQuery(s, &pairs);
        QCOMPARE(pairs, 4);
        QCOMPARE(q.value(QStringLiteral("cn_user")), user);
        QCOMPARE(q.value(QStringLiteral("cn_pass")), pass);
        QCOMPARE(q.value(QStringLiteral("action")), QString::fromNSString(action));
        QCOMPARE(q.value(QString::fromNSString(key)), id);
    }
}

void TstNotifQuery::numericPictureIdIsKept()
{
    //pic_uid read from JSON can be an NSNumber.
    id number = @42;
    NSString *s = CalaosQueryURL(@"http://box/api.php", @"u", @"p", @"event_picture", @"pic_uid", number);
    int pairs = 0;
    QCOMPARE(serverQuery(s, &pairs).value(QStringLiteral("pic_uid")), QStringLiteral("42"));
}

void TstNotifQuery::missingCredentialsAreEmpty()
{
    NSString *s = CalaosQueryURL(@"http://box/api.php", nil, nil, @"eventlog", @"uuid", @"1");
    int pairs = 0;
    const auto q = serverQuery(s, &pairs);
    QCOMPARE(pairs, 4);
    QVERIFY(q.contains(QStringLiteral("cn_user")));
    QCOMPARE(q.value(QStringLiteral("cn_user")), QString());
    QCOMPARE(q.value(QStringLiteral("cn_pass")), QString());
}

QTEST_GUILESS_MAIN(TstNotifQuery)
#include "tst_notifquery.moc"
