import QtQuick
import QtQuick.Layouts
import SharedComponents
import "../quickflux"

//The list of the box, about 600 zones: filtered with the virtual keyboard.
ColumnLayout {
    id: picker

    property string selected: boxTime.timezone

    readonly property bool canApply: boxTime.loaded && !boxTime.busy &&
                                     selected !== "" && selected !== boxTime.timezone &&
                                     boxTime.zones.indexOf(selected) >= 0

    signal chosen(string zone)
    signal cancelled()

    function askFilter() {
        AppActions.openKeyboard(qsTr("Time zone"), qsTr("Filter the time zones"), boxTime.zoneFilter,
                                TextInput.Normal, false,
                                function(text) { boxTime.zoneFilter = text })
    }

    Component.onCompleted: {
        boxTime.zoneFilter = ""
        Qt.callLater(function() {
            zoneList.positionViewAtIndex(boxTime.zones.indexOf(picker.selected), ListView.Center)
        })
    }

    spacing: Units.dp(8)

    Text {
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
        color: Theme.colorAlpha(Theme.whiteColor, 0.7)
        text: qsTr("Time zone")
        Layout.fillWidth: true
    }

    RowLayout {
        spacing: Units.dp(8)
        Layout.fillWidth: true

        CalaosItemBase {
            objectName: "zoneFilterButton"
            text: boxTime.zoneFilter !== ""? boxTime.zoneFilter : qsTr("Filter the time zones")
            Layout.fillWidth: true
            Layout.preferredHeight: Units.dp(40)
            onButtonClicked: picker.askFilter()
        }

        CalaosButton {
            objectName: "zoneFilterClear"
            text: qsTr("Clear")
            visible: boxTime.zoneFilter !== ""
            Layout.preferredHeight: Units.dp(40)
            Layout.minimumWidth: width
            onButtonClicked: boxTime.zoneFilter = ""
        }
    }

    Item {
        clip: true
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: zoneList
            objectName: "zoneList"
            anchors.fill: parent
            model: boxTime.zones

            delegate: MouseArea {
                required property string modelData

                objectName: "zone_" + modelData
                width: ListView.view.width
                height: Units.dp(44)
                onClicked: picker.selected = modelData

                Rectangle {
                    anchors.fill: parent
                    radius: Units.dp(4)
                    color: Theme.colorAlpha(Theme.blueColor, 0.15)
                    visible: modelData === picker.selected
                }

                Text {
                    anchors {
                        left: parent.left; leftMargin: Units.dp(12)
                        right: parent.right; rightMargin: Units.dp(12)
                        verticalCenter: parent.verticalCenter
                    }
                    elide: Text.ElideMiddle
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(16) }
                    color: modelData === picker.selected? Theme.blueColor : Theme.colorAlpha(Theme.whiteColor, 0.8)
                    text: modelData
                }
            }
        }

        ScrollBar { listObject: zoneList }

        Text {
            objectName: "zoneNone"
            anchors.centerIn: parent
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            visible: boxTime.zones.length === 0 && !boxTime.zonesLoading
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
            color: boxTime.zonesError !== ""? Theme.redColor : Theme.colorAlpha(Theme.whiteColor, 0.7)
            text: boxTime.zonesError !== ""? boxTime.zonesError : qsTr("No time zone matches")
        }
    }

    RowLayout {
        spacing: Units.dp(8)
        Layout.fillWidth: true

        CalaosButton {
            objectName: "zonesRetry"
            text: qsTr("Try again")
            visible: boxTime.zonesError !== ""
            Layout.minimumWidth: width
            onButtonClicked: boxTime.loadZones()
        }

        Item { height: 1; Layout.fillWidth: true }

        CalaosButton {
            objectName: "zoneCancel"
            text: qsTr("Cancel")
            Layout.minimumWidth: width
            onButtonClicked: picker.cancelled()
        }

        CalaosButton {
            objectName: "zoneApply"
            text: qsTr("Apply")
            disabled: !picker.canApply
            Layout.minimumWidth: width
            onButtonClicked: picker.chosen(picker.selected)
        }
    }
}
