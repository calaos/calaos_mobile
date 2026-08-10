/*
 * Tests unitaires de src/RoomFilterModel.cpp — cf. ticket T04.
 *
 * Contrat couvert ici :
 *   - resetCache() est IDEMPOTENT : il reconstruit intégralement son état
 *     (leftCache/rightCache ET les listes shutters/lights/temps/other). Deux
 *     appels successifs donnent exactement le même partitionnement, sans
 *     doublon et sans croissance des listes internes.
 *     Avant T04 seuls leftCache/rightCache étaient vidés : les quatre listes
 *     étaient appendées à chaque appel (rowsInserted, rowsRemoved, modelReset,
 *     changement de filtre), d'où des doublons, une croissance non bornée et
 *     surtout des IOBase* pendants après un reset du modèle source.
 *   - resetCache() et lessThan() survivent à un item non-IOBase dans le modèle
 *     source (les dynamic_cast n'étaient pas vérifiés avant de déréférencer).
 *
 * LIMITE : src/RoomModel.cpp n'est pas lié (il tire HardwareUtils, quickflux et
 * QtQuick). Le test lie le vrai src/RoomModel.h avec la doublure de lien
 * roommodel_stub.cpp ; voir l'en-tête de ce fichier. Seul le comportement de
 * RoomModel::getItemModel() compte pour RoomFilterModel et il y est reproduit.
 */

#include <QtTest>
#include <QLoggingCategory>
#include <QSet>
#include <QStandardItem>

#include "Common.h"
#include "RoomFilterModel.h"
#include "RoomModel.h"

namespace {

/* resetCache() est un slot privé et les caches sont protégés : on passe par une
 * sous-classe de test plutôt que de modifier RoomFilterModel.h. */
class TestableRoomFilterModel: public RoomFilterModel
{
public:
    using RoomFilterModel::RoomFilterModel;

    bool callResetCache() { return QMetaObject::invokeMethod(this, "resetCache"); }

    QStringList leftIds() const { return sortedKeys(leftCache); }
    QStringList rightIds() const { return sortedKeys(rightCache); }

    QList<IOBase *> shuttersList() const { return shutters; }
    QList<IOBase *> lightsList() const { return lights; }
    QList<IOBase *> tempsList() const { return temps; }
    QList<IOBase *> otherList() const { return other; }

    int internalListsTotal() const
    {
        return shutters.size() + lights.size() + temps.size() + other.size();
    }

private:
    static QStringList sortedKeys(const QHash<QString, IOBase *> &cache)
    {
        QStringList keys = cache.keys();
        keys.sort();
        return keys;
    }
};

struct IODef
{
    const char *id;
    const char *name;
    Common::IOType type;
};

/* Jeu d'IO couvrant les quatre catégories de resetCache() :
 * shutters (2), temps (2), lights (3), other (2, dont un scénario). */
const QVector<IODef> &ioDefs()
{
    static const QVector<IODef> defs = {
        { "io_shutter_1", "Volet salon",   Common::Shutter      },
        { "io_shutter_2", "Volet cuisine", Common::ShutterSmart },
        { "io_light_1",   "Plafonnier",    Common::Light        },
        { "io_light_2",   "Variateur",     Common::LightDimmer  },
        { "io_light_3",   "Bandeau LED",   Common::LightRgb     },
        { "io_temp_1",    "Temperature",   Common::Temp         },
        { "io_temp_2",    "Compteur",      Common::VarInt       },
        { "io_other_1",   "Prise",         Common::Switch       },
        { "io_scen_1",    "Depart",        Common::Scenario     },
    };
    return defs;
}

IOBase *makeIo(const IODef &def)
{
    //engine et connection nuls : la doublure de lien ne les déréférence pas.
    IOBase *io = new IOBase(nullptr, nullptr, static_cast<int>(def.type));
    io->update_ioType(def.type);
    io->update_ioId(QString::fromLatin1(def.id));
    io->update_ioName(QString::fromLatin1(def.name));
    return io;
}

QStringList allIoIds()
{
    QStringList ids;
    for (const IODef &d: ioDefs())
        ids << QString::fromLatin1(d.id);
    ids.sort();
    return ids;
}

//Capture des qWarning() le temps d'un test (et les empêche de polluer la sortie).
QStringList g_warnings;
QtMessageHandler g_previousHandler = nullptr;

void captureMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtWarningMsg)
        g_warnings << msg;
}

class WarningCapture
{
public:
    WarningCapture()
    {
        g_warnings.clear();
        g_previousHandler = qInstallMessageHandler(captureMessageHandler);
    }

    ~WarningCapture()
    {
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
    }

