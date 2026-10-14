/*
 * IOBase : le point d'entrée/sortie, testé seul.
 *
 * Ce test est le témoin de T31. Avant lui, aucun test ne pouvait instancier un
 * IOBase sans lier soit HardwareUtils (+ QtWidgets, pour le QInputDialog), soit
 * quickflux : IOBase::askStateText() ouvrait la boîte de saisie elle-même et
 * son entrée dans la table de metacall de moc rendait la branche non
 * supprimable à l'édition de liens, quelle que soit l'étroitesse du test.
 *
 * Demander une valeur à l'utilisateur est désormais une décision du QML de
 * chaque variante (action ActionTypes.openAskTextForIo), qui rappelle
 * IOBase::sendStringValue() avec la réponse. Ce fichier vérifie les deux moitiés
 * du contrat qui restent en C++ :
 *   - sendStringValue() et ses voisines partent bien vers la connexion,
 *     puisque c'est ce que le QML appelle maintenant directement ;
 *   - IOBase n'expose plus askStateText() : la couche interface ne peut plus
 *     être rappelée depuis le modèle, et le couplage ne peut pas revenir sans
 *     faire tomber ce test.
 *
 * Il ne lie que RoomModel.cpp, Common.cpp et IOTypeRegistry.cpp — pas de
 * transport, pas de plateforme, pas de moteur QML.
 */

#include <QtTest>
#include <QVector>

#include "Common.h"
#include "IOConnection.h"
#include "RoomModel.h"

namespace {

struct SentCommand
{
    QString id;
    QString value;
    QString type;
    QString action;
    int seq = 0;
};

} //namespace

//Connexion factice : n'ouvre aucune socket, enregistre simplement ce qu'on lui
//demande d'envoyer.
class RecordingConnection: public QObject, public IOConnection
{
    Q_OBJECT

public:
    QObject *eventSource() override { return this; }
    bool isHttpApiV2() const override { return true; }

    void sendCommand(QString id, QString value, QString type, QString action) override
    {
        sent.append({ id, value, type, action, 0 });
    }

    void sendNumberedCommand(QString id, QString value, QString type, QString action, int seq) override
    {
        sent.append({ id, value, type, action, seq });
    }

    QVector<SentCommand> sent;

signals:
    void eventInputChange(QString id, QString key, QString value);
    void eventOutputChange(QString id, QString key, QString value);
    void eventIoStatusChange(QString id, QVariantMap statusData);
    void eventDeviceChange(QString id, QVariantMap device);
    void eventCommandRefused(QString id);
    void eventCommandAnswered(QString id, int seq, bool ok);
};

namespace {

QVariantMap climateItem(const QVariant &device)
{
    QVariantMap data;
    data["id"] = QStringLiteral("salon_ac");
    data["name"] = QStringLiteral("Clim salon");
    data["gui_type"] = QStringLiteral("climate");
    data["state"] = QStringLiteral("cool");
    data["rw"] = QStringLiteral("true");
    if (device.isValid())
        data["device"] = device;
    return data;
}

QVariantMap sampleDevice()
{
    return QJsonDocument::fromJson(R"({
        "family":"climate","impl":"proxy",
        "capabilities":{"modes":["off","cool"],"target_temp":{"min":16,"max":30,"step":0.5}},
        "state":{"mode":"cool","current_temp":23.4,"target_temp":21.0},
        "ios":{"mode":"salon_ac_mode"}})").object().toVariantMap();
}

} //namespace

class TstIOBase: public QObject
{
    Q_OBJECT

private:
    RecordingConnection *con = nullptr;

    //Un io "var_string" tel que RoomModel::load() le construirait, mais sans
    //moteur QML : IOBase n'en a plus besoin pour ces chemins.
    IOBase *makeVarString(int direction)
    {
        IOBase *io = new IOBase(nullptr, con, direction);

        QVariantMap data;
        data["id"] = QStringLiteral("io_str_1");
        data["name"] = QStringLiteral("Message cuisine");
        data["gui_type"] = QStringLiteral("var_string");
        data["hits"] = QStringLiteral("0");
        data["io_style"] = QString();
        data["unit"] = QString();
        data["rw"] = QStringLiteral("true");
        data["value_warning"] = QStringLiteral("false");
        io->load(data);

        return io;
    }

private slots:
    void init()
    {
        con = new RecordingConnection;
    }

