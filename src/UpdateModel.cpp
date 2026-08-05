#include "UpdateModel.h"

#include <QSet>

UpdatePackagesModel::UpdatePackagesModel(QObject *parent):
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleName] = "pkgName";
    roles[RoleInstalledVersion] = "pkgInstalledVersion";
    roles[RoleNewVersion] = "pkgNewVersion";
    roles[RoleHasUpdate] = "pkgHasUpdate";
    roles[RoleUpgradeState] = "pkgUpgradeState";
    roles[RolePercent] = "pkgPercent";
    setItemRoleNames(roles);
}

UpdatePackagesModel::~UpdatePackagesModel()
{
}

void UpdatePackagesModel::setInstalled(const QJsonObject &pkgs)
{
    struct SavedInfo
    {
        QString installedVersion;
        QString newVersion;
        QString upgradeState;
        int percent = 0;
        bool hasUpdate = false;
    };

    //keep track of update info already known for existing rows so a refresh
    //of the installed set does not lose in-progress/known update state
    QHash<QString, SavedInfo> saved;
    for (int i = 0; i < rowCount(); i++)
    {
        UpdatePackageItem *it = static_cast<UpdatePackageItem *>(item(i));
        if (!it) continue;

        SavedInfo info;
        info.hasUpdate = it->get_pkgHasUpdate();
        info.installedVersion = it->get_pkgInstalledVersion();
        info.newVersion = it->get_pkgNewVersion();
        info.upgradeState = it->get_pkgUpgradeState();
        info.percent = it->get_pkgPercent();
        saved.insert(it->get_pkgName(), info);
    }

    clear();

    QStringList names = pkgs.keys();

    //rows that only exist through mergeAvailable (update available for a
    //package absent from the installed list, e.g. a brand new calaos
    //component) must survive the rebuild, or the visible list could
    //disagree with updatesCount
    for (auto sIt = saved.constBegin(); sIt != saved.constEnd(); ++sIt)
    {
        if (sIt.value().hasUpdate && !pkgs.contains(sIt.key()))
            names.append(sIt.key());
    }

    names.sort(Qt::CaseInsensitive);

    for (const QString &name: names)
    {
        QJsonObject obj = pkgs.value(name).toObject();

        UpdatePackageItem *it = new UpdatePackageItem();
        it->update_pkgName(obj.value("name").toString(name));
        it->update_pkgInstalledVersion(obj.value("version").toString());
        it->update_pkgNewVersion(QString());
        it->update_pkgHasUpdate(false);
        it->update_pkgUpgradeState(QString());
        it->update_pkgPercent(0);

        if (saved.contains(name))
        {
            const SavedInfo &info = saved.value(name);
            it->update_pkgHasUpdate(info.hasUpdate);
            it->update_pkgNewVersion(info.newVersion);
            it->update_pkgUpgradeState(info.upgradeState);
            it->update_pkgPercent(info.percent);

            //preserved available-only row: keep its current installed
            //version (typically the "current_version" from mergeAvailable)
            if (!pkgs.contains(name))
                it->update_pkgInstalledVersion(info.installedVersion);
        }

        appendRow(it);
    }
}

