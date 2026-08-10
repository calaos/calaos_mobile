/*
 * Tests unitaires de src/Common.cpp — conversions Common::IOType <-> chaîne.
 *
 * Le contrat couvert ici :
 *   - Common::IOTypeFromString(type, style) doit reconnaître TOUTES les valeurs
 *     de l'enum Common::IOType, y compris les types stylés (Pump/Outlet/Boiler/
 *     Heater et les capteurs binaires) qui partagent la chaîne de type d'un
 *     autre enum et ne se distinguent que par le style.
 *   - Common::IOTypeToString(t) doit rendre la chaîne de type correspondante.
 *   - Le round-trip enum -> chaîne -> enum doit être l'identité.
 *
 * La table `ioTypeCases()` est la référence unique, et `enumCoverageIsComplete()`
 * vérifie via QMetaEnum qu'elle couvre exactement l'enum déclaré dans Common.h :
 * ajouter une valeur à l'enum sans l'ajouter ici fait échouer les tests.
 */

#include <QtTest>
#include <QMetaEnum>
#include <QMetaObject>
#include <QVector>

#include "Common.h"

namespace {

struct IOTypeCase
{
    Common::IOType value;
    const char *name;   //nom de la valeur d'enum, sert de tag de données
    const char *type;   //chaîne de type attendue/acceptée
    const char *style;  //style discriminant ("" si aucun)

    /* false => IOTypeToString() rend actuellement une chaîne vide pour cette
     * valeur : elle n'a pas de case dans le switch de Common.cpp. Le round-trip
     * est alors impossible (cf. ioTypeRoundTrip / QEXPECT_FAIL). */
    bool toStringSupported;
};

const QVector<IOTypeCase> &ioTypeCases()
{
    static const QVector<IOTypeCase> cases = {
        { Common::Unkown,               "Unkown",               "",              "",                true  },
        { Common::Light,                "Light",                "light",         "",                true  },
        { Common::Temp,                 "Temp",                 "temp",          "",                true  },
        { Common::AnalogIn,             "AnalogIn",             "analog_in",     "",                true  },
        { Common::AnalogOut,            "AnalogOut",            "analog_out",    "",                true  },
        { Common::LightDimmer,          "LightDimmer",          "light_dimmer",  "",                true  },
        { Common::LightRgb,             "LightRgb",             "light_rgb",     "",                true  },
        { Common::Shutter,              "Shutter",              "shutter",       "",                true  },
        { Common::ShutterSmart,         "ShutterSmart",         "shutter_smart", "",                true  },
        { Common::VarBool,              "VarBool",              "var_bool",      "",                true  },
        { Common::VarInt,               "VarInt",               "var_int",       "",                true  },
        { Common::VarString,            "VarString",            "var_string",    "",                true  },
        { Common::Scenario,             "Scenario",             "scenario",      "",                true  },
        { Common::AVReceiver,           "AVReceiver",           "avreceiver",    "",                true  },
        { Common::StringIn,             "StringIn",             "string_in",     "",                true  },
        { Common::StringOut,            "StringOut",            "string_out",    "",                true  },
        { Common::Timer,                "Timer",                "timer",         "",                true  },
        { Common::Time,                 "Time",                 "time",          "",                true  },
        { Common::TimeRange,            "TimeRange",            "time_range",    "",                true  },
        { Common::Switch,               "Switch",               "switch",        "",                true  },
        { Common::Switch3,              "Switch3",              "switch3",       "",                true  },
        { Common::SwitchLong,           "SwitchLong",           "switch_long",   "",                true  },
        { Common::AudioInput,           "AudioInput",           "audio_input",   "",                true  },
        { Common::AudioOutput,          "AudioOutput",          "audio_output",  "",                true  },
        { Common::CameraInput,          "CameraInput",          "camera_input",  "",                true  },
        { Common::CameraOutput,         "CameraOutput",         "camera_output", "",                true  },
        { Common::FavoritesLightsCount, "FavoritesLightsCount", "fav_all_lights","",                true  },

        //Types stylés : même chaîne de type que Light, discriminés par le style.
        { Common::Pump,                 "Pump",                 "light",         "pump",            true  },
        { Common::Outlet,               "Outlet",               "light",         "outlet",          true  },
        { Common::Heater,               "Heater",               "light",         "heater",          true  },
        { Common::Boiler,               "Boiler",               "light",         "boiler",          true  },

        /* Capteurs binaires : même chaîne de type que Switch, discriminés par le
         * style. IOTypeToString() ne les gère pas (pas de case dans le switch de
         * Common.cpp) et rend une chaîne vide : le round-trip est cassé pour eux.
         * Voir T16 (IOTypeRegistry) — quand ce sera corrigé, passer le dernier
         * champ à true et retirer le QEXPECT_FAIL correspondant. */
        { Common::DoorSensor,           "DoorSensor",           "switch",        "door",            false },
        { Common::OccupancySensor,      "OccupancySensor",      "switch",        "occupancy",       false },
        { Common::SmokeSensor,          "SmokeSensor",          "switch",        "smoke",           false },
        { Common::WaterLeakSensor,      "WaterLeakSensor",      "switch",        "water",           false },
        { Common::GasLeakSensor,        "GasLeakSensor",        "switch",        "gas",             false },
        { Common::CO2Sensor,            "CO2Sensor",            "switch",        "carbon_monoxide", false },
        { Common::SoundSensor,          "SoundSensor",          "switch",        "sound",           false },
        { Common::MotionSensor,         "MotionSensor",         "switch",        "motion",          false },
        { Common::VibrationSensor,      "VibrationSensor",      "switch",        "vibration",       false },
        { Common::LockSensor,           "LockSensor",           "switch",        "lock",            false },
        { Common::GarageDoorSensor,     "GarageDoorSensor",     "switch",        "garage_door",     false },
    };

    return cases;
}

QMetaEnum ioTypeMetaEnum()
{
    const int idx = Common::staticMetaObject.indexOfEnumerator("IOType");
    if (idx < 0)
        return QMetaEnum();
    return Common::staticMetaObject.enumerator(idx);
}

} //namespace

