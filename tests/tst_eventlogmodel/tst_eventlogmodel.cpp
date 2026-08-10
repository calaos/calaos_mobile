/*
 * Test unitaire de EventLogIO::resolve() (src/EventLogModel.h), la fonction
 * gabarit extraite lors de T05 depuis EventLogItem::load() (voir
 * src/EventLogModel.cpp, autour de la ligne 140).
 *
 * Bug corrige par T05 : avant la correction, la resolution etait ecrite
 *
 *   IOBase *io = IOCache::Instance().searchInput(id);
 *   if (!io)
 *       IOCache::Instance().searchOutput(id);   // resultat jete !
 *   if (io)
 *       ...
 *
 * Le resultat de searchOutput() n'etait jamais reaffecte a `io` : toute
 * entree du journal concernant un IO *output* (lumiere, prise, volet...)
 * restait donc anonyme. EventLogIO::resolve() porte exactement ce motif
 * (chercher en entree, sinon retomber sur la sortie, garder le resultat) et
 * c'est desormais la fonction utilisee par EventLogItem::load().
 *
 * Portee reduite : IOCache/IOBase (src/RoomModel.cpp) ne peuvent pas etre
 * lies dans ce test isole sans entrainer CalaosConnection, HardwareUtils et
 * quickflux en cascade (voir tst_eventlogmodel.pro). On verifie donc le
 * motif de resolution avec un IO factice minimal plutot qu'un vrai IOBase ;
 * la fonction testee est neanmoins la fonction reellement appelee par le
 * code de production (EventLogModel.h/.cpp), pas une reimplementation.
 */

#include <QtTest>
#include <QHash>
#include <QString>

#include "EventLogModel.h"

namespace {

// IO factice minimal : seul le nom nous interesse pour verifier quel objet
// a ete resolu (evTitle == nom de l'IO dans le code de production).
struct FakeIO
{
    QString name;
};

} // namespace

class TstEventLogModel: public QObject
{
    Q_OBJECT

private slots:
    void resolvePicksInputOrOutput_data();
    void resolvePicksInputOrOutput();

    void resolveInputTakesPriorityOverOutput();
    void resolveReturnsNullWhenAbsentFromBoth();
};

void TstEventLogModel::resolvePicksInputOrOutput_data()
{
    QTest::addColumn<bool>("presentInInput");
    QTest::addColumn<bool>("presentInOutput");
    QTest::addColumn<QString>("expectedName");

    // Cas 1 : IO present uniquement en output (le cas casse avant T05).
    QTest::newRow("output_only") << false << true << QStringLiteral("Output Light");

    // Cas 2 : IO present uniquement en input.
    QTest::newRow("input_only") << true << false << QStringLiteral("Input Sensor");
}

void TstEventLogModel::resolvePicksInputOrOutput()
{
    QFETCH(bool, presentInInput);
    QFETCH(bool, presentInOutput);
    QFETCH(QString, expectedName);

    const QString id = QStringLiteral("io-1");

    FakeIO inputIO { QStringLiteral("Input Sensor") };
    FakeIO outputIO { QStringLiteral("Output Light") };

    QHash<QString, FakeIO *> inputCache;
    QHash<QString, FakeIO *> outputCache;
    if (presentInInput) inputCache[id] = &inputIO;
    if (presentInOutput) outputCache[id] = &outputIO;

    FakeIO *resolved = EventLogIO::resolve<FakeIO>(
        [&](const QString &i) { return inputCache.value(i, nullptr); },
        [&](const QString &i) { return outputCache.value(i, nullptr); },
        id);

    QVERIFY(resolved != nullptr);
    QCOMPARE(resolved->name, expectedName);
}

// Cas symetrique explicite : quand un id existe des deux cotes, l'entree
// (input) doit toujours l'emporter — c'est l'ordre historique du code de
// EventLogItem::load(), que la correction T05 ne doit pas inverser.
void TstEventLogModel::resolveInputTakesPriorityOverOutput()
{
    const QString id = QStringLiteral("io-both");

    FakeIO inputIO { QStringLiteral("Input Wins") };
    FakeIO outputIO { QStringLiteral("Output Loses") };

    QHash<QString, FakeIO *> inputCache { { id, &inputIO } };
    QHash<QString, FakeIO *> outputCache { { id, &outputIO } };

    FakeIO *resolved = EventLogIO::resolve<FakeIO>(
        [&](const QString &i) { return inputCache.value(i, nullptr); },
        [&](const QString &i) { return outputCache.value(i, nullptr); },
        id);

    QVERIFY(resolved != nullptr);
    QCOMPARE(resolved->name, QStringLiteral("Input Wins"));
}

// Cas 3 : IO absent des deux caches (id inconnu). Doit rendre nullptr sans
// planter — c'est ce que EventLogItem::load() teste ensuite via `if (io)`
// pour garder son titre par defaut ("Unknown event!").
void TstEventLogModel::resolveReturnsNullWhenAbsentFromBoth()
{
    const QString id = QStringLiteral("io-unknown");

    QHash<QString, FakeIO *> inputCache;
    QHash<QString, FakeIO *> outputCache;

    FakeIO *resolved = EventLogIO::resolve<FakeIO>(
        [&](const QString &i) { return inputCache.value(i, nullptr); },
        [&](const QString &i) { return outputCache.value(i, nullptr); },
        id);

    QVERIFY(resolved == nullptr);
}

QTEST_APPLESS_MAIN(TstEventLogModel)

#include "tst_eventlogmodel.moc"
