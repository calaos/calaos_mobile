import QtQuick 2.5
import SharedComponents 1.0

Item {
    id: widget

    property variant widgetModel

    x: widgetModel.posX
    y: widgetModel.posY

    width: Math.min(Math.max(widgetModel.width, maximumSize.width), minimumSize.width)
    height: Math.min(Math.max(widgetModel.height, maximumSize.height), minimumSize.height)

    property size minimumSize: Qt.size(20, 20)
    property size maximumSize: Qt.size(500, 500)

    property bool editMode: false

    BorderImage {
        id: editBg
        source: "qrc:/img/widget_back_edit.png"

        border { left: Units.dp(62); right: Units.dp(70); top: Units.dp(65); bottom: Units.dp(64); }
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat

        anchors.fill: parent

        opacity: editMode?1:0
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation {} }
    }

    Loader {
        id: wloader
        anchors.centerIn: parent
        source: widgetModel.moduleSource

        opacity: editMode?0.2:1.0
        Behavior on opacity { PropertyAnimation {} }

        onLoaded: {
            item.widgetModel = widget.widgetModel
            widget.minimumSize = item.minimumSize
            widget.maximumSize = item.maximumSize
        }
    }
}