//Messages d'erreur lisibles (« Light » plutôt qu'une valeur numérique brute).
namespace QTest {
template<>
char *toString(const Common::IOType &t)
{
    const QMetaEnum me = ioTypeMetaEnum();
    const char *key = me.isValid() ? me.valueToKey(static_cast<int>(t)) : nullptr;
    return qstrdup(qPrintable(QStringLiteral("Common::%1 (%2)")
                                  .arg(QString::fromLatin1(key ? key : "<invalide>"))
                                  .arg(static_cast<int>(t))));
}
} //namespace QTest

class TstCommon: public QObject
{
    Q_OBJECT

private slots:
    void enumCoverageIsComplete();

    void ioTypeToString_data();
    void ioTypeToString();

    void ioTypeFromString_data();
    void ioTypeFromString();

    void ioTypeRoundTrip_data();
    void ioTypeRoundTrip();

    void ioTypeFromStringUnknown_data();
    void ioTypeFromStringUnknown();

    void audioStatusRoundTrip_data();
    void audioStatusRoundTrip();

    void parseIntFromString_data();
    void parseIntFromString();

    void parseDoubleFromString_data();
    void parseDoubleFromString();

    void parseLongLongFromString_data();
    void parseLongLongFromString();

    void parseFromVariant_data();
    void parseFromVariant();

    void parseFromJson_data();
    void parseFromJson();

    void parseUsesGivenDefault();
    void parseWarnsOnlyOnMalformedValues();

    void initTestCase();
    void cleanupTestCase();

private:
    void fillIOTypeRows();
};

void TstCommon::fillIOTypeRows()
{
    QTest::addColumn<Common::IOType>("value");
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("style");
    QTest::addColumn<bool>("toStringSupported");

    for (const IOTypeCase &c: ioTypeCases())
    {
        QTest::newRow(c.name) << c.value
                              << QString::fromLatin1(c.type)
                              << QString::fromLatin1(c.style)
                              << c.toStringSupported;
    }
}

/* Garde-fou : la table de ce fichier doit couvrir exactement l'enum IOType tel
 * que déclaré dans src/Common.h. */
