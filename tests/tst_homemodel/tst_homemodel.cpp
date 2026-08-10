/*
 * Tests unitaires de src/HomeModel.cpp — LightOnModel::onCache (T06).
 *
 * Bug reproduit : LightOnModel maintient un QHash<QString, IOBase *> onCache
 * pour dedupliquer les lumieres allumees (HomeModel.h). Avant le correctif,
 * HomeModel::load() appelait lightOnModel->clear(), qui (herite de
 * QStandardItemModel) ne vidait que les lignes du modele, jamais onCache.
 * Consequence apres une reconnexion (logout/login) : LightOnModel::addLight()
 * voit onCache.contains(ioId) vrai pour une entree perimee et refuse de
 * reajouter une lumiere pourtant allumee -> compteur de lumieres fantome
 * (bloque a 0 au lieu de refleter les lumieres reellement allumees).
 *
 * Correctif : LightOnModel::clear() masque desormais
 * QStandardItemModel::clear() (non virtuelle) et vide onCache en meme temps
 * que les lignes. Comme lightOnModel est toujours declare LightOnModel*
 * (HomeModel.h, Application.h), tous les appels existants a
 * lightOnModel->clear() beneficient du correctif sans etre modifies.
 *
 * Perimetre du lien (voir tst_homemodel.pro) : ce test appelle le vrai
 * LightOnModel::addLight(IOBase *)/removeLight()/clear(), donc a besoin d'un
 * vrai IOBase. IOBase est defini dans src/RoomModel.cpp ; ses methodes sont
 * Q_INVOKABLE/slots (addLight, removeLight, getItemModel, askStateText...),
 * donc referencees inconditionnellement par la table de dispatch generee par
 * moc, meme quand ce test ne les appelle pas - impossible de les faire
 * elaguer par l'editeur de liens. RoomModel.cpp entraine donc
 * CalaosConnection.cpp (utilise par le constructeur d'IOBase) et, via
 * IOBase::askStateText(), soit HardwareUtils (+QtWidgets), soit quickflux
 * (~20 fichiers) ; ce test choisit HardwareUtils (DEFINES += CALAOS_MOBILE
 * dans le .pro), le plus leger des deux. Aucun de ces objets ne fait d'appel
 * reseau ou d'E/S ici : on ne fait que les construire.
 */

#include <QtTest>

#include "HomeModel.h"
#include "RoomModel.h"
#include "CalaosConnection.h"

namespace {

//Construit un IOBase "lumiere allumee" minimal, comme le ferait RoomModel en
//parsant la reponse JSON du serveur (cf. RoomModel::load()).
IOBase *makeLight(CalaosConnection *con, const QString &ioId, const QString &roomName)
{
    IOBase *io = new IOBase(nullptr, con, IOBase::IOOutput);

    QVariantMap data;
    data["id"] = ioId;
    data["name"] = ioId;
    data["gui_type"] = QStringLiteral("light");
    data["hits"] = QStringLiteral("0");
    data["io_style"] = QString();
    data["unit"] = QString();
    data["rw"] = QStringLiteral("true");
    data["value_warning"] = QStringLiteral("false");
    io->load(data);
    io->update_room_name(roomName);

    return io;
}

} //namespace

class TstHomeModel: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void clearAlsoEmptiesCache();
    void ghostLightsAfterReconnect();

private:
    CalaosConnection *connection = nullptr;
};

void TstHomeModel::init()
{
    connection = new CalaosConnection();
}

void TstHomeModel::cleanup()
{
    delete connection;
    connection = nullptr;
}

//Verification ciblee : clear() doit vider les lignes ET onCache, pas juste
//les lignes (comportement herite de QStandardItemModel avant le correctif).
void TstHomeModel::clearAlsoEmptiesCache()
{
    LightOnModel model(nullptr, connection);

    IOBase *salon = makeLight(connection, QStringLiteral("io_salon_1"), QStringLiteral("Salon"));
    IOBase *cuisine = makeLight(connection, QStringLiteral("io_cuisine_1"), QStringLiteral("Cuisine"));

    model.addLight(salon);
    model.addLight(cuisine);
    QCOMPARE(model.rowCount(), 2);

    model.clear();
    QCOMPARE(model.rowCount(), 0);

    //Si onCache n'a pas ete vide par clear(), re-ajouter la meme lumiere est
    //silencieusement refuse par addLight() (HomeModel.cpp) : rowCount()
    //resterait a 0 au lieu de repasser a 1. C'est exactement le bug T06.
    IOBase *salonAgain = makeLight(connection, QStringLiteral("io_salon_1"), QStringLiteral("Salon"));
    model.addLight(salonAgain);
    QCOMPARE(model.rowCount(), 1);

    delete salon;
    delete cuisine;
}

//Reproduit le scenario du ticket T06 : deux cycles load -> clear -> load avec
//les memes lumieres allumees ; le compteur (rowCount()) doit etre identique
//apres chaque cycle (pas 0, pas double).
void TstHomeModel::ghostLightsAfterReconnect()
{
    LightOnModel model(nullptr, connection);

    auto lightCycle = [&]()
    {
        //Le serveur renvoie l'etat courant : les deux memes lumieres sont
        //allumees (memes id, comme apres une reconnexion sans rien changer
        //physiquement). addLight() clone l'IO qu'on lui passe (voir
        //HomeModel.cpp) sans en prendre possession : on detruit donc les
        //originaux nous-memes une fois ajoutes.
        IOBase *salon = makeLight(connection, QStringLiteral("io_salon_1"), QStringLiteral("Salon"));
        IOBase *cuisine = makeLight(connection, QStringLiteral("io_cuisine_1"), QStringLiteral("Cuisine"));
        model.addLight(salon);
        model.addLight(cuisine);
        delete salon;
        delete cuisine;
    };

    //Cycle 1 : deux lumieres s'allument.
    lightCycle();
    QCOMPARE(model.rowCount(), 2);

    //Reconnexion : HomeModel::load() appelle lightOnModel->clear().
    model.clear();
    QCOMPARE(model.rowCount(), 0);

    //Cycle 2 : memes lumieres toujours allumees apres la reconnexion.
    lightCycle();
    QCOMPARE(model.rowCount(), 2);

    //Un 3e cycle confirme que ce n'est pas un simple decalage d'un cran.
    model.clear();
    lightCycle();
    QCOMPARE(model.rowCount(), 2);
}

QTEST_GUILESS_MAIN(TstHomeModel)
#include "tst_homemodel.moc"