    void cleanup()
    {
        delete con;
        con = nullptr;
    }

    /* T31 : le QML rappelle io.sendStringValue(texte) quand l'utilisateur a
     * validé la saisie. C'est le seul chemin de retour du dialogue, il doit
     * atteindre la connexion tel quel. */
    void sendStringValueReachesConnection()
    {
        QScopedPointer<IOBase> io(makeVarString(IOBase::IOOutput));

        io->sendStringValue(QStringLiteral("Bonjour"));

        QCOMPARE(con->sent.size(), 1);
        QCOMPARE(con->sent.at(0).id, QStringLiteral("io_str_1"));
        QCOMPARE(con->sent.at(0).value, QStringLiteral("Bonjour"));
        QCOMPARE(con->sent.at(0).type, QStringLiteral("output"));
        QCOMPARE(con->sent.at(0).action, QStringLiteral("set_state"));
    }

    //Un io d'entrée envoie sur "input" : la même saisie ne part pas au même
    //endroit selon le sens de l'io.
    void sendStringValueUsesIoDirection()
    {
        QScopedPointer<IOBase> io(makeVarString(IOBase::IOInput));

        io->sendStringValue(QStringLiteral("42"));

        QCOMPARE(con->sent.size(), 1);
        QCOMPARE(con->sent.at(0).type, QStringLiteral("input"));
    }

    //Annuler le dialogue ne doit rien envoyer : côté C++, cela se traduit par
    //"personne n'appelle sendStringValue", donc aucune commande.
    void nothingIsSentWithoutAnAnswer()
    {
        QScopedPointer<IOBase> io(makeVarString(IOBase::IOOutput));

        QCOMPARE(con->sent.size(), 0);
    }

    void sendIntValueReachesConnection()
    {
        QScopedPointer<IOBase> io(makeVarString(IOBase::IOOutput));

        io->sendIntValue(12.5);

        QCOMPARE(con->sent.size(), 1);
        //Le protocole attend "set <valeur>", pas la valeur nue.
        QCOMPARE(con->sent.at(0).value, QStringLiteral("set 12.5"));
        QCOMPARE(con->sent.at(0).action, QStringLiteral("set_state"));
    }

    void sendTrueAndSendFalseReachConnection()
    {
        QScopedPointer<IOBase> io(makeVarString(IOBase::IOOutput));

        io->sendTrue();
        io->sendFalse();

        QCOMPARE(con->sent.size(), 2);
        QCOMPARE(con->sent.at(0).value, QStringLiteral("true"));
        QCOMPARE(con->sent.at(1).value, QStringLiteral("false"));
    }

    /* Le garde-fou de T31 : IOBase ne doit plus exposer de méthode qui ouvre un
     * dialogue. Si askStateText() revient dans le méta-objet, le modèle sait de
     * nouveau qu'une interface existe et les .pro des tests devront de nouveau
     * lier HardwareUtils ou quickflux. */
    void ioBaseExposesNoDialogEntryPoint()
    {
        const QMetaObject &mo = IOBase::staticMetaObject;

        QCOMPARE(mo.indexOfMethod("askStateText()"), -1);
        QCOMPARE(mo.indexOfMethod("textDialogValid(QString)"), -1);
    }

    void refusalIsForwardedForOwnIdOnly()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QSignalSpy spy(io.data(), &IOBase::commandRefused);