void TstCommon::enumCoverageIsComplete()
{
    const QMetaEnum me = ioTypeMetaEnum();
    QVERIFY2(me.isValid(), "L'enum Common::IOType n'est pas exposé au meta-object system");

    const QVector<IOTypeCase> &cases = ioTypeCases();

    QSet<QString> tableKeys;
    for (const IOTypeCase &c: cases)
    {
        const QString name = QString::fromLatin1(c.name);
        QVERIFY2(!tableKeys.contains(name),
                 qPrintable(QStringLiteral("Doublon dans la table de test : %1").arg(name)));
        tableKeys.insert(name);

        const int v = me.keyToValue(c.name);
        QVERIFY2(v >= 0, qPrintable(QStringLiteral("%1 n'est pas une valeur de Common::IOType").arg(name)));
        QCOMPARE(static_cast<int>(c.value), v);
    }

    QSet<QString> enumKeys;
    for (int i = 0; i < me.keyCount(); i++)
        enumKeys.insert(QString::fromLatin1(me.key(i)));

    const QSet<QString> missing = enumKeys - tableKeys;
    QVERIFY2(missing.isEmpty(),
             qPrintable(QStringLiteral("Valeurs de Common::IOType non testées : %1")
                            .arg(QStringList(missing.values()).join(", "))));

    QCOMPARE(cases.size(), me.keyCount());
}

void TstCommon::ioTypeToString_data() { fillIOTypeRows(); }

void TstCommon::ioTypeToString()
{
    QFETCH(Common::IOType, value);
    QFETCH(QString, type);
    QFETCH(bool, toStringSupported);

    if (!toStringSupported)
    {
        //Comportement actuel documenté : pas de case dans le switch => chaîne vide.
        QVERIFY2(Common::IOTypeToString(value).isEmpty(),
                 "Cette valeur est maintenant gérée par IOTypeToString() : "
                 "mettre toStringSupported à true dans ioTypeCases()");
        return;
    }

    QCOMPARE(Common::IOTypeToString(value), type);
}

void TstCommon::ioTypeFromString_data() { fillIOTypeRows(); }

void TstCommon::ioTypeFromString()
{
    QFETCH(Common::IOType, value);
    QFETCH(QString, type);
    QFETCH(QString, style);

    QCOMPARE(Common::IOTypeFromString(type, style), value);
}

void TstCommon::ioTypeRoundTrip_data() { fillIOTypeRows(); }

void TstCommon::ioTypeRoundTrip()
{
    QFETCH(Common::IOType, value);
    QFETCH(QString, style);
    QFETCH(bool, toStringSupported);

    if (!toStringSupported)
    {
        QEXPECT_FAIL("", "IOTypeToString() ne gère pas les capteurs binaires stylés "
                         "(chaîne vide) : round-trip impossible, cf. T16", Abort);
    }

    QCOMPARE(Common::IOTypeFromString(Common::IOTypeToString(value), style), value);
}

void TstCommon::ioTypeFromStringUnknown_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<QString>("style");
    QTest::addColumn<Common::IOType>("expected");

    QTest::newRow("chaine vide")            << QString() << QString()                     << Common::Unkown;
    QTest::newRow("type inconnu")           << QStringLiteral("does_not_exist") << QString() << Common::Unkown;
    QTest::newRow("casse differente")       << QStringLiteral("Light") << QString()        << Common::Unkown;
    QTest::newRow("espaces")                << QStringLiteral(" light ") << QString()      << Common::Unkown;
    //Un style inconnu doit retomber sur le type de base, pas sur Unkown.
    QTest::newRow("light + style inconnu")  << QStringLiteral("light") << QStringLiteral("nope")  << Common::Light;
    QTest::newRow("switch + style inconnu") << QStringLiteral("switch") << QStringLiteral("nope") << Common::Switch;
    //Le style ne doit pas être pris en compte pour un type qui ne le gère pas.
    QTest::newRow("temp + style pump")      << QStringLiteral("temp") << QStringLiteral("pump")   << Common::Temp;
    //Le style seul ne suffit pas.
    QTest::newRow("style pump sans type")   << QString() << QStringLiteral("pump")                << Common::Unkown;
}