void UpdatePackagesModel::mergeAvailable(const QJsonObject &calaosPkgs)
{
    QSet<QString> updatedNames;

    for (auto it = calaosPkgs.constBegin(); it != calaosPkgs.constEnd(); ++it)
    {
        const QString &name = it.key();
        QJsonObject obj = it.value().toObject();
        QString newVersion = obj.value("version").toString();
        QString currentVersion = obj.value("current_version").toString();

        updatedNames.insert(name);

        UpdatePackageItem *pkgItem = itemForPackage(name);
        if (pkgItem)
        {
            pkgItem->update_pkgHasUpdate(true);
            pkgItem->update_pkgNewVersion(newVersion);

            if (pkgItem->get_pkgInstalledVersion().isEmpty())
                pkgItem->update_pkgInstalledVersion(currentVersion);
        }
        else
        {
            pkgItem = new UpdatePackageItem();
            pkgItem->update_pkgName(name);
            pkgItem->update_pkgInstalledVersion(currentVersion);
            pkgItem->update_pkgNewVersion(newVersion);
            pkgItem->update_pkgHasUpdate(true);
            pkgItem->update_pkgUpgradeState(QString());
            pkgItem->update_pkgPercent(0);

            insertRow(sortedInsertPosition(name), pkgItem);
        }
    }

    //rows not present in this update set are no longer pending an update
    for (int i = 0; i < rowCount(); i++)
    {
        UpdatePackageItem *pkgItem = static_cast<UpdatePackageItem *>(item(i));
        if (!pkgItem) continue;

        if (!updatedNames.contains(pkgItem->get_pkgName()))
        {
            pkgItem->update_pkgHasUpdate(false);
            pkgItem->update_pkgNewVersion(QString());
        }
    }
}

void UpdatePackagesModel::setPkgProgress(const QString &name, const QString &state, int percent)
{
    UpdatePackageItem *it = itemForPackage(name);
    if (!it) return;

    it->update_pkgUpgradeState(state);
    it->update_pkgPercent(percent);
}

void UpdatePackagesModel::clearProgress()
{
    for (int i = 0; i < rowCount(); i++)
    {
        UpdatePackageItem *it = static_cast<UpdatePackageItem *>(item(i));
        if (!it) continue;

        it->update_pkgUpgradeState(QString());
        it->update_pkgPercent(0);
    }
}

void UpdatePackagesModel::clearAvailable()
{
    for (int i = 0; i < rowCount(); i++)
    {
        UpdatePackageItem *it = static_cast<UpdatePackageItem *>(item(i));
        if (!it) continue;

        if (it->get_pkgHasUpdate())
        {
            it->update_pkgInstalledVersion(it->get_pkgNewVersion());
            it->update_pkgHasUpdate(false);
            it->update_pkgNewVersion(QString());
        }

        //reset progress on every row: a package upgraded as a pure
        //dependency carries a "done" state without ever having had an update
        it->update_pkgUpgradeState(QString());
        it->update_pkgPercent(0);
    }
}

UpdatePackageItem *UpdatePackagesModel::itemForPackage(const QString &name) const
{
    for (int i = 0; i < rowCount(); i++)
    {
        UpdatePackageItem *it = static_cast<UpdatePackageItem *>(item(i));
        if (it && it->get_pkgName() == name)
            return it;
    }

    return nullptr;
}

int UpdatePackagesModel::sortedInsertPosition(const QString &name) const
{
    int pos = 0;
    for (; pos < rowCount(); pos++)
    {
        UpdatePackageItem *it = static_cast<UpdatePackageItem *>(item(pos));
        if (it && QString::compare(it->get_pkgName(), name, Qt::CaseInsensitive) > 0)
            break;
    }

    return pos;
}

UpdateGroupsModel::UpdateGroupsModel(QObject *parent):
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleGroupId] = "groupId";
    roles[RolePkgCount] = "groupPkgCount";
    roles[RolePackages] = "groupPackages";
    roles[RoleUpgradeState] = "groupUpgradeState";
    roles[RolePercent] = "groupPercent";
    setItemRoleNames(roles);
}

UpdateGroupsModel::~UpdateGroupsModel()
{
}

