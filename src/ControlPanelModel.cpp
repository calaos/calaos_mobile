#include "ControlPanelModel.h"
#include "Application.h"

ControlPanelModel::ControlPanelModel(QObject *parent):
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleTitle] = "cpTitle";
    roles[RoleSubTitle] = "cpSubTitle";
    roles[RoleIcon] = "cpIcon";
    roles[RoleClickId] = "cpClickId";
    setItemRoleNames(roles);

    calaosApp = static_cast<Application *>(qApp);
    load();
}

ControlPanelModel::~ControlPanelModel()
{
}

void ControlPanelModel::load()
{
    ControlPanelModelItem *it;

    it = new ControlPanelModelItem();
    it->update_cpTitle(tr("Screen saver"));
    it->update_cpSubTitle(tr("Screen power management"));
    it->update_cpIcon("qrc:/img/icon_item_screensaver.png");
    it->update_cpClickId("config/screen");
    appendRow(it);

    it = new ControlPanelModelItem();
    it->update_cpTitle(tr("Localization"));
    it->update_cpSubTitle(tr("Language settings"));
    it->update_cpIcon("qrc:/img/icon_config_l18n.png");
    it->update_cpClickId("config/l18n");
    appendRow(it);

    it = new ControlPanelModelItem();
    it->update_cpTitle(tr("Information"));
    it->update_cpSubTitle(tr("Personal settings"));
    it->update_cpIcon("qrc:/img/icon_config_info.png");
    it->update_cpClickId("config/info");
    appendRow(it);

    it = new ControlPanelModelItem();
    it->update_cpTitle(tr("Network"));
    it->update_cpSubTitle(tr("IP, gateway, DNS"));
    it->update_cpIcon("qrc:/img/icon_config_network.png");
    it->update_cpClickId("config/network");
    appendRow(it);

    it = new ControlPanelModelItem();
    it->update_cpTitle(tr("Update"));
    it->update_cpSubTitle(tr("Software updates"));
    it->update_cpIcon("qrc:/img/icon_config_fw.png");
    it->update_cpClickId("config/update");
    appendRow(it);

    if (calaosApp->get_hasInstall())
    {
        it = new ControlPanelModelItem();
        it->update_cpTitle(tr("Installation"));
        it->update_cpSubTitle(tr("Install on disk"));
        it->update_cpIcon("qrc:/img/icon_config_install.png");
        it->update_cpClickId("config/install");
        appendRow(it);
    }

    if (calaosApp->get_isSnapshotBoot())
    {
        it = new ControlPanelModelItem();
        it->update_cpTitle(tr("Rollback"));
        it->update_cpSubTitle(tr("Rollback to this snapshot"));
        it->update_cpIcon("qrc:/img/icon_config_rollback.png");
        it->update_cpClickId("config/rollback");
        appendRow(it);
    }
}