void TstCommon::ioTypeFromStringUnknown()
{
    QFETCH(QString, type);
    QFETCH(QString, style);
    QFETCH(Common::IOType, expected);

    QCOMPARE(Common::IOTypeFromString(type, style), expected);
}

void TstCommon::audioStatusRoundTrip_data()
{
    QTest::addColumn<Common::AudioStatusType>("value");
    QTest::addColumn<QString>("text");

    QTest::newRow("StatusPlay")  << Common::StatusPlay  << QStringLiteral("play");
    QTest::newRow("StatusPause") << Common::StatusPause << QStringLiteral("pause");
    QTest::newRow("StatusStop")  << Common::StatusStop  << QStringLiteral("stop");
}

void TstCommon::audioStatusRoundTrip()
{
    QFETCH(Common::AudioStatusType, value);
    QFETCH(QString, text);

    QCOMPARE(Common::audioStatusToString(value), text);
    QCOMPARE(Common::audioStatusFromString(text), value);
    QCOMPARE(Common::audioStatusFromString(Common::audioStatusToString(value)), value);

    //Alias et entrées inconnues
    QCOMPARE(Common::audioStatusFromString(QStringLiteral("playing")), Common::StatusPlay);
    QCOMPARE(Common::audioStatusFromString(QStringLiteral("bogus")), Common::StatusUnknown);
    QVERIFY(Common::audioStatusToString(Common::StatusUnknown).isEmpty());
}

/*
 * Helpers de parsing défensif (T11).
 *
 * Contrat vérifié ici, identique pour les trois surcharges :
 *   - une valeur lisible est rendue telle quelle (aucune régression sur les
 *     données valides, c'est la condition pour pouvoir remplacer les parses nus
 *     de src/ sans changer le comportement) ;
 *   - un champ absent ou vide rend le défaut, SANS warning : une clé manquante
 *     d'une QVariantMap est un cas normal, pas une donnée corrompue ;
 *   - une valeur présente mais non convertible rend le défaut AVEC un warning,
 *     au lieu du 0 silencieux de QString::toInt() / QVariant::toDouble().
 */

void TstCommon::parseIntFromString_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expected");

    QTest::newRow("entier")             << QStringLiteral("42")        << 42;
    QTest::newRow("entier negatif")     << QStringLiteral("-7")        << -7;
    QTest::newRow("zero")               << QStringLiteral("0")         << 0;
    QTest::newRow("espaces autour")     << QStringLiteral("  42  ")    << 42;
    //Les cas suivants retombent tous sur le défaut (0 ici).
    QTest::newRow("chaine vide")        << QString()                   << 0;
    QTest::newRow("chaine vide non nulle") << QStringLiteral("")       << 0;
    QTest::newRow("non numerique")      << QStringLiteral("abc")       << 0;
    QTest::newRow("booleen texte")      << QStringLiteral("true")      << 0;
    QTest::newRow("flottant")           << QStringLiteral("3.5")       << 0;
    QTest::newRow("suffixe parasite")   << QStringLiteral("42abc")     << 0;
    QTest::newRow("hors bornes int")    << QStringLiteral("99999999999") << 0;
}

void TstCommon::parseIntFromString()
{
    QFETCH(QString, input);
    QFETCH(int, expected);

    QCOMPARE(Common::toIntSafe(input), expected);
}

void TstCommon::parseDoubleFromString_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<double>("expected");

    QTest::newRow("entier")             << QStringLiteral("42")     << 42.0;
    QTest::newRow("flottant")           << QStringLiteral("3.5")    << 3.5;
    QTest::newRow("flottant negatif")   << QStringLiteral("-0.25")  << -0.25;
    QTest::newRow("espaces autour")     << QStringLiteral(" 1.5 ")  << 1.5;
    QTest::newRow("chaine vide")        << QString()                << 0.0;
    QTest::newRow("non numerique")      << QStringLiteral("abc")    << 0.0;
    QTest::newRow("booleen texte")      << QStringLiteral("true")   << 0.0;
    QTest::newRow("couleur rgb")        << QStringLiteral("#ff0000") << 0.0;
    QTest::newRow("depassement")        << QStringLiteral("1e9999") << 0.0;
}

void TstCommon::parseDoubleFromString()
{
    QFETCH(QString, input);
    QFETCH(double, expected);

    QCOMPARE(Common::toDoubleSafe(input), expected);
}

