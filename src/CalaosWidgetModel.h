#ifndef CALAOSWIDGETMODEL_H
#define CALAOSWIDGETMODEL_H

#include <QStandardItemModel>
#include <QQmlApplicationEngine>
#include "CalaosWidget.h"

class CalaosWidgetModel : public QStandardItemModel
{
    Q_OBJECT
public:
    static CalaosWidgetModel *Instance()
    {
        static CalaosWidgetModel inst;
        return &inst;
    }

    Q_INVOKABLE QObject *getWidget(int idx) const;

    Q_INVOKABLE void loadFromDisk();
    Q_INVOKABLE void saveToDisk();

    static CalaosWidget *createWidget(const QVariantMap &obj);

private:
    CalaosWidgetModel();

    void loadWidgetSources();

    static QStringList widgetSources;
};

#endif // CALAOSWIDGETMODEL_H