    QStringList messages() const { return g_warnings; }
};

} //namespace

class TstRoomFilterModel: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void init();
    void cleanup();

    void resetCacheIsIdempotent();
    void internalListsDoNotGrow_data();
    void internalListsDoNotGrow();
    void internalListsHaveNoDuplicates();
    void partitionCoversEveryIoExactlyOnce();
    void filterChangeKeepsPartitionStable();
    void sourceModelResetDropsStaleItems();
    void nonIOBaseItemIsSkipped();
    void sortingWithNonIOBaseItemDoesNotCrash();

private:
    RoomModel *rmodel = nullptr;
    TestableRoomFilterModel *proxy = nullptr;
};

void TstRoomFilterModel::initTestCase()
{
    /* lessThan() trace chaque comparaison via qDebug() : inexploitable dans la
     * sortie du test. */
    QLoggingCategory::setFilterRules(QStringLiteral("default.debug=false"));
}

void TstRoomFilterModel::init()
{
    rmodel = new RoomModel(nullptr, nullptr);
    for (const IODef &d: ioDefs())
        rmodel->appendRow(makeIo(d));

    proxy = new TestableRoomFilterModel();
    proxy->set_source(rmodel);

    QCOMPARE(rmodel->rowCount(), ioDefs().size());
}

void TstRoomFilterModel::cleanup()
{
    delete proxy;
    proxy = nullptr;
    delete rmodel;
    rmodel = nullptr;
}

/* Cœur du ticket : deux resetCache() successifs doivent donner le même état. */
void TstRoomFilterModel::resetCacheIsIdempotent()
{
    QVERIFY(proxy->callResetCache());
    const QStringList left1 = proxy->leftIds();
    const QStringList right1 = proxy->rightIds();
    const int total1 = proxy->internalListsTotal();

    QVERIFY(!left1.isEmpty());
    QVERIFY(!right1.isEmpty());

    for (int i = 0; i < 4; i++)
    {
        QVERIFY(proxy->callResetCache());
        QCOMPARE(proxy->leftIds(), left1);
        QCOMPARE(proxy->rightIds(), right1);
        QCOMPARE(proxy->internalListsTotal(), total1);
    }
}

void TstRoomFilterModel::internalListsDoNotGrow_data()
{
    QTest::addColumn<int>("resetCount");

    QTest::newRow("1 appel")  << 1;
    QTest::newRow("2 appels") << 2;
    QTest::newRow("5 appels") << 5;
}

/* Les quatre listes sont reconstruites, pas appendées : leur taille ne dépend
 * pas du nombre d'appels à resetCache(). */
void TstRoomFilterModel::internalListsDoNotGrow()
{
    QFETCH(int, resetCount);

    for (int i = 0; i < resetCount; i++)
        QVERIFY(proxy->callResetCache());

    QCOMPARE(proxy->shuttersList().size(), 2);  //Shutter + ShutterSmart
    QCOMPARE(proxy->lightsList().size(), 3);    //Light + LightDimmer + LightRgb
    QCOMPARE(proxy->tempsList().size(), 2);     //Temp + VarInt
    QCOMPARE(proxy->otherList().size(), 2);     //Switch + Scenario
    QCOMPARE(proxy->internalListsTotal(), ioDefs().size());
}

void TstRoomFilterModel::internalListsHaveNoDuplicates()
{
    for (int i = 0; i < 3; i++)
        QVERIFY(proxy->callResetCache());

    const QList<QList<IOBase *>> lists = {
        proxy->shuttersList(), proxy->lightsList(),
        proxy->tempsList(), proxy->otherList()
    };

    QSet<IOBase *> seen;
    for (const QList<IOBase *> &l: lists)
    {
        for (IOBase *io: l)
        {
            QVERIFY2(io != nullptr, "Une liste interne contient un pointeur nul");
            QVERIFY2(!seen.contains(io),
                     qPrintable(QStringLiteral("IO en double dans les listes internes : %1")
                                    .arg(io->get_ioId())));
            seen.insert(io);
        }
    }

    QCOMPARE(seen.size(), ioDefs().size());
}

/* Chaque IO se retrouve dans exactement une des deux moitiés. */
void TstRoomFilterModel::partitionCoversEveryIoExactlyOnce()
{
    QVERIFY(proxy->callResetCache());

    QStringList left = proxy->leftIds();
    QStringList right = proxy->rightIds();

    const QSet<QString> leftSet(left.begin(), left.end());
    const QSet<QString> rightSet(right.begin(), right.end());

    QVERIFY2((leftSet & rightSet).isEmpty(), "Un IO est à la fois à gauche et à droite");

    QStringList all = left + right;
    all.sort();
    QCOMPARE(all, allIoIds());
}

