import QtQuick
import Calaos
import QtQuick.Layouts
import SharedComponents
import QtQuick.Controls
import QtQuick.Controls.Material
//import QtQuick.Controls.Styles

Item {
    id: rootitem

    property string event_uuid
    property string event_message
    property string event_picuid
    property bool event_haspic: true

    property bool loading: false
    property QtObject eventModel
    //Downloaded while the view is open, dropped with it.
    property QtObject picture: null
    readonly property string pictureUrl: picture ? picture.url : ""
    readonly property bool pictureLoading: picture ? picture.loading : false
    readonly property bool pictureFailed: picture ? picture.failed : false

    //evPicUid and evHasPicture arrive in either order: open on both, once per uid.
    property string openedUid: ""

    function openPicture() {
        var uid = event_haspic ? event_picuid : ""
        if (uid === openedUid)
            return
        if (picture)
            picture.release()
        openedUid = uid
        picture = uid !== "" ? eventLogModel.loadPicture(uid) : null
    }

    onEvent_picuidChanged: openPicture()
    onEvent_haspicChanged: openPicture()
    Component.onDestruction: {
        if (picture)
            picture.release()
    }

    onEvent_uuidChanged: {
        if (event_uuid == "") {
            loading = false;
            return;
        }

        loading = true

        event_haspic = false
        eventModel = eventLogModel.loadEvent(event_uuid)
        event_message = Qt.binding(function() { return eventModel.evNotifText })
        event_picuid = Qt.binding(function() { return eventModel.evPicUid })
        event_haspic = Qt.binding(function() { return eventModel.evHasPicture })
        loading = Qt.binding(function() { return eventModel.loading })
    }

    Rectangle {
        color: "black"
        anchors.fill: parent
    }

    BusyIndicator {
        id: busy
        opacity: loading || pictureLoading || pic.status === Image.Loading? 1:0
        running: visible
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation { } }
        anchors {
            top: flick.top; topMargin: Units.dp(15)
            horizontalCenter: parent.horizontalCenter
        }

        contentItem: Image {
            opacity: 0.70
            visible: busy.running
            source: "qrc:/img/spinner.svg"
            RotationAnimator on rotation {
                running: busy.running
                loops: Animation.Infinite
                duration: 2000
                from: 0 ; to: 360
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
                source: pictureUrl

                property double ratio: sourceSize.height > 0 ? sourceSize.width / sourceSize.height : 1
                width: parent.width
                Layout.preferredHeight: ratio > 0 ? width / ratio : 0
            }

            Text {
                visible: pictureFailed
                color: "#e7e7e7"
                font { family: calaosFont.fontFamily; pointSize: 12 }
                text: qsTr("The picture could not be loaded.")
                wrapMode: Text.Wrap
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: rootitem.width
            }

            Button {
                id: retryButton
                visible: pictureFailed
                text: qsTr("Retry")
                Layout.alignment: Qt.AlignHCenter
                onClicked: picture.retry()
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
