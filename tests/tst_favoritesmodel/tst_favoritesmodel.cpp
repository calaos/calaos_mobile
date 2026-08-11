/*
 * Unit tests for src/FavoritesModel.cpp - persistence of the favorite types
 * (T19).
 *
 * Bug reproduced: FavoritesModel::save() only serialized the favorites whose
 * type was Common::FavIO and printed "TODO!" for every other one, and
 * FavoritesModel::addFavorite() returned false for them without doing
 * anything. Any favorite of another Common::FavoriteType present in the saved
 * list (FavLightsCount, FavShutterCount, FavAudio, FavCamera) was therefore
 * silently deleted from app/favorites at the next saveSettings().
 *
 * Fix: save() serializes every row (type + id of the IO the favorite points
 * at), addFavorite() rebuilds the row from the IO cache for every known type,
 * and the entries that cannot be turned into a row are kept aside and written
 * back untouched at their original position instead of being dropped.
 *
 * Link perimeter: see tst_favoritesmodel.pro. The model rebuilds its rows from
 * IOCache/IOBase::cloneIO(), so the tests populate IOCache::Instance() with
 * real IOBase objects (engine is nullptr: nothing here goes through QML, and
 * FavoritesModel only dereferences the engine in getItemModel(), which the
 * tests do not call - they check the same dynamic_cast<IOBase *> it does).
 *
 * T18: IOBase takes the src/IOConnection.h interface instead of a
 * CalaosConnection *, so this test provides its own FakeConnection and links
 * neither CalaosConnection.cpp nor CalaosEventDecoder.cpp any more.
 * FavoritesModel itself still takes a CalaosConnection * (src/FavoritesModel.h,
 * outside the T18 perimeter) but never dereferences it - only HomeFavModel
 * does, and it is not exercised here - so nullptr is passed for it.
 */

#include <QtTest>
#include <QVariantList>
#include <QVariantMap>

#include "FavoritesModel.h"
#include "RoomModel.h"
#include "IOConnection.h"
#include "Common.h"

//Everything an IOBase asks of a connection (src/IOConnection.h): send a
//command, tell the API version, carry the three io event signals. No socket,
//no network, no state.
class FakeConnection: public QObject, public IOConnection
{
    Q_OBJECT

public:
    QObject *eventSource() override { return this; }
    bool isHttpApiV2() const override { return true; }
    void sendCommand(QString, QString, QString, QString) override {}

signals:
    void eventInputChange(QString id, QString key, QString value);
    void eventOutputChange(QString id, QString key, QString value);
    void eventIoStatusChange(QString id, QVariantMap statusData);
};

namespace
{

//Creates an output IO and registers it in the cache, the way RoomModel::load()
//does for every item of the home.
IOBase *addCachedOutput(IOConnection *con, const QString &ioId, const QString &guiType)
{
    IOBase *io = new IOBase(nullptr, con, IOBase::IOOutput);

    QVariantMap data;
    data["id"] = ioId;
    data["name"] = ioId;
    data["gui_type"] = guiType;
    data["hits"] = QStringLiteral("0");
    data["io_style"] = QString();
    data["unit"] = QString();
    data["rw"] = QStringLiteral("true");
    data["value_warning"] = QStringLiteral("false");
    io->load(data);
    io->update_room_name(QStringLiteral("Salon"));

    IOCache::Instance().addOutput(io);

    return io;
}

QVariantMap favEntry(const QString &ioId, int type)
{
    QVariantMap fav;
    fav["id"] = ioId;
    fav["type"] = type;
    return fav;
}

} //namespace

class TstFavoritesModel: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void favIoIsSavedAsBefore();
    void allLightsCountFavoriteIsRebuilt();
    void audioAndCameraFavoritesAreSaved();
    void unsupportedFavoriteIsKeptAtItsPlace();
    void unknownFavoriteTypeIsKept();
    void clearAlsoDropsKeptFavorites();

private:
    FakeConnection *connection = nullptr;
};

void TstFavoritesModel::init()
{
    connection = new FakeConnection();
    IOCache::Instance().clearCache();
}

void TstFavoritesModel::cleanup()
{
    IOCache::Instance().clearCache();
    delete connection;
    connection = nullptr;
}

//The historical FavIO path must not change: one row, and {id, type} written
//back.
void TstFavoritesModel::favIoIsSavedAsBefore()
{
    addCachedOutput(connection, QStringLiteral("io_light"), QStringLiteral("light"));

    FavoritesModel model(nullptr, nullptr);

    QVERIFY(model.addFavorite(QStringLiteral("io_light"), Common::FavIO));
    QCOMPARE(model.rowCount(), 1);

    QVariantList saved = model.save();
    QCOMPARE(saved.size(), 1);
    QCOMPARE(saved.at(0).toMap()["id"].toString(), QStringLiteral("io_light"));
    QCOMPARE(saved.at(0).toMap()["type"].toInt(), (int)Common::FavIO);

    //A favorite pointing at an IO that no longer exists is still dropped.
    QVERIFY(!model.addFavorite(QStringLiteral("io_gone"), Common::FavIO));
    QCOMPARE(model.rowCount(), 1);
}