/* Un changement de filtre déclenche resetCache() : le partitionnement calculé
 * ne doit pas dépendre du nombre de changements déjà subis. */
void TstRoomFilterModel::filterChangeKeepsPartitionStable()
{
    const QStringList left1 = proxy->leftIds();
    const QStringList right1 = proxy->rightIds();

    proxy->set_filter(Common::FilterLeft);
    const int leftRows = proxy->rowCount();
    proxy->set_filter(Common::FilterRight);
    const int rightRows = proxy->rowCount();
    proxy->set_filter(Common::FilterAll);

    QCOMPARE(proxy->leftIds(), left1);
    QCOMPARE(proxy->rightIds(), right1);
    QCOMPARE(leftRows, left1.size());
    QCOMPARE(rightRows, right1.size());
    QCOMPARE(proxy->rowCount(), ioDefs().size());

    //Deuxième tour : mêmes résultats qu'au premier.
    proxy->set_filter(Common::FilterLeft);
    QCOMPARE(proxy->rowCount(), leftRows);
    proxy->set_filter(Common::FilterRight);
    QCOMPARE(proxy->rowCount(), rightRows);
}

/* Régression principale : après un reset du modèle source (reconnexion), les
 * IOBase sont détruits. Les listes internes ne doivent plus les référencer. */
void TstRoomFilterModel::sourceModelResetDropsStaleItems()
{
    QVERIFY(proxy->callResetCache());
    QCOMPARE(proxy->internalListsTotal(), ioDefs().size());

    rmodel->clear(); //détruit tous les IOBase et émet modelReset()

    QCOMPARE(proxy->internalListsTotal(), 0);
    QVERIFY(proxy->leftIds().isEmpty());
    QVERIFY(proxy->rightIds().isEmpty());
    QCOMPARE(proxy->rowCount(), 0);

    //Repeuplement : l'état reparte de zéro, sans trace des anciens pointeurs.
    for (const IODef &d: ioDefs())
        rmodel->appendRow(makeIo(d));

    QVERIFY(proxy->callResetCache());
    QCOMPARE(proxy->internalListsTotal(), ioDefs().size());
    QStringList all = proxy->leftIds() + proxy->rightIds();
    all.sort();
    QCOMPARE(all, allIoIds());
}

/* Un item qui n'est pas un IOBase (ou un getItemModel() qui rend nullptr) ne
 * doit pas faire planter resetCache(). */
void TstRoomFilterModel::nonIOBaseItemIsSkipped()
{
    QStringList warnings;
    {
        WarningCapture capture;
        rmodel->appendRow(new QStandardItem(QStringLiteral("pas un IOBase")));
        QVERIFY(proxy->callResetCache());
        warnings = capture.messages();
    }

    //L'item étranger est ignoré, les autres sont toujours partitionnés.
    QCOMPARE(rmodel->rowCount(), ioDefs().size() + 1);
    QCOMPARE(proxy->internalListsTotal(), ioDefs().size());

    QStringList all = proxy->leftIds() + proxy->rightIds();
    all.sort();
    QCOMPARE(all, allIoIds());

    bool warned = false;
    for (const QString &w: warnings)
    {
        if (w.contains(QStringLiteral("IOBase")))
        {
            warned = true;
            break;
        }
    }
    QVERIFY2(warned, "resetCache() doit signaler l'item ignoré par un qWarning()");
}

/* lessThan() déréférençait lobj/robj sans vérifier le dynamic_cast : avec un
 * item non-IOBase dans le modèle, le tri plantait. */
void TstRoomFilterModel::sortingWithNonIOBaseItemDoesNotCrash()
{
    WarningCapture capture; //resetCache() va prévenir pour chaque item ignoré

    proxy->set_filter(Common::FilterAll);
    rmodel->appendRow(new QStandardItem(QStringLiteral("pas un IOBase")));

    //Force un tri complet du proxy (dynamicSortFilter est actif).
    proxy->invalidate();
    proxy->sort(0);

    QCOMPARE(proxy->rowCount(), ioDefs().size() + 1);

    //Tous les IOBase restent accessibles au travers du proxy.
    QStringList seen;
    for (int i = 0; i < proxy->rowCount(); i++)
    {
        IOBase *io = dynamic_cast<IOBase *>(proxy->getItemModel(i));
        if (io)
            seen << io->get_ioId();
    }
    seen.sort();
    QCOMPARE(seen, allIoIds());
}

QTEST_GUILESS_MAIN(TstRoomFilterModel)

#include "tst_roomfiltermodel.moc"
