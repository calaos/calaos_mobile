#include <QtTest>
#include <QUrl>
#import "CalaosQuery.h"

class TstNotifQuery: public QObject
{
    Q_OBJECT

private slots:
    void rawValueEncoding_data();
    void rawValueEncoding();
    void rawFullUrl();
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

//Checked on the raw string: QUrl would normalise the encoding before any check.
void TstNotifQuery::rawValueEncoding_data()
{
    QTest::addColumn<QString>("value");
    QTest::addColumn<QString>("encoded");

    QTest::newRow("unreserved") << QStringLiteral("aZ09-._~") << QStringLiteral("aZ09-._~");
    QTest::newRow("non-ascii") << QStringLiteral("rené") << QStringLiteral("ren%C3%A9");
    QTest::newRow("reserved") << QStringLiteral("& + = % #") << QStringLiteral("%26%20%2B%20%3D%20%25%20%23");
}

void TstNotifQuery::rawValueEncoding()
{
    QFETCH(QString, value);
    QFETCH(QString, encoded);

    NSString *v = value.toNSString();
    NSString *s = CalaosQueryURL(@"http://box/api.php", v, v, @"eventlog", @"uuid", v);
    QCOMPARE(QString::fromNSString(s),
             QStringLiteral("http://box/api.php?cn_user=") + encoded + QStringLiteral("&cn_pass=") + encoded
             + QStringLiteral("&action=eventlog&uuid=") + encoded);
}

void TstNotifQuery::rawFullUrl()
{
    NSString *s = CalaosQueryURL(@"https://box.example:443/calaos/api.php", @"rené", @"p&a+s=s%#~",
                                 @"event_picture", @"pic_uid", @"pic-1_a.b");
    QCOMPARE(QString::fromNSString(s),
             QStringLiteral("https://box.example:443/calaos/api.php?cn_user=ren%C3%A9"
                            "&cn_pass=p%26a%2Bs%3Ds%25%23~&action=event_picture&pic_uid=pic-1_a.b"));
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
