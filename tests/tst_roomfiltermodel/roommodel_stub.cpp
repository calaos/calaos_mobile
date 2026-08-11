/*
 * Doublure de lien pour src/RoomModel.cpp.
 *
 * RoomFilterModel manipule RoomModel et IOBase par dynamic_cast : impossible de
 * le tester sans ces deux classes. Or src/RoomModel.cpp tire HardwareUtils et
 * quickflux (qfappdispatcher.h), donc QtQuick et les ressources de l'appli —
 * exactement ce que tests/README.md interdit de faire entrer dans un test.
 *
 * On lie donc le VRAI src/RoomModel.h (les déclarations, le moc et donc les
 * meta-objets/typeinfo utilisés par dynamic_cast sont ceux du dépôt) avec des
 * définitions minimales des seuls symboles réclamés par l'éditeur de liens.
 *
 * LIMITE ASSUMÉE : ce fichier n'est PAS testé, et il doit rester aligné sur les
 * déclarations de src/RoomModel.h. Si RoomModel.h change, le test ne compilera
 * plus — c'est voulu. La seule méthode dont le comportement compte pour
 * RoomFilterModel est RoomModel::getItemModel(), reproduite à l'identique de
 * src/RoomModel.cpp moins l'appel à QQmlEngine::setObjectOwnership() (pas de
 * moteur QML dans un test).
 */

#include "RoomModel.h"

//--- RoomModel ---------------------------------------------------------------

RoomModel::RoomModel(QQmlApplicationEngine *eng, IOConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "ioType";
    roles[RoleHits] = "ioHits";
    roles[RoleName] = "ioName";
    roles[RoleId] = "ioId";
    roles[RoleRoomName] = "roomName";
    roles[RoleStyle] = "ioStyle";
    setItemRoleNames(roles);
}

//Identique à src/RoomModel.cpp, sans l'appel au moteur QML.
QObject *RoomModel::getItemModel(int idx)
{
    return dynamic_cast<IOBase *>(item(idx));
}

void RoomModel::load(QVariantMap &, ScenarioModel *, int) {}
void RoomModel::temperatureIoDestroyed() {}
void RoomModel::temperatureIoChanged() {}

//--- ScenarioModel / ScenarioSortModel ---------------------------------------
// Jamais utilisés par le test, mais référencés par le moc de RoomModel.h.

ScenarioModel::ScenarioModel(QQmlApplicationEngine *eng, IOConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{}

QObject *ScenarioModel::getItemModel(int idx) { return dynamic_cast<IOBase *>(item(idx)); }

QObject *ScenarioSortModel::getItemModel(int idx)
{
    QStandardItemModel *m = dynamic_cast<QStandardItemModel *>(sourceModel());
    if (!m) return nullptr;
    return dynamic_cast<IOBase *>(m->item(indexToSource(idx)));
}

bool ScenarioSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    return QSortFilterProxyModel::lessThan(left, right);
}

//--- IOCache -----------------------------------------------------------------

IOBase *IOCache::searchInput(QString id) { return inputCache.value(id, nullptr); }
IOBase *IOCache::searchOutput(QString id) { return outputCache.value(id, nullptr); }
void IOCache::addInput(IOBase *) {}
void IOCache::addOutput(IOBase *) {}
void IOCache::delInput(IOBase *) {}
void IOCache::delOutput(IOBase *) {}
void IOCache::clearCache() { inputCache.clear(); outputCache.clear(); }

//--- IOBase ------------------------------------------------------------------

IOBase::IOBase(QQmlApplicationEngine *eng, IOConnection *con, int t):
    QStandardItem(),
    engine(eng),
    connection(con),
    ioType(t)
{
    update_ioType(static_cast<Common::IOType>(t));
    update_ioHits(0);
    update_rw(false);
    update_hasWarning(false);
}

IOBase *IOBase::cloneIO() const { return nullptr; }
void IOBase::load(const QVariantMap &io) { ioData = io; }
void IOBase::checkFirstState() {}

void IOBase::sendTrue() {}
void IOBase::sendFalse() {}
void IOBase::sendInc() {}
void IOBase::sendDec() {}
void IOBase::sendDown() {}
void IOBase::sendUp() {}
void IOBase::sendStop() {}
void IOBase::sendStringValue(QString) {}
void IOBase::sendIntValue(double) {}
void IOBase::sendColor(QColor) {}
void IOBase::sendRGB(int, int, int) {}
void IOBase::askStateText() {}

bool IOBase::getStateBool() { return false; }
double IOBase::getStateInt() { return 0.0; }
QString IOBase::getStateString() { return QString(); }
int IOBase::getStateRed() { return 0; }
int IOBase::getStateGreen() { return 0; }
int IOBase::getStateBlue() { return 0; }
int IOBase::getStateShutterPos() { return 0; }

void IOBase::inputChanged(QString, QString, QString) {}
void IOBase::outputChanged(QString, QString, QString) {}
void IOBase::ioStatusChanged(QString, QVariantMap) {}
void IOBase::textDialogValid(const QString &) {}
