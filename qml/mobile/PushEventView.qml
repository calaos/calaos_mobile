import QtQuick 2.2
import Calaos 1.0
import QtQuick.Layouts 1.3
import SharedComponents 1.0
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Item {
    id: rootitem

    property string event_uuid
    property string event_message
    property string event_picurl
    property bool event_haspic: true

    property bool loading: false
    property QtObject eventModel

    onEvent_uuidChanged: {
        if (event_uuid == "") {
            loading = false;
            return;
        }

        console.debug("Loading event: " + event_uuid)

        loading = true

        event_haspic = false
        eventModel = eventLogModel.loadEvent(event_uuid)
        event_message = Qt.binding(function() { return eventModel.evNotifText })
        event_picurl = Qt.binding(function() { return eventModel.evPictureUrl })
        event_haspic = Qt.binding(function() { return eventModel.evHasPicture })
        loading = Qt.binding(function() { return eventModel.loading })
    }

    Rectangle {
        color: "black"
        anchors.fill: parent
    }

    BusyIndicator {
        opacity: loading || pic.status === Image.Loading? 1:0
        running: visible
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation { } }
        anchors {
            top: flick.top; topMargin: Units.dp(15)
            horizontalCenter: parent.horizontalCenter
        }

        style: BusyIndicatorStyle {
            indicator: Image {
                opacity: 0.70
                visible: control.running
                source: "qrc:/img/spinner.svg"
                RotationAnimator on rotation {
                    running: control.running
                    loops: Animation.Infinite
                    duration: 2000
                    from: 0 ; to: 360
                }
            }
        }
    }

    Flickable {
        id: flick
        width: parent.width
        height: parent.height - header.height
        y: header.height

        contentWidth: parent.width
        contentHeight: col.implicitHeight

        ColumnLayout {
            id: col

            width: rootitem.width

            Image {
                id: pic
                Layout.maximumWidth: rootitem.width
                source: event_picurl

                property double ratio: sourceSize.width / sourceSize.height
                width: parent.width
                Layout.preferredHeight: width / ratio
            }

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: event_message
                wrapMode: Text.Wrap
                Layout.maximumWidth: rootitem.width
            }

        }
    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Push Notification")
        iconSource: calaosApp.getPictureSized("icon_notif")
    }

    Rectangle {
        color: "#e7e7e7"
        opacity: 0.7
        anchors {
            top: header.bottom
            left: parent.left
        }
        height: Units.dp(1)
        width: pic.status === Image.Loading? pic.progress * parent.width:0
    }
}
