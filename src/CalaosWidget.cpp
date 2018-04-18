#include "CalaosWidget.h"
#include <QUuid>
#include <QtCore>
#include "CalaosWidgetModel.h"

CalaosWidget::CalaosWidget():
    QStandardItem()
{
}

QVariantMap CalaosWidget::toVariantMap() const
{
    QVariantMap m;

    m["module"] = get_module();
    m["uuid"] = get_uuid();
    m["x"] = get_posX();
    m["y"] = get_posY();
    m["width"] = get_width();
    m["height"] = get_height();

    auto doc = QJsonDocument::fromJson(moduleData.toLocal8Bit());
    m["data"] = doc.object();

    return m;
}

CalaosWidget *CalaosWidget::fromVariantMap(const QVariantMap &obj)
{
    CalaosWidget *w = new CalaosWidget();

    w->update_module(obj["module"].toString());
    w->update_moduleSource(QStringLiteral("qrc:/widgets/%1/%1.qml").arg(w->get_module()));

    if (obj["uuid"].toString() == "")
        w->update_uuid(QUuid::createUuid().toString());
    else
        w->update_uuid(obj["uuid"].toString());

    w->set_posX(obj["x"].toInt());
    w->set_posY(obj["y"].toInt());
    w->set_width(obj["width"].toInt());
    w->set_height(obj["height"].toInt());

    QJsonDocument doc(QJsonObject::fromVariantMap(obj["data"].toMap()));
    w->moduleData = doc.toJson();

    return w;
}

QString CalaosWidget::getData()
{
    return moduleData;
}

void CalaosWidget::saveData(QString data)
{
    moduleData = data;
    CalaosWidgetModel::Instance()->saveToDisk();
}
