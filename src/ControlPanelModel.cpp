#include "ControlPanelModel.h"

ControlPanelModel::ControlPanelModel(QObject *parent):
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleTitle] = "cpTitle";
    roles[RoleSubTitle] = "cpSubTitle";
    roles[RoleIcon] = "cpIcon";
    roles[RoleClickId] = "cpClickId";
    setItemRoleNames(roles);

    load();
}

ControlPanelModel::~ControlPanelModel()
{
}

void ControlPanelModel::setOptions(bool hasInstall, bool isSnapshotBoot)
{
    if (hasInstall == m_hasInstall && isSnapshotBoot == m_isSnapshotBoot)
        return;

    m_hasInstall = hasInstall;
    m_isSnapshotBoot = isSnapshotBoot;
    clear();
    load();
}

// QStandardItem texts are plain strings: engine.retranslate() cannot reach them
void ControlPanelModel::retranslate()
{
    const QList<Entry> list = entries();
    if (list.size() != rowCount())
    {
        clear();
        load();
        return;
    }

    for (int i = 0; i < list.size(); i++)
    {
        auto *it = static_cast<ControlPanelModelItem *>(item(i));
        it->update_cpTitle(list[i].title);
        it->update_cpSubTitle(list[i].subTitle);
    }
}

QList<ControlPanelModel::Entry> ControlPanelModel::entries() const
{
    QList<Entry> list;

    list << Entry{ tr("Screen saver"), tr("Screen power management"),
                   "qrc:/img/icon_item_screensaver.png", "config/screen" };
    list << Entry{ tr("Localization"), tr("Language settings"),
                   "qrc:/img/icon_config_l18n.png", "config/l18n" };
    list << Entry{ tr("Date and time"), tr("Clock, time zone, automatic time"),
                   "qrc:/img/icon_config_clock.png", "config/time" };
    list << Entry{ tr("Information"), tr("Personal settings"),
                   "qrc:/img/icon_config_info.png", "config/info" };
    list << Entry{ tr("Network"), tr("IP, gateway, DNS"),
                   "qrc:/img/icon_config_network.png", "config/network" };
    list << Entry{ tr("Update"), tr("Software updates"),
                   "qrc:/img/icon_config_fw.png", "config/update" };

    if (m_hasInstall)
        list << Entry{ tr("Installation"), tr("Install on disk"),
                       "qrc:/img/icon_config_install.png", "config/install" };

    if (m_isSnapshotBoot)
        list << Entry{ tr("Rollback"), tr("Rollback to this snapshot"),
                       "qrc:/img/icon_config_rollback.png", "config/rollback" };

    return list;
}

void ControlPanelModel::load()
{
    for (const Entry &e: entries())
    {
        auto *it = new ControlPanelModelItem();
        it->update_cpTitle(e.title);
        it->update_cpSubTitle(e.subTitle);
        it->update_cpIcon(e.icon);
        it->update_cpClickId(e.clickId);
        appendRow(it);
    }
}
