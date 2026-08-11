#include "RoomFilterModel.h"
#include "RoomModel.h"
#include "IOTypeRegistry.h"

namespace
{

/* Display families of a room view, listed in display order. The same ranking
 * drives resetCache() (which side of the view an IO goes to) and lessThan()
 * (in which order the IOs are shown), so the two cannot disagree. The families
 * themselves come from the registry (src/IOTypeRegistry.h). */
enum IoRank
{
    RankScenario = 0,
    RankShutter,
    RankMeasurement,
    RankLight,
    RankOther,
};

int ioRank(Common::IOType t)
{
    const IOTypeRegistry::Category cat = IOTypeRegistry::category(t);

    if (cat == IOTypeRegistry::Category::Scenario) return RankScenario;
    if (cat == IOTypeRegistry::Category::Shutter) return RankShutter;
    if (IOTypeRegistry::isMeasurement(t)) return RankMeasurement;
    if (IOTypeRegistry::isLight(t)) return RankLight;

    return RankOther;
}

} //namespace

RoomFilterModel::RoomFilterModel(QObject *parent):
    QSortFilterProxyModel(parent)
{
    set_filter(Common::FilterAll);
    set_scenarioVisible(true);

    connect(this, &RoomFilterModel::sourceChanged, this, [=](QObject *source)
    {
        RoomModel *rmodel = dynamic_cast<RoomModel *>(source);
        if (source && !rmodel)
            qWarning() << "Source model (" << source << ") is not a RoomModel, aborting!";
        setSourceModel(rmodel);

        connect(rmodel, &RoomModel::rowsInserted, this, &RoomFilterModel::resetCache);
        connect(rmodel, &RoomModel::rowsRemoved, this, &RoomFilterModel::resetCache);
        connect(rmodel, &RoomModel::modelReset, this, &RoomFilterModel::resetCache);

        resetCache();
        invalidate();
    });
    connect(this, &RoomFilterModel::filterChanged, [=](Common::FilterModelType)
    {
        resetCache();
        invalidate();
    });
    connect(this, &RoomFilterModel::scenarioVisibleChanged, [=](bool)
    {
        resetCache();
        invalidate();
    });

    setDynamicSortFilter(true);
    sort(0);
}

int RoomFilterModel::indexToSource(int idx)
{
    return mapToSource(index(idx, 0)).row();
}

int RoomFilterModel::indexFromSource(int idx)
{
    return mapFromSource(index(idx, 0)).row();
}

QObject *RoomFilterModel::getItemModel(int idx)
{
    RoomModel *rmodel = dynamic_cast<RoomModel *>(sourceModel());
    if (rmodel)
        return rmodel->getItemModel(indexToSource(idx));

    return nullptr;
}