        emit con->eventCommandRefused(QStringLiteral("another_io"));
        QCOMPARE(spy.count(), 0);
        emit con->eventCommandRefused(QStringLiteral("salon_ac"));
        QCOMPARE(spy.count(), 1);
    }

    void commandsAreNumberedAndTheirAnswerIsForwarded()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QSignalSpy spy(io.data(), &IOBase::commandAnswered);

        QCOMPARE(io->sendStringValue(QStringLiteral("heat")), 1);
        QCOMPARE(io->sendStringValue(QStringLiteral("{\"target_temp\":22}")), 2);
        QCOMPARE(con->sent.at(0).seq, 1);
        QCOMPARE(con->sent.at(1).seq, 2);
        QCOMPARE(con->sent.at(1).action, QStringLiteral("set_state"));

        emit con->eventCommandAnswered(QStringLiteral("another_io"), 1, false);
        QCOMPARE(spy.count(), 0);
        emit con->eventCommandAnswered(QStringLiteral("salon_ac"), 2, false);
        emit con->eventCommandAnswered(QStringLiteral("salon_ac"), 1, true);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(spy.at(0).at(1).toBool(), false);
        QCOMPARE(spy.at(1).at(0).toInt(), 1);
        QCOMPARE(spy.at(1).at(1).toBool(), true);
    }

    void numberingIsPerIo()
    {
        QScopedPointer<IOBase> a(new IOBase(nullptr, con, IOBase::IOOutput));
        QScopedPointer<IOBase> b(new IOBase(nullptr, con, IOBase::IOOutput));
        a->load(climateItem(sampleDevice()));
        b->load(climateItem(sampleDevice()));
        QCOMPARE(a->sendStringValue(QStringLiteral("x")), 1);
        QCOMPARE(a->sendStringValue(QStringLiteral("x")), 2);
        QCOMPARE(b->sendStringValue(QStringLiteral("x")), 1);
    }

    void deviceStampIsWhenTheFrameArrived()
    {
        static qint64 now = 1000;
        IOBase::setClock([]() -> qint64 { return now; });

        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QCOMPARE(io->getDeviceStamp(), qint64(1000));

        now = 61000;
        QVariantMap dev = sampleDevice();
        dev["state"] = QVariantMap{ { "mode", "heat" }, { "lockout", QVariantMap{ { "remaining", 90 } } } };
        emit con->eventDeviceChange(QStringLiteral("salon_ac"), dev);
        QCOMPARE(io->getDeviceStamp(), qint64(61000));

        // a frame for another io leaves it alone
        now = 99000;
        emit con->eventDeviceChange(QStringLiteral("another_io"), dev);
        QCOMPARE(io->getDeviceStamp(), qint64(61000));

        IOBase::setClock(nullptr);
    }

    void climateItemKeepsDeviceObject()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));

        QCOMPARE(io->get_ioType(), Common::Climate);
        QVERIFY(io->hasDevice());
        QCOMPARE(io->getDevice().value("impl").toString(), QStringLiteral("proxy"));
        QCOMPARE(io->getDeviceCapabilities().value("modes").toList().size(), 2);
        QCOMPARE(io->getDeviceState().value("mode").toString(), QStringLiteral("cool"));
        QCOMPARE(io->property("deviceState").toMap().value("target_temp").toDouble(), 21.0);

        //A clone (what the room models hold) keeps it too.
        QScopedPointer<IOBase> clone(io->cloneIO());
        QCOMPARE(clone->getDevice(), io->getDevice());
    }

    void itemWithoutDeviceHasNone()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(QVariant()));

        QVERIFY(!io->hasDevice());
        QVERIFY(io->getDevice().isEmpty());
        QVERIFY(io->getDeviceCapabilities().isEmpty());
        QVERIFY(io->getDeviceState().isEmpty());
    }

    void reloadWithoutDeviceDropsThePreviousOne()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        io->load(climateItem(QVariant()));

        QVERIFY(!io->hasDevice());
    }

    void malformedDeviceIsIgnored_data()
    {
        QTest::addColumn<QVariant>("device");
        QTest::newRow("string") << QVariant(QStringLiteral("oops"));
        QTest::newRow("list") << QVariant(QVariantList{ 1, 2 });
        QTest::newRow("empty object") << QVariant(QVariantMap());
        QTest::newRow("null") << QVariant::fromValue(nullptr);
    }

    void malformedDeviceIsIgnored()
    {
        QFETCH(QVariant, device);
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(device));

        QVERIFY(!io->hasDevice());
        QCOMPARE(io->getStateString(), QStringLiteral("cool"));
    }

    void deviceChangeReplacesWholesaleAndNotifies()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QSignalSpy spy(io.data(), &IOBase::deviceChange);

        emit con->eventDeviceChange(QStringLiteral("salon_ac"),
            { { "family", "climate" },
              { "state", QVariantMap{ { "mode", "heat" }, { "target_temp", 19.5 } } } });

        QCOMPARE(spy.count(), 1);
        QCOMPARE(io->getDeviceState().value("mode").toString(), QStringLiteral("heat"));
        QCOMPARE(io->getDeviceState().value("target_temp").toDouble(), 19.5);
        //Sections absent from the new object are gone.
        QVERIFY(io->getDeviceCapabilities().isEmpty());
        QVERIFY(!io->getDevice().contains("ios"));

        QScopedPointer<IOBase> clone(io->cloneIO());
        QCOMPARE(clone->getDevice(), io->getDevice());
    }

    void emptyDeviceEventIsIgnored()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QSignalSpy spy(io.data(), &IOBase::deviceChange);

        emit con->eventDeviceChange(QStringLiteral("salon_ac"), QVariantMap());

        QCOMPARE(spy.count(), 0);
        QVERIFY(io->hasDevice());
    }

    void loadNotifiesWhenDeviceAppearsDisappearsOrDiffers()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        QSignalSpy spy(io.data(), &IOBase::deviceChange);

        io->load(climateItem(QVariant()));
        QCOMPARE(spy.count(), 0);

        io->load(climateItem(sampleDevice()));
        QCOMPARE(spy.count(), 1);

        io->load(climateItem(sampleDevice()));
        QCOMPARE(spy.count(), 1);

        QVariantMap other = sampleDevice();
        other["impl"] = QStringLiteral("thermostat");
        io->load(climateItem(other));
        QCOMPARE(spy.count(), 2);

        io->load(climateItem(QVariant()));
        QCOMPARE(spy.count(), 3);
    }

    void emptyDeviceWarnsOnceNotOnClone()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("ignoring malformed device"));
        io->load(climateItem(QVariantMap()));

        QTest::failOnWarning();
        QScopedPointer<IOBase> clone(io->cloneIO());
        QVERIFY(!clone->hasDevice());
    }

    void deviceChangeForAnotherIoIsIgnored()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOOutput));
        io->load(climateItem(sampleDevice()));
        QSignalSpy spy(io.data(), &IOBase::deviceChange);

        emit con->eventDeviceChange(QStringLiteral("other"),
                                    { { "state", QVariantMap{ { "mode", "off" } } } });

        QCOMPARE(spy.count(), 0);
        QCOMPARE(io->getDeviceState().value("mode").toString(), QStringLiteral("cool"));
    }

    void deviceChangeCreatesDeviceWhenGetHomeHadNone()
    {
        QScopedPointer<IOBase> io(new IOBase(nullptr, con, IOBase::IOInput));
        io->load(climateItem(QVariant()));

        emit con->eventDeviceChange(QStringLiteral("salon_ac"), sampleDevice());

        QVERIFY(io->hasDevice());
    }

    //Ces modèles se construisent sans QQmlApplicationEngine : getItemModel()
    //appelait setObjectOwnership() à travers un pointeur de moteur nul, alors
    //que c'est un statique de QQmlEngine.
    void getItemModelWorksWithoutQmlEngine()
    {
        RoomModel model(nullptr, con);
        IOBase *io = makeVarString(IOBase::IOOutput);
        model.appendRow(io);

        QCOMPARE(model.getItemModel(0), static_cast<QObject *>(io));
        QCOMPARE(model.getItemModel(42), nullptr);
    }
};

QTEST_MAIN(TstIOBase)

#include "tst_iobase.moc"
