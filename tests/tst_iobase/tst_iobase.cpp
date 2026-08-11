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
        sent.append({ id, value, type, action });
    }

    QVector<SentCommand> sent;

signals:
    void eventInputChange(QString id, QString key, QString value);
    void eventOutputChange(QString id, QString key, QString value);
    void eventIoStatusChange(QString id, QVariantMap statusData);
};

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
