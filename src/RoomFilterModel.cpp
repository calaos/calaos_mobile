#include "RoomFilterModel.h"
#include "RoomModel.h"

RoomFilterModel::RoomFilterModel(QObject *parent):
    QSortFilterProxyModel(parent)
{
    set_filter(Common::FilterAll);
    set_scenarioVisible(true);

    connect(this, &RoomFilterModel::sourceChanged, [=](QObject *source)
    {
        RoomModel *rmodel = dynamic_cast<RoomModel *>(source);
        if (source && !rmodel)
            qWarning() << "Source model (" << source << ") is not a RoomModel, aborting!";
        setSourceModel(rmodel);

        connect(rmodel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(resetCache()));
        connect(rmodel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(resetCache()));
        connect(rmodel, SIGNAL(modelReset()), this, SLOT(resetCache()));

        resetCache();
    });
    connect(this, &RoomFilterModel::filterChanged, [=](Common::FilterModelType)
    {
        invalidate();
        resetCache();
    });
    connect(this, &RoomFilterModel::scenarioVisibleChanged, [=](bool)
    {
        invalidate();
        resetCache();
    });
    setDynamicSortFilter(true);
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

        if (obj->get_ioType() == Common::Shutter ||
            obj->get_ioType() == Common::ShutterSmart)
            shutters << obj;
        else if (obj->get_ioType() == Common::Light ||
                 obj->get_ioType() == Common::LightDimmer ||
                 obj->get_ioType() == Common::LightRgb)
            lights << obj;
        else if (obj->get_ioType() == Common::Temp ||
                 obj->get_ioType() == Common::AnalogIn ||
                 obj->get_ioType() == Common::VarInt)
            temps << obj;
        else
            other << obj;

        if (obj->get_ioType() == Common::ShutterSmart)
            totalCount += 3;
        else if (obj->get_ioType() == Common::LightDimmer ||
                 obj->get_ioType() == Common::LightRgb)
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
        if (leftCount < half)
        {
            leftCache[io->get_ioId()] = io;
            if (io->get_ioType() == Common::Light)
                leftCount += 1;
            else
                leftCount += 2;
        }
        else
        {
            rightCache[io->get_ioId()] = io;
            if (io->get_ioType() == Common::Light)
                rightCount += 1;
            else
                rightCount += 2;
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

    //* first sort by io type if they are different
    // shutter < temps < light < other
    if (lobj->get_ioType() != robj->get_ioType())
    {
        int l = 3, r = 3;

        if (shutters.contains(lobj)) l = 0;
        else if (temps.contains(lobj)) l = 1;
        else if (lights.contains(lobj)) l = 2;

        if (shutters.contains(robj)) r = 0;
        else if (temps.contains(robj)) r = 1;
        else if (lights.contains(robj)) r = 2;

        return l < r;
    }

    //prevent a bug when sorting equal items
    if (lobj->get_ioName() == robj->get_ioName())
        return lobj->get_ioId() < robj->get_ioId();

    //* then sort by io name
    return lobj->get_ioName() < robj->get_ioName();
}