void TstCommon::parseLongLongFromString_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<qint64>("expected");

    QTest::newRow("uptime")             << QStringLiteral("1234567890") << Q_INT64_C(1234567890);
    //Valeur qui déborde d'un int mais tient dans un qint64.
    QTest::newRow("hors bornes int")    << QStringLiteral("99999999999") << Q_INT64_C(99999999999);
    QTest::newRow("chaine vide")        << QString()                    << Q_INT64_C(0);
    QTest::newRow("non numerique")      << QStringLiteral("abc")        << Q_INT64_C(0);
}

void TstCommon::parseLongLongFromString()
{
    QFETCH(QString, input);
    QFETCH(qint64, expected);

    QCOMPARE(Common::toLongLongSafe(input), expected);
}

void TstCommon::parseFromVariant_data()
{
    QTest::addColumn<QVariant>("input");
    QTest::addColumn<int>("expectedInt");
    QTest::addColumn<double>("expectedDouble");

    //Cas réel : clé absente d'une QVariantMap venant du serveur.
    QTest::newRow("QVariant invalide") << QVariant()                        << 0   << 0.0;
    QTest::newRow("QString nulle")     << QVariant(QString())               << 0   << 0.0;
    QTest::newRow("QString vide")      << QVariant(QStringLiteral(""))      << 0   << 0.0;
    QTest::newRow("int natif")         << QVariant(42)                      << 42  << 42.0;
    QTest::newRow("double natif")      << QVariant(3.5)                     << 4   << 3.5;
    QTest::newRow("nombre en chaine")  << QVariant(QStringLiteral("42"))    << 42  << 42.0;
    QTest::newRow("flottant en chaine") << QVariant(QStringLiteral("3.5"))  << 0   << 3.5;
    QTest::newRow("texte")             << QVariant(QStringLiteral("abc"))   << 0   << 0.0;
    QTest::newRow("couleur rgb")       << QVariant(QStringLiteral("#ff0000")) << 0 << 0.0;
    QTest::newRow("booleen natif")     << QVariant(true)                    << 1   << 1.0;
}

void TstCommon::parseFromVariant()
{
    QFETCH(QVariant, input);
    QFETCH(int, expectedInt);
    QFETCH(double, expectedDouble);

    QCOMPARE(Common::toIntSafe(input), expectedInt);
    QCOMPARE(Common::toDoubleSafe(input), expectedDouble);
}

void TstCommon::parseFromJson_data()
{
    QTest::addColumn<QJsonValue>("input");
    QTest::addColumn<int>("expectedInt");
    QTest::addColumn<double>("expectedDouble");

    QTest::newRow("undefined")         << QJsonValue(QJsonValue::Undefined)      << 0  << 0.0;
    QTest::newRow("null")              << QJsonValue(QJsonValue::Null)           << 0  << 0.0;
    QTest::newRow("nombre entier")     << QJsonValue(42)                         << 42 << 42.0;
    QTest::newRow("nombre negatif")    << QJsonValue(-7)                         << -7 << -7.0;
    //Un JSON fractionnaire n'est pas un int : défaut côté int, valeur côté double.
    QTest::newRow("nombre fractionnaire") << QJsonValue(3.5)                     << 0  << 3.5;
    //Certains serveurs transportent les nombres en chaîne.
    QTest::newRow("entier en chaine")  << QJsonValue(QStringLiteral("42"))       << 42 << 42.0;
    QTest::newRow("flottant en chaine") << QJsonValue(QStringLiteral("3.5"))     << 0  << 3.5;
    QTest::newRow("chaine vide")       << QJsonValue(QStringLiteral(""))         << 0  << 0.0;
    QTest::newRow("texte")             << QJsonValue(QStringLiteral("abc"))      << 0  << 0.0;
    QTest::newRow("booleen")           << QJsonValue(true)                       << 0  << 0.0;
    QTest::newRow("objet")             << QJsonValue(QJsonObject())              << 0  << 0.0;
}

