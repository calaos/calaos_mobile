#ifndef UPDATEMODEL_H
#define UPDATEMODEL_H

#include <QStandardItemModel>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QStringList>
#include "qqmlhelpers.h"

class UpdatePackageItem;
class UpdateGroupItem;
class UpdateProgressItem;

/*
 * Model listing the Calaos components ("calaos-*" packages) with their
 * installed/available version and per-package upgrade progress.
 * Rows are always kept sorted alphabetically by package name.
 */
class UpdatePackagesModel: public QStandardItemModel
{
public:
    explicit UpdatePackagesModel(QObject *parent = nullptr);
    virtual ~UpdatePackagesModel();

    enum
    {
        RoleName = Qt::UserRole + 1,
        RoleInstalledVersion,
        RoleNewVersion,
        RoleHasUpdate,
        RoleUpgradeState,
        RolePercent,
    };

    //Full rebuild from the set of installed calaos packages, preserving
    //any update info (hasUpdate/newVersion/upgradeState/percent) that was
    //already known for a given package name.
    //pkgs: {name: {"name":..., "version":...}}
    void setInstalled(const QJsonObject &pkgs);

    //Merge in the set of pending calaos updates.
    //calaosPkgs: {name: {"name":..., "version":<new>, "current_version":<installed>}}
    void mergeAvailable(const QJsonObject &calaosPkgs);

    void setPkgProgress(const QString &name, const QString &state, int percent);
    void clearProgress();

    //Optimistic post-upgrade reset: installedVersion becomes newVersion for
    //every package that had an update. Real data is expected to be re-fetched
    //separately afterwards.
    void clearAvailable();

    UpdatePackageItem *itemForPackage(const QString &name) const;

private:
    int sortedInsertPosition(const QString &name) const;
};

class UpdatePackageItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, pkgName, UpdatePackagesModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, pkgInstalledVersion, UpdatePackagesModel::RoleInstalledVersion)
    QML_READONLY_PROPERTY_MODEL(QString, pkgNewVersion, UpdatePackagesModel::RoleNewVersion)
    QML_READONLY_PROPERTY_MODEL(bool, pkgHasUpdate, UpdatePackagesModel::RoleHasUpdate)
    QML_READONLY_PROPERTY_MODEL(QString, pkgUpgradeState, UpdatePackagesModel::RoleUpgradeState)
    QML_READONLY_PROPERTY_MODEL(int, pkgPercent, UpdatePackagesModel::RolePercent)

public:
    UpdatePackageItem() {}
    virtual ~UpdatePackageItem() {}
};

/*
 * Model listing the synthetic (non-Calaos) update groups, as computed by
 * UpdateGrouping. Each row represents a bucket of system packages
 * (e.g. "kernel", "system") with a combined upgrade progress.
 */
class UpdateGroupsModel: public QStandardItemModel
{
public:
    explicit UpdateGroupsModel(QObject *parent = nullptr);
    virtual ~UpdateGroupsModel();

    enum
    {
        RoleGroupId = Qt::UserRole + 1,
        RolePkgCount,
        RolePackages,
        RoleUpgradeState,
        RolePercent,
    };

    //Full rebuild. Rows are ordered with "kernel" first (if present), then
    //the remaining groups alphabetically ("system" last).
    void setGroups(const QMap<QString, QStringList> &groups);

    void setGroupProgress(const QString &groupId, const QString &state, int percent);
    void clearProgress();

    UpdateGroupItem *itemForGroup(const QString &groupId) const;
};

class UpdateGroupItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, groupId, UpdateGroupsModel::RoleGroupId)
    QML_READONLY_PROPERTY_MODEL(int, groupPkgCount, UpdateGroupsModel::RolePkgCount)
    QML_READONLY_PROPERTY_MODEL(QStringList, groupPackages, UpdateGroupsModel::RolePackages)
    QML_READONLY_PROPERTY_MODEL(QString, groupUpgradeState, UpdateGroupsModel::RoleUpgradeState)
    QML_READONLY_PROPERTY_MODEL(int, groupPercent, UpdateGroupsModel::RolePercent)

public:
    UpdateGroupItem() {}
    virtual ~UpdateGroupItem() {}
};

/*
 * Model mirroring the per-package progress list ("packages" array) of the
 * daemon status snapshots for the ongoing upgrade. Rows are kept in the
 * array's order and updated in place, so the QML view only re-evaluates
 * the rows whose data actually changed instead of rebuilding all delegates
 * on every snapshot.
 */
class UpdateProgressModel: public QStandardItemModel
{
public:
    explicit UpdateProgressModel(QObject *parent = nullptr);
    virtual ~UpdateProgressModel();

    enum
    {
        RoleName = Qt::UserRole + 1,
        RoleState,
        RolePercent,
    };

    //In-place upsert from a snapshot's packages array, in the array's order:
    //new packages are inserted at their index, existing rows only get their
    //state/percent updated when changed, rows gone from the array are removed
    //(in practice the set only grows or stays the same during an upgrade).
    //packages: [{"name":..., "state":..., "percent":...}, ...]
    void syncPackages(const QJsonArray &packages);

    //QStandardItemModel::clear() is used between upgrades
};

class UpdateProgressItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, ppName, UpdateProgressModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, ppState, UpdateProgressModel::RoleState)
    QML_READONLY_PROPERTY_MODEL(int, ppPercent, UpdateProgressModel::RolePercent)

public:
    UpdateProgressItem() {}
    virtual ~UpdateProgressItem() {}
};

#endif // UPDATEMODEL_H
