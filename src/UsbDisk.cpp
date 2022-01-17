#include "UsbDisk.h"

#ifdef Q_OS_LINUX
#include "3rd_party/drivelist/src/drivelist.hpp"
#endif

UsbDiskModel::UsbDiskModel(QQmlApplicationEngine *eng, QObject *parent):
    QAbstractTableModel(parent),
    engine(eng)
{
    loadModel();
}

void UsbDiskModel::loadModel()
{
#ifdef Q_OS_LINUX
    QVector<UsbDisk *> disks;
    auto lst = Drivelist::ListStorageDevices();

    for (auto &i: lst)
    {
        QStringList mountpoints;
        for (auto &s: i.mountpoints)
        {
            mountpoints.append(QString::fromStdString(s));
        }

        if (i.size == 0)
            continue;

        auto d = new UsbDisk();

        d->set_physicalDevice(QString::fromStdString(i.device));
        d->set_name(QString::fromStdString(i.description));
        d->set_isRemovable(i.isRemovable);
        d->set_isSystem(i.isSystem);
        d->set_size(i.size);
        d->set_sizeHuman(UsbDisk::sizeHuman(i.size));
        d->set_volumes(mountpoints);
        d->set_isUsb(i.isUSB);
        d->set_isSD(i.isCard);

        disks.append(d);
    }

    beginResetModel();
    qDeleteAll(m_items);
    m_items.clear();
    m_items.append(disks);
    endResetModel();
#endif
}

const UsbDisk *UsbDiskModel::itemAt(int rowidx) const
{
    if (rowidx < 0 || rowidx >= m_items.size())
            return nullptr;
        return m_items.at(rowidx);
}

UsbDisk *UsbDiskModel::itemAt(int rowidx)
{
    if (rowidx < 0 || rowidx >= m_items.size())
            return nullptr;
    return m_items.at(rowidx);
}

void UsbDiskModel::clear()
{
    beginResetModel();
    qDeleteAll(m_items);
    m_items.clear();
    endResetModel();
}

QModelIndex UsbDiskModel::lastIndex()
{
    return index(rowCount() - 1, 0);
}

int UsbDiskModel::rowCount(const QModelIndex &) const
{
    return m_items.count();
}

int UsbDiskModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant UsbDiskModel::data(const QModelIndex &index, int role) const
{
    if (index.row() >= m_items.size())
        return {};

    auto *im = m_items.at(index.row());

    if (role == Qt::DisplayRole ||
        role == Qt::EditRole)
    {
        return QStringLiteral("%1 - %2")
                .arg(im->get_volumes().isEmpty()? "": im->get_volumes().join(", "),
                     im->get_name());
    }

    return {};
}

QVariant UsbDiskModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return tr("Removable disks");
    return {};
}

QObject *UsbDiskModel::getUsbModel(int idx) const
{
    if (idx < 0 || idx >= m_items.count())
        return nullptr;
    auto it = m_items.at(idx);
    engine->setObjectOwnership(it, QQmlEngine::JavaScriptOwnership);
    return it;
}

QString UsbDisk::sizeHuman(qint64 sz)
{
    double num = sz;
    QStringList list;
    list << "KB" << "MB" << "GB" << "TB";

    QStringListIterator i(list);
    QString unit("bytes");

    while(num >= 1024.0 && i.hasNext())
    {
        unit = i.next();
        num /= 1024.0;
    }
    return QString().setNum(num, 'f', 2) + " " + unit;
}

UsbDisk::UsbDisk():
    QObject()
{
}

UsbDisk::UsbDisk(UsbDisk *d)
{
    set_name(d->get_name());
    set_volumes(d->get_volumes());
    set_size(d->get_size());
    set_sizeHuman(sizeHuman(get_size()));
    set_sectorSize(d->get_sectorSize());
    set_physicalDevice(d->get_physicalDevice());
    set_isRemovable(d->get_isRemovable());
    set_isSystem(d->get_isSystem());
    set_isUsb(d->get_isUsb());
    set_isSD(d->get_isSD());
}