void TstCommon::parseFromJson()
{
    QFETCH(QJsonValue, input);
    QFETCH(int, expectedInt);
    QFETCH(double, expectedDouble);

    QCOMPARE(Common::toIntSafe(input), expectedInt);
    QCOMPARE(Common::toDoubleSafe(input), expectedDouble);
}

//Le défaut doit être rendu tel quel : c'est ce qui distingue un helper d'un
//parse nu, dont le « défaut » est toujours 0 et n'est jamais choisi.
void TstCommon::parseUsesGivenDefault()
{
    QCOMPARE(Common::toIntSafe(QStringLiteral("abc"), -1), -1);
    QCOMPARE(Common::toIntSafe(QString(), -1), -1);
    QCOMPARE(Common::toDoubleSafe(QStringLiteral("abc"), -1.5), -1.5);
    QCOMPARE(Common::toDoubleSafe(QString(), -1.5), -1.5);
    QCOMPARE(Common::toLongLongSafe(QStringLiteral("abc"), -1), Q_INT64_C(-1));

    QCOMPARE(Common::toIntSafe(QVariant(), 100), 100);
    QCOMPARE(Common::toIntSafe(QVariant(QStringLiteral("abc")), 100), 100);
    QCOMPARE(Common::toDoubleSafe(QVariant(QStringLiteral("abc")), 100.0), 100.0);

    QCOMPARE(Common::toIntSafe(QJsonValue(QJsonValue::Undefined), 100), 100);
    QCOMPARE(Common::toIntSafe(QJsonValue(QStringLiteral("abc")), 100), 100);
    QCOMPARE(Common::toDoubleSafe(QJsonValue(true), 100.0), 100.0);

    //Une valeur lisible ne doit jamais faire gagner le défaut.
    QCOMPARE(Common::toIntSafe(QStringLiteral("0"), -1), 0);
    QCOMPARE(Common::toDoubleSafe(QVariant(0.0), -1.5), 0.0);
}

namespace {

/* Les tables ci-dessus contiennent volontairement beaucoup de valeurs
 * illisibles : sans cela le qWarning des helpers noierait la sortie de
 * `make check`. On le compte au lieu de l'imprimer, ce qui permet en prime de
 * vérifier le contrat « pas de bruit sur un champ absent » — aussi important
 * que la valeur rendue, un warning par clé manquante rendrait les logs de
 * l'application inutilisables. */
int g_warningCount = 0;
QtMessageHandler g_previousHandler = nullptr;

void countWarnings(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    Q_UNUSED(ctx)
    if (type == QtWarningMsg)
    {
        g_warningCount++;
        return;
    }
    if (g_previousHandler)
        g_previousHandler(type, ctx, msg);
}

}

void TstCommon::initTestCase()
{
    g_previousHandler = qInstallMessageHandler(countWarnings);
}

void TstCommon::cleanupTestCase()
{
    qInstallMessageHandler(g_previousHandler);
}

void TstCommon::parseWarnsOnlyOnMalformedValues()
{
    g_warningCount = 0;

    //Champ absent ou vide : silencieux.
    QVariantMap serverData;
    serverData[QStringLiteral("hits")] = QStringLiteral("12");
    Common::toIntSafe(serverData[QStringLiteral("missing")], 0, "test.missing");
    Common::toIntSafe(serverData[QStringLiteral("hits")], 0, "test.hits");
    Common::toIntSafe(QString(), 0, "test.emptyString");
    Common::toDoubleSafe(QVariant(QStringLiteral("")), 0.0, "test.emptyVariant");
    Common::toIntSafe(QJsonValue(QJsonValue::Undefined), 0, "test.undefined");
    Common::toDoubleSafe(QJsonValue(QJsonValue::Null), 0.0, "test.null");
    QCOMPARE(g_warningCount, 0);

    //Valeur présente mais illisible : une trace par appel, quelle que soit la
    //surcharge empruntée.
    Common::toIntSafe(QStringLiteral("abc"), 0, "test.garbageString");
    Common::toDoubleSafe(QVariant(QStringLiteral("abc")), 0.0, "test.garbageVariant");
    Common::toIntSafe(QJsonValue(true), 0, "test.garbageJson");
    QCOMPARE(g_warningCount, 3);
}

QTEST_APPLESS_MAIN(TstCommon)

#include "tst_common.moc"
