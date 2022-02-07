#ifndef CONTROLPANELMODEL_H
#define CONTROLPANELMODEL_H

#include <QStandardItemModel>
#include "qqmlhelpers.h"

class ControlPanelModelItem;
class Application;

class ControlPanelModel: public QStandardItemModel
{
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

    void load();

private:
    Application *calaosApp;
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
