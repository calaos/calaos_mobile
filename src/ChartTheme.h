#ifndef CHARTTHEME_H
#define CHARTTHEME_H

#include <QtQuick>
#include <QtWidgets>
#include <QtCharts>

class ChartTheme: public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void update_chart(QQuickItem *item);
    Q_INVOKABLE void update_axes(QtCharts::QAbstractAxis *axisX, QtCharts::QAbstractAxis *axisY);
};


#endif // CHARTTHEME_H
