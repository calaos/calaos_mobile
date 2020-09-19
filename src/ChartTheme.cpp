#include "ChartTheme.h"

void ChartTheme::update_chart(QQuickItem *item){
    if(QGraphicsScene *scene = item->findChild<QGraphicsScene *>()){
        for(QGraphicsItem *it : scene->items()){
            if(QtCharts::QChart *chart = dynamic_cast<QtCharts::QChart *>(it)){
                // Customize chart background
                QLinearGradient backgroundGradient;
                backgroundGradient.setStart(QPointF(0, 0));
                backgroundGradient.setFinalStop(QPointF(0, 1));
                backgroundGradient.setColorAt(0.0, QRgb(0x123e47));
                backgroundGradient.setColorAt(1.0, QRgb(0x040505));
                backgroundGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
                chart->setBackgroundBrush(backgroundGradient);
                // Customize plot area background
//                QLinearGradient plotAreaGradient;
//                plotAreaGradient.setStart(QPointF(0, 1));
//                plotAreaGradient.setFinalStop(QPointF(1, 0));
//                plotAreaGradient.setColorAt(0.0, qRgba(0, 0, 0, 10));
//                plotAreaGradient.setColorAt(1.0, qRgba(0, 0, 0, 30));
//                plotAreaGradient.setCoordinateMode(QGradient::ObjectBoundingMode);
//                chart->setPlotAreaBackgroundBrush(plotAreaGradient);
                chart->setPlotAreaBackgroundVisible(false);
                // Remove margins
                chart->setMargins(QMargins(2, 2, 2, 2));
            }
        }
    }
    printf("Update chart\n");

}

void ChartTheme::update_axes(QtCharts::QAbstractAxis *axisX, QtCharts::QAbstractAxis *axisY){
    if(axisX && axisY){
        // Customize axis colors
        QPen axisPen(QRgb(0x005f71));
        axisPen.setWidth(1);
        axisX->setLinePen(axisPen);
        axisY->setLinePen(axisPen);

        QFont font;
        font.setPixelSize(10);
        axisX->setLabelsFont(font);
        axisY->setLabelsFont(font);
        // Customize grid lines and shades
        axisY->setShadesPen(Qt::NoPen);
        axisY->setShadesBrush(QBrush(QRgb(0x005f71)));
    }
}