void UpdateGroupsModel::setGroups(const QMap<QString, QStringList> &groups)
{
    clear();

    //deterministic order: "kernel" first, "system" last, everything else
    //alphabetically in between
    QStringList ids = groups.keys();
    ids.removeAll(QStringLiteral("kernel"));
    ids.removeAll(QStringLiteral("system"));
    ids.sort(Qt::CaseInsensitive);

    QStringList ordered;
    if (groups.contains(QStringLiteral("kernel")))
        ordered << QStringLiteral("kernel");
    ordered << ids;
    if (groups.contains(QStringLiteral("system")))
        ordered << QStringLiteral("system");

    for (const QString &id: ordered)
    {
        const QStringList &packages = groups.value(id);

        UpdateGroupItem *it = new UpdateGroupItem();
        it->update_groupId(id);
        it->update_groupPackages(packages);
        it->update_groupPkgCount(packages.size());
        it->update_groupUpgradeState(QString());
        it->update_groupPercent(0);

        appendRow(it);
    }
}

void UpdateGroupsModel::setGroupProgress(const QString &groupId, const QString &state, int percent)
{
    UpdateGroupItem *it = itemForGroup(groupId);
    if (!it) return;

    it->update_groupUpgradeState(state);
    it->update_groupPercent(percent);
}

void UpdateGroupsModel::clearProgress()
{
    for (int i = 0; i < rowCount(); i++)
    {
        UpdateGroupItem *it = static_cast<UpdateGroupItem *>(item(i));
        if (!it) continue;

        it->update_groupUpgradeState(QString());
        it->update_groupPercent(0);
    }
}

UpdateGroupItem *UpdateGroupsModel::itemForGroup(const QString &groupId) const
{
    for (int i = 0; i < rowCount(); i++)
    {
        UpdateGroupItem *it = static_cast<UpdateGroupItem *>(item(i));
        if (it && it->get_groupId() == groupId)
            return it;
    }

    return nullptr;
}

UpdateProgressModel::UpdateProgressModel(QObject *parent):
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleName] = "ppName";
    roles[RoleState] = "ppState";
    roles[RolePercent] = "ppPercent";
    setItemRoleNames(roles);
}

UpdateProgressModel::~UpdateProgressModel()
{
}

void UpdateProgressModel::syncPackages(const QJsonArray &packages)
{
    //names present in this snapshot
    QSet<QString> names;
    for (const QJsonValue &v: packages)
    {
        const QString n = v.toObject().value("name").toString();
        if (!n.isEmpty())
            names.insert(n);
    }

    //drop rows that disappeared from the snapshot (in practice the set only
    //grows or stays the same during an upgrade)
    for (int i = rowCount() - 1; i >= 0; i--)
    {
        UpdateProgressItem *it = static_cast<UpdateProgressItem *>(item(i));
        if (it && !names.contains(it->get_ppName()))
            removeRow(i);
    }

    //in-place upsert, keeping the array's order row by row
    int row = 0;
    for (const QJsonValue &v: packages)
    {
        const QJsonObject p = v.toObject();
        const QString name = p.value("name").toString();
        if (name.isEmpty())
            continue;

        UpdateProgressItem *it = static_cast<UpdateProgressItem *>(item(row));
        if (!it || it->get_ppName() != name)
        {
            //not the expected row: move the existing row up here if the
            //name lives further down (order changed), insert it otherwise
            int existing = -1;
            for (int i = row + 1; i < rowCount(); i++)
            {
                UpdateProgressItem *cand = static_cast<UpdateProgressItem *>(item(i));
                if (cand && cand->get_ppName() == name)
                {
                    existing = i;
                    break;
                }
            }

            if (existing >= 0)
            {
                insertRow(row, takeRow(existing));
                it = static_cast<UpdateProgressItem *>(item(row));
            }
            else
            {
                it = new UpdateProgressItem();
                it->update_ppName(name);
                it->update_ppState(QString());
                it->update_ppPercent(0);
                insertRow(row, it);
            }
        }

        //only touch rows whose data actually changed so the view does not
        //re-evaluate untouched delegates on every snapshot
        const QString state = p.value("state").toString();
        const int percent = p.value("percent").toInt();
        if (it->get_ppState() != state)
            it->update_ppState(state);
        if (it->get_ppPercent() != percent)
            it->update_ppPercent(percent);

        row++;
    }
}
