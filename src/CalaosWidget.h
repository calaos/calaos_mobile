#ifndef CALAOSWIDGET_H
#define CALAOSWIDGET_H

#include <QObject>
#include <QStandardItem>
#include "qqmlhelpers.h"

class CalaosWidget: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY(QString, uuid)
    QML_READONLY_PROPERTY(QString, module)
    QML_READONLY_PROPERTY(QString, moduleSource)
    QML_WRITABLE_PROPERTY(int, posX)
    QML_WRITABLE_PROPERTY(int, posY)
    QML_WRITABLE_PROPERTY(int, width)
    QML_WRITABLE_PROPERTY(int, height)
public:
    CalaosWidget();

    QVariantMap toVariantMap() const;
    static CalaosWidget *fromVariantMap(const QVariantMap &obj);

private:
    QVariantMap moduleConfig;
};

#endif // CALAOSWIDGET_H
