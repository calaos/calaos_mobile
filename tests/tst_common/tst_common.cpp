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
 *
 * La seconde moitié du fichier couvre src/IOTypeRegistry.cpp, à qui Common
 * délègue depuis T16 : catégorie, « est-ce une lumière », visibilité dans une
 * pièce et table du protocole historique. Ces tests vivent ici (et non dans un
 * tst_iotyperegistry/) parce que les deux fichiers forment un seul contrat et
 * sont liés ensemble.
 */

#include <QtTest>
#include <QMetaEnum>
#include <QMetaObject>
#include <QVector>

#include "Common.h"
#include "IOTypeRegistry.h"

namespace {

struct IOTypeCase
{
    Common::IOType value;
    const char *name;   //nom de la valeur d'enum, sert de tag de données
    const char *type;   //chaîne de type attendue/acceptée
    const char *style;  //style discriminant ("" si aucun)
};

const QVector<IOTypeCase> &ioTypeCases()
{
    static const QVector<IOTypeCase> cases = {
        { Common::Unkown,               "Unkown",               "",              "" },
        { Common::Light,                "Light",                "light",         "" },
        { Common::Temp,                 "Temp",                 "temp",          "" },
        { Common::AnalogIn,             "AnalogIn",             "analog_in",     "" },
        { Common::AnalogOut,            "AnalogOut",            "analog_out",    "" },
        { Common::LightDimmer,          "LightDimmer",          "light_dimmer",  "" },
        { Common::LightRgb,             "LightRgb",             "light_rgb",     "" },
        { Common::Shutter,              "Shutter",              "shutter",       "" },
        { Common::ShutterSmart,         "ShutterSmart",         "shutter_smart", "" },
        { Common::VarBool,              "VarBool",              "var_bool",      "" },
        { Common::VarInt,               "VarInt",               "var_int",       "" },
        { Common::VarString,            "VarString",            "var_string",    "" },
        { Common::Scenario,             "Scenario",             "scenario",      "" },
        { Common::AVReceiver,           "AVReceiver",           "avreceiver",    "" },
        { Common::StringIn,             "StringIn",             "string_in",     "" },
        { Common::StringOut,            "StringOut",            "string_out",    "" },
        { Common::Timer,                "Timer",                "timer",         "" },
        { Common::Time,                 "Time",                 "time",          "" },
        { Common::TimeRange,            "TimeRange",            "time_range",    "" },
        { Common::Switch,               "Switch",               "switch",        "" },
        { Common::Switch3,              "Switch3",              "switch3",       "" },
        { Common::SwitchLong,           "SwitchLong",           "switch_long",   "" },
        { Common::AudioInput,           "AudioInput",           "audio_input",   "" },
        { Common::AudioOutput,          "AudioOutput",          "audio_output",  "" },
        { Common::CameraInput,          "CameraInput",          "camera_input",  "" },
        { Common::CameraOutput,         "CameraOutput",         "camera_output", "" },
        { Common::FavoritesLightsCount, "FavoritesLightsCount", "fav_all_lights","" },

        //Types stylés : même chaîne de type que Light, discriminés par le style.
        { Common::Pump,                 "Pump",                 "light",         "pump" },
        { Common::Outlet,               "Outlet",               "light",         "outlet" },
        { Common::Heater,               "Heater",               "light",         "heater" },
        { Common::Boiler,               "Boiler",               "light",         "boiler" },

        /* Capteurs binaires : même chaîne de type que Switch, discriminés par
         * le style, exactement comme les types stylés ci-dessus. Le registre
         * (T16) les décrit sur la même ligne que le reste, donc IOTypeToString()
         * rend « switch » et le round-trip est l'identité pour eux aussi. */
        { Common::DoorSensor,           "DoorSensor",           "switch",        "door" },
        { Common::OccupancySensor,      "OccupancySensor",      "switch",        "occupancy" },
        { Common::SmokeSensor,          "SmokeSensor",          "switch",        "smoke" },
        { Common::WaterLeakSensor,      "WaterLeakSensor",      "switch",        "water" },
        { Common::GasLeakSensor,        "GasLeakSensor",        "switch",        "gas" },
        { Common::CO2Sensor,            "CO2Sensor",            "switch",        "carbon_monoxide" },
        { Common::SoundSensor,          "SoundSensor",          "switch",        "sound" },
        { Common::MotionSensor,         "MotionSensor",         "switch",        "motion" },
        { Common::VibrationSensor,      "VibrationSensor",      "switch",        "vibration" },
        { Common::LockSensor,           "LockSensor",           "switch",        "lock" },
        { Common::GarageDoorSensor,     "GarageDoorSensor",     "switch",        "garage_door" },
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

    void registryCoversEveryIoType();
    void registryProperties_data();
    void registryProperties();
    void registryStyleName();
    void onlyRealLightsAreCounted();
    void registryMediaGuiTypes();
    void registryLegacyGuiType_data();
    void registryLegacyGuiType();
    void registryLegacyGuiTypeUnknown();

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

    for (const IOTypeCase &c: ioTypeCases())
    {
        QTest::newRow(c.name) << c.value
                              << QString::fromLatin1(c.type)
                              << QString::fromLatin1(c.style);
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

/* ---------------------------------------------------------------------------
 * T16 — le registre (src/IOTypeRegistry.h) derrière Common::IOType*.
 *
 * Il porte desormais TOUT ce que l'application derive du type d'un IO. Les
 * colonnes ci-dessous figent ce contrat :
 *   - categorie / isLight / dimmable : ce que RoomFilterModel, EventLogModel et
 *     IOBase lisaient chacun de leur cote avant T16 ;
 *   - visibleInput / visibleOutput : les trois listes de gui_type en dur de
 *     RoomModel::load(), reprises telles quelles.
 * ------------------------------------------------------------------------- */

using Cat = IOTypeRegistry::Category;

namespace {

struct RegistryCase
{
    Common::IOType value;
    Cat category;
    bool isLight;
    bool dimmable;
    bool measurement;
    bool visibleInput;
    bool visibleOutput;
};

const QVector<RegistryCase> &registryCases()
{
    static const QVector<RegistryCase> cases = {
    { Common::Unkown,               Cat::Other,    false, false, false, false, false },
    { Common::Light,                Cat::Light,    true,  false, false, false, true  },
    { Common::Pump,                 Cat::Light,    true,  false, false, false, true  },
    { Common::Outlet,               Cat::Light,    true,  false, false, false, true  },
    { Common::Boiler,               Cat::Light,    true,  false, false, false, true  },
    { Common::Heater,               Cat::Light,    true,  false, false, false, true  },
    { Common::LightDimmer,          Cat::Light,    true,  true,  false, false, true  },
    { Common::LightRgb,             Cat::Light,    true,  true,  false, false, true  },
    { Common::Shutter,              Cat::Shutter,  false, false, false, false, true  },
    { Common::ShutterSmart,         Cat::Shutter,  false, false, false, false, true  },
    { Common::Temp,                 Cat::Temp,     false, false, true,  true,  false },
    { Common::AnalogIn,             Cat::Temp,     false, false, true,  true,  false },
    { Common::AnalogOut,            Cat::Other,    false, false, false, false, true  },
    { Common::VarBool,              Cat::Var,      false, false, false, false, true  },
    { Common::VarInt,               Cat::Var,      false, false, true,  false, true  },
    { Common::VarString,            Cat::Var,      false, false, false, false, true  },
    { Common::Scenario,             Cat::Scenario, false, false, false, true,  false },
    { Common::AVReceiver,           Cat::Media,    false, false, false, false, false },
    { Common::AudioInput,           Cat::Media,    false, false, false, false, false },
    { Common::AudioOutput,          Cat::Media,    false, false, false, false, false },
    { Common::CameraInput,          Cat::Media,    false, false, false, false, false },
    { Common::CameraOutput,         Cat::Media,    false, false, false, false, false },
    { Common::StringIn,             Cat::Other,    false, false, false, true,  false },
    { Common::StringOut,            Cat::Other,    false, false, false, false, true  },
    { Common::Timer,                Cat::Other,    false, false, false, false, false },
    { Common::Time,                 Cat::Other,    false, false, false, false, false },
    { Common::TimeRange,            Cat::Other,    false, false, false, false, false },
    { Common::Switch,               Cat::Other,    false, false, false, true,  false },
    { Common::Switch3,              Cat::Other,    false, false, false, false, false },
    { Common::SwitchLong,           Cat::Other,    false, false, false, false, false },
    { Common::DoorSensor,           Cat::Sensor,   false, false, false, true,  false },
    { Common::OccupancySensor,      Cat::Sensor,   false, false, false, true,  false },
    { Common::SmokeSensor,          Cat::Sensor,   false, false, false, true,  false },
    { Common::WaterLeakSensor,      Cat::Sensor,   false, false, false, true,  false },
    { Common::GasLeakSensor,        Cat::Sensor,   false, false, false, true,  false },
    { Common::CO2Sensor,            Cat::Sensor,   false, false, false, true,  false },
    { Common::SoundSensor,          Cat::Sensor,   false, false, false, true,  false },
    { Common::MotionSensor,         Cat::Sensor,   false, false, false, true,  false },
    { Common::VibrationSensor,      Cat::Sensor,   false, false, false, true,  false },
    { Common::LockSensor,           Cat::Sensor,   false, false, false, true,  false },
    { Common::GarageDoorSensor,     Cat::Sensor,   false, false, false, true,  false },
    { Common::FavoritesLightsCount, Cat::Other,    false, false, false, false, false },
    };

    return cases;
}

QString ioTypeName(Common::IOType t)
{
    const QMetaEnum me = ioTypeMetaEnum();
    const char *key = me.isValid()? me.valueToKey(static_cast<int>(t)): nullptr;
    return QString::fromLatin1(key? key: "<invalide>");
}

} //namespace

/* Tout type connu de l'enum a une ligne dans le registre — sauf Unkown, qui est
 * la reponse rendue quand rien ne correspond, pas un type que le serveur envoie. */
void TstCommon::registryCoversEveryIoType()
{
    const QMetaEnum me = ioTypeMetaEnum();
    QVERIFY(me.isValid());

    for (int i = 0; i < me.keyCount(); i++)
    {
        const Common::IOType t = static_cast<Common::IOType>(me.value(i));
        if (t == Common::Unkown)
        {
            QVERIFY(IOTypeRegistry::entry(t) == nullptr);
            QVERIFY(IOTypeRegistry::guiType(t).isEmpty());
            continue;
        }

        QVERIFY2(IOTypeRegistry::entry(t) != nullptr,
                 qPrintable(QStringLiteral("%1 n'a pas de ligne dans IOTypeRegistry").arg(ioTypeName(t))));

        //Aller-retour au niveau du registre lui-meme.
        QCOMPARE(IOTypeRegistry::fromGuiType(IOTypeRegistry::guiType(t),
                                             IOTypeRegistry::style(t)), t);
    }

    //La table de ce test couvre elle aussi tout l'enum.
    QCOMPARE(registryCases().size(), me.keyCount());

    /* Integrite de la table : un IOType par ligne, et un couple
     * (gui_type, style) par ligne — sans quoi une ligne serait inatteignable. */
    QSet<int> seenTypes;
    QSet<QString> seenPairs;
    for (const IOTypeRegistry::Entry &e: IOTypeRegistry::entries())
    {
        const int t = static_cast<int>(e.ioType);
        QVERIFY2(!seenTypes.contains(t),
                 qPrintable(QStringLiteral("%1 apparait deux fois dans le registre")
                                .arg(ioTypeName(e.ioType))));
        seenTypes.insert(t);

        const QString pair = QString::fromLatin1(e.guiType) + QLatin1Char('/')
                             + QString::fromLatin1(e.style);
        QVERIFY2(!seenPairs.contains(pair),
                 qPrintable(QStringLiteral("Couple (gui_type, style) en double : %1").arg(pair)));
        seenPairs.insert(pair);
    }

    QCOMPARE(IOTypeRegistry::entries().size(), me.keyCount() - 1); //Unkown excepte
}

void TstCommon::registryProperties_data()
{
    QTest::addColumn<Common::IOType>("value");
    QTest::addColumn<int>("category");
    QTest::addColumn<bool>("isLight");
    QTest::addColumn<bool>("dimmable");
    QTest::addColumn<bool>("measurement");
    QTest::addColumn<bool>("visibleInput");
    QTest::addColumn<bool>("visibleOutput");

    for (const RegistryCase &c: registryCases())
    {
        QTest::newRow(qPrintable(ioTypeName(c.value)))
            << c.value << static_cast<int>(c.category)
            << c.isLight << c.dimmable << c.measurement
            << c.visibleInput << c.visibleOutput;
    }
}

void TstCommon::registryProperties()
{
    QFETCH(Common::IOType, value);
    QFETCH(int, category);
    QFETCH(bool, isLight);
    QFETCH(bool, dimmable);
    QFETCH(bool, measurement);
    QFETCH(bool, visibleInput);
    QFETCH(bool, visibleOutput);

    QCOMPARE(static_cast<int>(IOTypeRegistry::category(value)), category);
    QCOMPARE(IOTypeRegistry::isLight(value), isLight);
    QCOMPARE(IOTypeRegistry::isDimmableLight(value), dimmable);
    //Une lumiere est soit booleenne soit variable, jamais les deux.
    QCOMPARE(IOTypeRegistry::isBinaryLight(value), isLight && !dimmable);
    QCOMPARE(IOTypeRegistry::isMeasurement(value), measurement);
    QCOMPARE(IOTypeRegistry::isRoomVisibleInput(value), visibleInput);
    QCOMPARE(IOTypeRegistry::isRoomVisibleOutput(value), visibleOutput);
}

/* Le compteur de lumieres ne doit compter que de vraies lumieres. Pompe, prise,
 * chaudiere et radiateur arrivent en gui_type "light" avec un style et se
 * dessinent comme une lumiere, mais les compter afficherait des lumieres
 * allumees alors qu'aucune ne l'est. Regle posee par Raoul le 2026-08-11 apres
 * que T16 les ait fait entrer dans le compteur ; la spec du ticket affirmait
 * l'inverse. Pilote par la table pour qu'un type ajoute plus tard doive etre
 * classe explicitement. */
void TstCommon::onlyRealLightsAreCounted()
{
    const QSet<Common::IOType> expected = {
        Common::Light, Common::LightDimmer, Common::LightRgb
    };

    for (const IOTypeRegistry::Entry &e: IOTypeRegistry::entries())
    {
        const bool shouldCount = expected.contains(e.ioType);
        QVERIFY2(IOTypeRegistry::countsAsLight(e.ioType) == shouldCount,
                 qPrintable(QStringLiteral("%1 : countsAsLight vaut %2, attendu %3")
                                .arg(ioTypeName(e.ioType))
                                .arg(IOTypeRegistry::countsAsLight(e.ioType))
                                .arg(shouldCount)));

        //Tout ce qui compte est une lumiere, l'inverse est faux.
        if (IOTypeRegistry::countsAsLight(e.ioType))
            QVERIFY(IOTypeRegistry::isLight(e.ioType));
    }

    //Les quatre styles restent des lumieres pour le regroupement et le rendu :
    //seule leur participation au compteur change.
    for (Common::IOType t: { Common::Pump, Common::Outlet,
                             Common::Boiler, Common::Heater })
    {
        QVERIFY(IOTypeRegistry::isLight(t));
        QVERIFY(IOTypeRegistry::isBinaryLight(t));
        QVERIFY(!IOTypeRegistry::countsAsLight(t));
    }
}

//Nom de la variante visuelle : le style s'il y en a un, le gui_type sinon.
void TstCommon::registryStyleName()
{
    QCOMPARE(IOTypeRegistry::styleName(Common::Light), QStringLiteral("light"));
    QCOMPARE(IOTypeRegistry::styleName(Common::Pump), QStringLiteral("pump"));
    QCOMPARE(IOTypeRegistry::styleName(Common::Outlet), QStringLiteral("outlet"));
    QCOMPARE(IOTypeRegistry::styleName(Common::Boiler), QStringLiteral("boiler"));
    QCOMPARE(IOTypeRegistry::styleName(Common::Heater), QStringLiteral("heater"));
    QCOMPARE(IOTypeRegistry::styleName(Common::DoorSensor), QStringLiteral("door"));
    QVERIFY(IOTypeRegistry::styleName(Common::Unkown).isEmpty());
}

/* Les endpoints media d'une piece sont reconnus sur le gui_type : deux de ces
 * noms sont des noms historiques sans IOType. */
void TstCommon::registryMediaGuiTypes()
{
    QVERIFY(IOTypeRegistry::isMediaGuiType(QStringLiteral("audio_output")));
    QVERIFY(IOTypeRegistry::isMediaGuiType(QStringLiteral("camera_output")));
    QVERIFY(IOTypeRegistry::isMediaGuiType(QStringLiteral("fav_all_lights")));
    QVERIFY(IOTypeRegistry::isMediaGuiType(QStringLiteral("audio_player")));
    QVERIFY(IOTypeRegistry::isMediaGuiType(QStringLiteral("camera")));

    QVERIFY(!IOTypeRegistry::isMediaGuiType(QStringLiteral("light")));
    QVERIFY(!IOTypeRegistry::isMediaGuiType(QString()));
}

/* Table de verite figee de l'ancien RoomModel::detectOldGuiType() : ces 88
 * lignes sont celles du protocole historique, reprises une a une. */
void TstCommon::registryLegacyGuiType_data()
{
    QTest::addColumn<QString>("protocolType");
    QTest::addColumn<QString>("guiType");

    QTest::newRow("InputTime") << QStringLiteral("InputTime") << QStringLiteral("time");
    QTest::newRow("InPlageHoraire") << QStringLiteral("InPlageHoraire") << QStringLiteral("time_range");
    QTest::newRow("TimeRange") << QStringLiteral("TimeRange") << QStringLiteral("time_range");
    QTest::newRow("GpioInputSwitch") << QStringLiteral("GpioInputSwitch") << QStringLiteral("switch");
    QTest::newRow("GpioInputSwitchLongPress") << QStringLiteral("GpioInputSwitchLongPress") << QStringLiteral("switch_long");
    QTest::newRow("GpioInputSwitchTriple") << QStringLiteral("GpioInputSwitchTriple") << QStringLiteral("switch3");
    QTest::newRow("OWTemp") << QStringLiteral("OWTemp") << QStringLiteral("temp");
    QTest::newRow("WIAnalog") << QStringLiteral("WIAnalog") << QStringLiteral("analog_in");
    QTest::newRow("WagoInputAnalog") << QStringLiteral("WagoInputAnalog") << QStringLiteral("analog_in");
    QTest::newRow("WIDigitalBP") << QStringLiteral("WIDigitalBP") << QStringLiteral("switch");
    QTest::newRow("WIDigital") << QStringLiteral("WIDigital") << QStringLiteral("switch");
    QTest::newRow("WagoInputSwitch") << QStringLiteral("WagoInputSwitch") << QStringLiteral("switch");
    QTest::newRow("WIDigitalLong") << QStringLiteral("WIDigitalLong") << QStringLiteral("switch_long");
    QTest::newRow("WagoInputSwitchLongPress") << QStringLiteral("WagoInputSwitchLongPress") << QStringLiteral("switch_long");
    QTest::newRow("WIDigitalTriple") << QStringLiteral("WIDigitalTriple") << QStringLiteral("switch3");
    QTest::newRow("WagoInputSwitchTriple") << QStringLiteral("WagoInputSwitchTriple") << QStringLiteral("switch3");
    QTest::newRow("WITemp") << QStringLiteral("WITemp") << QStringLiteral("temp");
    QTest::newRow("WagoInputTemp") << QStringLiteral("WagoInputTemp") << QStringLiteral("temp");
    QTest::newRow("WebInputSwitch") << QStringLiteral("WebInputSwitch") << QStringLiteral("switch");
    QTest::newRow("WebInputAnalog") << QStringLiteral("WebInputAnalog") << QStringLiteral("analog_in");
    QTest::newRow("WebInputTemp") << QStringLiteral("WebInputTemp") << QStringLiteral("temp");
    QTest::newRow("WebInputString") << QStringLiteral("WebInputString") << QStringLiteral("string_in");
    QTest::newRow("ZibaseTemp") << QStringLiteral("ZibaseTemp") << QStringLiteral("temp");
    QTest::newRow("ZibaseAnalogIn") << QStringLiteral("ZibaseAnalogIn") << QStringLiteral("analog_in");
    QTest::newRow("ZibaseDigitalIn") << QStringLiteral("ZibaseDigitalIn") << QStringLiteral("switch");
    QTest::newRow("MySensorsInputAnalog") << QStringLiteral("MySensorsInputAnalog") << QStringLiteral("analog_in");
    QTest::newRow("MySensorsInputString") << QStringLiteral("MySensorsInputString") << QStringLiteral("string_in");
    QTest::newRow("MySensorsInputSwitch") << QStringLiteral("MySensorsInputSwitch") << QStringLiteral("switch");
    QTest::newRow("MySensorsInputSwitchLongPress") << QStringLiteral("MySensorsInputSwitchLongPress") << QStringLiteral("switch_long");
    QTest::newRow("MySensorsInputSwitchTriple") << QStringLiteral("MySensorsInputSwitchTriple") << QStringLiteral("switch3");
    QTest::newRow("MySensorsInputTemp") << QStringLiteral("MySensorsInputTemp") << QStringLiteral("temp");
    QTest::newRow("PingInputSwitch") << QStringLiteral("PingInputSwitch") << QStringLiteral("switch");
    QTest::newRow("KNXInputSwitch") << QStringLiteral("KNXInputSwitch") << QStringLiteral("switch");
    QTest::newRow("KNXInputAnalog") << QStringLiteral("KNXInputAnalog") << QStringLiteral("analog_in");
    QTest::newRow("KNXInputSwitchLongPress") << QStringLiteral("KNXInputSwitchLongPress") << QStringLiteral("switch_long");
    QTest::newRow("KNXInputSwitchTriple") << QStringLiteral("KNXInputSwitchTriple") << QStringLiteral("switch3");
    QTest::newRow("KNXInputTemp") << QStringLiteral("KNXInputTemp") << QStringLiteral("temp");
    QTest::newRow("OutputFake") << QStringLiteral("OutputFake") << QStringLiteral("light");
    QTest::newRow("GpioOutputSwitch") << QStringLiteral("GpioOutputSwitch") << QStringLiteral("light");
    QTest::newRow("GpioOutputShutter") << QStringLiteral("GpioOutputShutter") << QStringLiteral("shutter");
    QTest::newRow("GpioOutputShutterSmart") << QStringLiteral("GpioOutputShutterSmart") << QStringLiteral("shutter_smart");
    QTest::newRow("WOAnalog") << QStringLiteral("WOAnalog") << QStringLiteral("analog_out");
    QTest::newRow("WagoOutputAnalog") << QStringLiteral("WagoOutputAnalog") << QStringLiteral("analog_out");
    QTest::newRow("WODali") << QStringLiteral("WODali") << QStringLiteral("light_dimmer");
    QTest::newRow("WagoOutputDimmer") << QStringLiteral("WagoOutputDimmer") << QStringLiteral("light_dimmer");
    QTest::newRow("WODaliRVB") << QStringLiteral("WODaliRVB") << QStringLiteral("light_rgb");
    QTest::newRow("WagoOutputDimmerRGB") << QStringLiteral("WagoOutputDimmerRGB") << QStringLiteral("light_rgb");
    QTest::newRow("WODigital") << QStringLiteral("WODigital") << QStringLiteral("light");
    QTest::newRow("WagoOutputLight") << QStringLiteral("WagoOutputLight") << QStringLiteral("light");
    QTest::newRow("WOVolet") << QStringLiteral("WOVolet") << QStringLiteral("shutter");
    QTest::newRow("WagoOutputShutter") << QStringLiteral("WagoOutputShutter") << QStringLiteral("shutter");
    QTest::newRow("WOVoletSmart") << QStringLiteral("WOVoletSmart") << QStringLiteral("shutter_smart");
    QTest::newRow("WagoOutputShutterSmart") << QStringLiteral("WagoOutputShutterSmart") << QStringLiteral("shutter_smart");
    QTest::newRow("X10Output") << QStringLiteral("X10Output") << QStringLiteral("light");
    QTest::newRow("WebOutputString") << QStringLiteral("WebOutputString") << QStringLiteral("string_out");
    QTest::newRow("WebOutputLight") << QStringLiteral("WebOutputLight") << QStringLiteral("light");
    QTest::newRow("WebOutputLightRGB") << QStringLiteral("WebOutputLightRGB") << QStringLiteral("light_rgb");
    QTest::newRow("ZibaseDigitalOut") << QStringLiteral("ZibaseDigitalOut") << QStringLiteral("light");
    QTest::newRow("MySensorsOutputAnalog") << QStringLiteral("MySensorsOutputAnalog") << QStringLiteral("analog_out");
    QTest::newRow("MySensorsOutputDimmer") << QStringLiteral("MySensorsOutputDimmer") << QStringLiteral("light_dimmer");
    QTest::newRow("MySensorsOutputLight") << QStringLiteral("MySensorsOutputLight") << QStringLiteral("light");
    QTest::newRow("MySensorsOutputLightRGB") << QStringLiteral("MySensorsOutputLightRGB") << QStringLiteral("light_rgb");
    QTest::newRow("MySensorsOutputShutter") << QStringLiteral("MySensorsOutputShutter") << QStringLiteral("shutter");
    QTest::newRow("MySensorsOutputShutterSmart") << QStringLiteral("MySensorsOutputShutterSmart") << QStringLiteral("shutter_smart");
    QTest::newRow("MySensorsOutputString") << QStringLiteral("MySensorsOutputString") << QStringLiteral("string_out");
    QTest::newRow("OLAOutputLightDimmer") << QStringLiteral("OLAOutputLightDimmer") << QStringLiteral("light_dimmer");
    QTest::newRow("OLAOutputLightRGB") << QStringLiteral("OLAOutputLightRGB") << QStringLiteral("light_rgb");
    QTest::newRow("WOLOutputBool") << QStringLiteral("WOLOutputBool") << QStringLiteral("var_bool");
    QTest::newRow("KNXOutputLight") << QStringLiteral("KNXOutputLight") << QStringLiteral("light");
    QTest::newRow("KNXOutputAnalog") << QStringLiteral("KNXOutputAnalog") << QStringLiteral("analog_out");
    QTest::newRow("KNXOutputLightDimmer") << QStringLiteral("KNXOutputLightDimmer") << QStringLiteral("light_dimmer");
    QTest::newRow("KNXOutputLightRGB") << QStringLiteral("KNXOutputLightRGB") << QStringLiteral("light_rgb");
    QTest::newRow("KNXOutputShutter") << QStringLiteral("KNXOutputShutter") << QStringLiteral("shutter");
    QTest::newRow("KNXOutputShutterSmart") << QStringLiteral("KNXOutputShutterSmart") << QStringLiteral("shutter_smart");
    QTest::newRow("HueOutputLightRGB") << QStringLiteral("HueOutputLightRGB") << QStringLiteral("light_rgb");
    QTest::newRow("InputTimer") << QStringLiteral("InputTimer") << QStringLiteral("timer");
    QTest::newRow("Scenario") << QStringLiteral("Scenario") << QStringLiteral("scenario");
    QTest::newRow("InternalInt") << QStringLiteral("InternalInt") << QStringLiteral("var_int");
    QTest::newRow("InternalBool") << QStringLiteral("InternalBool") << QStringLiteral("var_bool");
    QTest::newRow("InternalString") << QStringLiteral("InternalString") << QStringLiteral("var_string");
    QTest::newRow("AVReceiver") << QStringLiteral("AVReceiver") << QStringLiteral("avreceiver");
    QTest::newRow("slim") << QStringLiteral("slim") << QStringLiteral("audio");
    QTest::newRow("Squeezebox") << QStringLiteral("Squeezebox") << QStringLiteral("audio");
    QTest::newRow("Axis") << QStringLiteral("Axis") << QStringLiteral("camera");
    QTest::newRow("Gadspot") << QStringLiteral("Gadspot") << QStringLiteral("camera");
    QTest::newRow("Planet") << QStringLiteral("Planet") << QStringLiteral("camera");
    QTest::newRow("StandardMjpeg") << QStringLiteral("StandardMjpeg") << QStringLiteral("camera");
    QTest::newRow("standard_mjpeg") << QStringLiteral("standard_mjpeg") << QStringLiteral("camera");
}

void TstCommon::registryLegacyGuiType()
{
    QFETCH(QString, protocolType);
    QFETCH(QString, guiType);

    QCOMPARE(IOTypeRegistry::legacyGuiType(protocolType), guiType);
}

//Un type inconnu (ou vide) ne produit pas de gui_type.
void TstCommon::registryLegacyGuiTypeUnknown()
{
    QVERIFY(IOTypeRegistry::legacyGuiType(QStringLiteral("NoSuchBackendClass")).isEmpty());
    QVERIFY(IOTypeRegistry::legacyGuiType(QString()).isEmpty());
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
