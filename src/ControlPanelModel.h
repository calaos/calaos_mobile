#ifndef CONTROLPANELMODEL_H
#define CONTROLPANELMODEL_H

#include <QStandardItemModel>
#include "qqmlhelpers.h"

class ControlPanelModelItem;

class ControlPanelModel: public QStandardItemModel
{
    Q_OBJECT

public:
    ControlPanelModel(QObject *parent = nullptr);
    virtual ~ControlPanelModel();

    enum
    {
        RoleTitle = Qt::UserRole + 1,
        RoleSubTitle,
        RoleIcon,
        RoleClickId,
    };

    void setOptions(bool hasInstall, bool isSnapshotBoot);
    void retranslate();

private:
    struct Entry
    {
        QString title, subTitle, icon, clickId;
    };

    QList<Entry> entries() const;
    void load();

    bool m_hasInstall = false;
    bool m_isSnapshotBoot = false;
};

class ControlPanelModelItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, cpTitle, ControlPanelModel::RoleTitle)
    QML_READONLY_PROPERTY_MODEL(QString, cpSubTitle, ControlPanelModel::RoleSubTitle)
    QML_READONLY_PROPERTY_MODEL(QString, cpIcon, ControlPanelModel::RoleIcon)
    QML_READONLY_PROPERTY_MODEL(QString, cpClickId, ControlPanelModel::RoleClickId)

public:
    ControlPanelModelItem() {}
    virtual ~ControlPanelModelItem() {}
};

#endif // CONTROLPANELMODEL_H
