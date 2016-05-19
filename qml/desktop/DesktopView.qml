import QtQuick 2.5
import SharedComponents 1.0

Item {

    Repeater {
        model: widgetsModel
        Widget {
            widgetModel: widgetsModel.getWidget(index)
        }
    }

}