void RoomFilterModel::resetCache()
{
    leftCache.clear();
    rightCache.clear();

    /* Those lists are rebuilt from scratch on every call. They MUST be cleared
     * here: resetCache() is triggered by rowsInserted/rowsRemoved/modelReset and
     * by every filter change. Without this, they would keep growing and would
     * hold pointers to IOBase objects already destroyed by the source model
     * (lessThan() dereferences/compares them). */
    shutters.clear();
    lights.clear();
    temps.clear();
    other.clear();

    /* Smart sorting of IO between left and right listview
     * Rules to follow:
     * - Group all shutter in left
     * - Balance half IO count between left/right
     * - 10 IOs is the number of visible items in left/right
     */

    RoomModel *rmodel = dynamic_cast<RoomModel *>(sourceModel());
    if (!rmodel) return;

    int leftCount = 0, rightCount = 0;
    int totalCount = 0;

    for (int i = 0;i < rmodel->rowCount();i++)
    {
        IOBase *obj = dynamic_cast<IOBase *>(rmodel->getItemModel(i));
        if (!obj)
        {
            qWarning() << "Item" << i << "of source model is not an IOBase, skipping it";
            continue;
        }

        const Common::IOType ioType = obj->get_ioType();

        switch (ioRank(ioType))
        {
        case RankShutter: shutters << obj; break;
        case RankMeasurement: temps << obj; break;
        case RankLight: lights << obj; break;
        default: other << obj; break; //scenarios included, as before
        }

        //A smart shutter and a dimmable light take more rows on screen than a
        //plain one: they weigh more in the left/right balance.
        if (ioType == Common::ShutterSmart)
            totalCount += 3;
        else if (IOTypeRegistry::isDimmableLight(ioType))
            totalCount += 2;
        else
        {
            if (!get_scenarioVisible())
            {
                if (obj->get_ioType() != Common::Scenario)
                    totalCount += 1;
            }
            else
                totalCount += 1;
        }
    }

    //Add shutters to left
    foreach (IOBase *io, shutters)
    {
        leftCache[io->get_ioId()] = io;
        if (io->get_ioType() == Common::Shutter)
            leftCount += 1;
        else
            leftCount += 3;
    }

    //Add temps to right
    foreach (IOBase *io, temps)
    {
        rightCache[io->get_ioId()] = io;
        rightCount += 1;
    }

    int half = totalCount / 2 ;

    foreach (IOBase *io, lights)
    {
        //Weights must match the ones used for totalCount above.
        const int weight = IOTypeRegistry::isDimmableLight(io->get_ioType())? 2: 1;

        if (leftCount < half)
        {
            leftCache[io->get_ioId()] = io;
            leftCount += weight;
        }
        else
        {
            rightCache[io->get_ioId()] = io;
            rightCount += weight;
        }
    }

    foreach (IOBase *io, other)
    {
        if (leftCount <= half)
        {
            leftCache[io->get_ioId()] = io;
            leftCount++;
        }
        else
        {
            rightCache[io->get_ioId()] = io;
            rightCount++;
        }
    }
}

bool RoomFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (get_filter() == Common::FilterAll)
        return true;

    RoomModel *rmodel = dynamic_cast<RoomModel *>(sourceModel());
    if (!rmodel) return false;

    IOBase *obj = dynamic_cast<IOBase *>(rmodel->getItemModel(source_row));
    if (!obj) return false;

    if (get_filter() == Common::FilterLeft &&
        leftCache.contains(obj->get_ioId()))
    {
        if (!get_scenarioVisible() && obj->get_ioType() == Common::Scenario)
            return false;
        return true;
    }

    if (get_filter() == Common::FilterRight &&
        rightCache.contains(obj->get_ioId()))
    {
        if (!get_scenarioVisible() && obj->get_ioType() == Common::Scenario)
            return false;
        return true;
    }

    if (get_filter() == Common::FilterScenario &&
        obj->get_ioType() == Common::Scenario)
    {
        return true;
    }

    return false;
}

bool RoomFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    RoomModel *rmodel = dynamic_cast<RoomModel *>(sourceModel());
    if (!rmodel) return false;

    IOBase *lobj = dynamic_cast<IOBase *>(rmodel->itemFromIndex(left));
    IOBase *robj = dynamic_cast<IOBase *>(rmodel->itemFromIndex(right));

    /* An invalid index or a non-IOBase item gives a null pointer here. Keep a
     * stable and transitive order in that case: non-IOBase items sort last. */
    if (!lobj || !robj)
        return lobj && !robj;

    qDebug() << "sort '" << lobj->get_ioName() << "' < '" << robj->get_ioName() << "'";

    //* first sort by io type if they are different
    // scenarios < shutter < temps < light < other
    if (lobj->get_ioType() != robj->get_ioType())
        return ioRank(lobj->get_ioType()) < ioRank(robj->get_ioType());

    //prevent a bug when sorting equal items
    if (lobj->get_ioName() == robj->get_ioName())
        return lobj->get_ioId() < robj->get_ioId();

    //* then sort by io name
    return lobj->get_ioName() < robj->get_ioName();
}
