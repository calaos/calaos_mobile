#include "CalaosWidgetModel.h"
#include "HardwareUtils.h"
#include "HardwareUtils_desktop.h"

QStringList CalaosWidgetModel::widgetSources = QStringList();

CalaosWidgetModel::CalaosWidgetModel():
    QStandardItemModel()
{
    loadWidgetSources();
}

QObject *CalaosWidgetModel::getWidget(int idx) const
{
    CalaosWidget *w = dynamic_cast<CalaosWidget *>(item(idx));
    if (w) QQmlEngine::setObjectOwnership(w, QQmlEngine::CppOwnership);
    return w;
}

void CalaosWidgetModel::loadFromDisk()
{
    HardwareUtilsDesktop *hw = dynamic_cast<HardwareUtilsDesktop *>(HardwareUtils::Instance());
    if (!hw) return;

    QString conf = hw->getConfigFile("calaos_widgets.json");

    QFile fconf(conf);
    if (fconf.open(QFile::ReadOnly))
    {
        QJsonParseError err;
        QJsonDocument jdoc = QJsonDocument::fromJson(fconf.readAll(), &err);

        if (err.error == QJsonParseError::NoError && jdoc.isArray())
        {
            clear();

            QJsonArray jarr = jdoc.array();
            for (int i = 0;i < jarr.count();i++)
            {
                CalaosWidget *w = createWidget(jarr.at(i).toVariant().toMap());
                if (w)
                    appendRow(w);
            }
        }
        else
            qInfo() << "Widgets config found but contains error: " << err.errorString();
    }
    else
        qInfo() << "No widgets config found.";
}

void CalaosWidgetModel::saveToDisk()
{
    HardwareUtilsDesktop *hw = dynamic_cast<HardwareUtilsDesktop *>(HardwareUtils::Instance());
    if (!hw) return;

    QString conf = hw->getConfigFile("calaos_widgets.json");
    QFile fconf(conf);
    if (fconf.open(QFile::WriteOnly | QFile::Truncate))
    {
        QJsonArray jarr;
        for (int i = 0;i < rowCount();i++)
        {
            CalaosWidget *w = dynamic_cast<CalaosWidget *>(item(i));
            if (w)
                jarr.append(QJsonObject::fromVariantMap(w->toVariantMap()));
        }

        QJsonDocument jdoc(jarr);
        fconf.write(jdoc.toJson());
    }
    else
        qWarning() << "Failed to write widgets to " << conf;
}

CalaosWidget *CalaosWidgetModel::createWidget(const QVariantMap &obj)
{
    CalaosWidget *w = nullptr;

    if (!widgetSources.contains(obj["module"].toString()))
    {
        qWarning() << "Unknown widget type: " << obj["module"].toString();
        return w;
    }

    w = CalaosWidget::fromVariantMap(obj);

    return w;
}

void CalaosWidgetModel::loadWidgetSources()
{
    widgetSources.clear();

    QDirIterator it(":/widgets", QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        // :/widgets/Clock/Clock.qml

        QString fname = it.next();
        QString moduleName = fname.section('/', 2, 2);

        if (widgetSources.contains(moduleName))
            continue;

        qInfo() << "Found widget module: " << moduleName;
        widgetSources.append(moduleName);
    }
}