//A FavLightsCount favorite points at the "fav_all_lights" pseudo output built
//by HomeFavModel::load(). It must come back as a real IOBase row (the QML
//Loader binds modelData to getItemModel(), which dynamic_cast<IOBase *>) whose
//RoleIOType is Common::FavoritesLightsCount - that is the role
//qml/SharedComponents/ItemListView.qml switches on to pick IOFavAllLights.
void TstFavoritesModel::allLightsCountFavoriteIsRebuilt()
{
    addCachedOutput(connection, QStringLiteral("fav_all_lights"), QStringLiteral("fav_all_lights"));

    FavoritesModel model(nullptr, nullptr);

    QVariantList favList;
    favList.append(favEntry(QStringLiteral("fav_all_lights"), Common::FavLightsCount));
    model.load(favList);

    QCOMPARE(model.rowCount(), 1);
    QVERIFY(dynamic_cast<IOBase *>(model.item(0)) != nullptr);
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleType).toInt(), (int)Common::FavLightsCount);
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleIOType).toInt(), (int)Common::FavoritesLightsCount);
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleId).toString(), QStringLiteral("fav_all_lights"));
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleName).toString(), QStringLiteral("fav_all_lights"));

    //Round trip: before the fix save() dropped it and printed "TODO!".
    QCOMPARE(model.save(), favList);
}

//Same for the favorites pointing at an audio player or a camera: they are
//regular outputs of the home, so they are rebuilt from the cache too.
void TstFavoritesModel::audioAndCameraFavoritesAreSaved()
{
    addCachedOutput(connection, QStringLiteral("io_player"), QStringLiteral("audio_output"));
    addCachedOutput(connection, QStringLiteral("io_cam"), QStringLiteral("camera_output"));

    FavoritesModel model(nullptr, nullptr);

    QVariantList favList;
    favList.append(favEntry(QStringLiteral("io_player"), Common::FavAudio));
    favList.append(favEntry(QStringLiteral("io_cam"), Common::FavCamera));
    model.load(favList);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleType).toInt(), (int)Common::FavAudio);
    QCOMPARE(model.item(1)->data(FavoritesModel::RoleType).toInt(), (int)Common::FavCamera);
    QCOMPARE(model.save(), favList);
}

//FavShutterCount has no counterpart in Common::IOType and no QML delegate, so
//nothing can be displayed for it: it is not added as a row, but it must
//survive the save/load round trip, at its position and with the extra keys it
//was stored with.
void TstFavoritesModel::unsupportedFavoriteIsKeptAtItsPlace()
{
    addCachedOutput(connection, QStringLiteral("io_light1"), QStringLiteral("light"));
    addCachedOutput(connection, QStringLiteral("io_light2"), QStringLiteral("light"));

    FavoritesModel model(nullptr, nullptr);

    QVariantMap shutters = favEntry(QStringLiteral("all_shutters"), Common::FavShutterCount);
    shutters["extra_data"] = QStringLiteral("kept as is");

    QVariantList favList;
    favList.append(favEntry(QStringLiteral("io_light1"), Common::FavIO));
    favList.append(shutters);
    favList.append(favEntry(QStringLiteral("io_light2"), Common::FavIO));

    model.load(favList);

    //Only the two displayable favorites are rows.
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.item(0)->data(FavoritesModel::RoleId).toString(), QStringLiteral("io_light1"));
    QCOMPARE(model.item(1)->data(FavoritesModel::RoleId).toString(), QStringLiteral("io_light2"));

    //...but nothing is lost: before the fix save() returned only the two FavIO
    //entries and the third one disappeared from the settings.
    QCOMPARE(model.save(), favList);

    //And it survives further round trips unchanged.
    model.load(model.save());
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.save(), favList);
}

//A type this build does not know at all (newer app, hand edited settings) is
//kept the same way rather than silently deleted.
void TstFavoritesModel::unknownFavoriteTypeIsKept()
{
    FavoritesModel model(nullptr, nullptr);

    QVariantList favList;
    favList.append(favEntry(QStringLiteral("something"), 4242));
    model.load(favList);

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.save(), favList);
}

//clear() must drop the kept favorites along with the rows, otherwise save()
//would keep writing entries the model no longer holds (logout path in
//Application).
void TstFavoritesModel::clearAlsoDropsKeptFavorites()
{
    FavoritesModel model(nullptr, nullptr);

    QVariantList favList;
    favList.append(favEntry(QStringLiteral("all_shutters"), Common::FavShutterCount));
    model.load(favList);
    QCOMPARE(model.save().size(), 1);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.save().isEmpty());
}

QTEST_MAIN(TstFavoritesModel)

#include "tst_favoritesmodel.moc"
