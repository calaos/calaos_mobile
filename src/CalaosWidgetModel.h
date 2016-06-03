#ifndef CALAOSWIDGETMODEL_H
#define CALAOSWIDGETMODEL_H

#include <QStandardItemModel>
#include <QQmlApplicationEngine>
#include "CalaosWidget.h"
#include "qqmlhelpers.h"

class CalaosWidgetModel : public QStandardItemModel
{
    Q_OBJECT
    Q_PROPERTY(QStringList availableWidgets READ getAvailableWidgets NOTIFY availableWidgetsChanged)
public:
    static CalaosWidgetModel *Instance()
    {
        static CalaosWidgetModel inst;
        return &inst;
    }

    Q_INVOKABLE QObject *getWidget(int idx) const;
    Q_INVOKABLE void addWidget(QString widgetSource);

    Q_INVOKABLE void loadFromDisk();
    Q_INVOKABLE void saveToDisk();

    Q_INVOKABLE void scheduleSave();

    static CalaosWidget *createWidget(const QVariantMap &obj);

    QStringList getAvailableWidgets() { return widgetSources; }

signals:
    void availableWidgetsChanged();

private:
    CalaosWidgetModel();

    void loadWidgetSources();

    static QStringList widgetSources;

    QTimer *timerSave = nullptr;
};

#endif // CALAOSWIDGETMODEL_H
