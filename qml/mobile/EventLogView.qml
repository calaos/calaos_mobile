import QtQuick
import Calaos
import SharedComponents
import QtQuick.Layouts
import "../quickflux"

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: listViewLog
        model: eventLogModel

        y: header.height
        x: Units.dp(2)
        width: parent.width - x
        height: parent.height - header.height

        spacing: Units.dp(5)

        delegate: default_delegate

        section.property: "evDate"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading
        section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

        onAtYEndChanged: {
            if (listViewLog.atYEnd)
                eventLogModel.loadMore()
        }

        onDragEnded: if (pullHeader.refresh) { eventLogModel.refresh() }

        PullRefreshHeader {
            id: pullHeader
            listview: listViewLog
            y: -listViewLog.contentY - height
        }
    }

    Text {
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors.centerIn: listViewLog
        text: qsTr("No events in log.<br>Calaos Server <b>v3.1</b> is required.")
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        visible: listViewLog.count == 0
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
    }

    ScrollBar { listObject: listViewLog }

    ViewHeader {
        id: header
        headerLabel: qsTr("Event log")
        iconSource: calaosApp.getPictureSized("history")

        ItemButtonAction {
            iconSource: "icon_filter"

            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: Units.dp(10)
            }

            //onButtonClicked: AppActions.openEventPushViewer(evNotifText, evPictureUrl)

//            visible: listViewLog.count > 0
            visible: false
        }
    }

    Component {
        id: sectionHeading
        Rectangle {
            z: 99
            width: listViewLog.width
            color: "black"
            height: Units.dp(45)

            Image {
                id: ic
                source: calaosApp.getPictureSized("icon_date")
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left; leftMargin: Units.dp(10)
                }
            }

            Text {
                id: txt
                color: "#e7e7e7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: section
                anchors {
                    left: ic.source === ""?parent.left:ic.right
                    leftMargin: Units.dp(5)
                    verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                color: "#333333"
                height: Units.dp(2)
            }
        }
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: calaosApp.getPictureSized("back_items_home")
            border.left: Units.dp(5); border.top: Units.dp(5)
            border.right: Units.dp(5); border.bottom: Units.dp(5)

            width: parent.width - Units.dp(10)
            height: col.implicitHeight

            ColumnLayout {
                id: col
                anchors {
                    left: parent.left; leftMargin: Units.dp(8)
                    right: parent.right; rightMargin: Units.dp(8)
                    top: parent.top; topMargin: Units.dp(8)
                }

                spacing: Units.dp(8)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Units.dp(8)

                    IconItem {
                        id: icon
                        source: calaosApp.getPictureSized(evIconSource)
                        visible: evIconSource != ""
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        color: "#3ab4d7"
                        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                        text: evTitle
                        elide: Text.ElideRight
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true; height: 1 }

                    Text {
                        color: "#ffda5a"
                        font { family: calaosFont.fontFamily; bold: false; pointSize: 8 }
                        text: evTime
                        elide: Text.ElideRight
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Units.dp(8)

                    visible: evType == Common.EventIoChanged

                    Text {
                        color: "#e7e7e7"
                        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; bold: false; pointSize: 10 }
                        text: evRoomName
                        elide: Text.ElideRight
                        //anchors.verticalCenter: btview.verticalCenter
                    }

                    Item { Layout.fillWidth: true; height: 1 }

                    Text {
                        color: "#ffda5a"
                        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; bold: false; pointSize: 10 }
                        text: evActionText
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Units.dp(8)

                    visible: evType == Common.EventPush

                    Text {
                        color: "#e7e7e7"
                        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; bold: false; pointSize: 10 }
                        text: evNotifText
                        wrapMode: Text.Wrap
                        anchors.verticalCenter: evHasPicture?btview.verticalCenter:undefined
                    }

                    Item { Layout.fillWidth: true; height: 1 }

                    ItemButtonAction {
                        id: btview
                        iconSource: "icon_camera_title"
                        visible: evHasPicture
                        onButtonClicked: AppActions.openEventPushViewer(evNotifText, evPictureUrl)
                    }
                }

                Item { Layout.fillWidth: true; height: Units.dp(10) }
            }

        }

    }
}
