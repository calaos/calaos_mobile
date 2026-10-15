import QtQuick
import QtQuick.Layouts
import "."

//Row content of a climate device in a list (UI_CLIMATE.md): the mini dial over
//the full height, the name and the room temperature above the mode pill, the
//status and the setpoint. Presentation only: IOClimate decides what to say.
Item {
    id: root

    property string fontFamily
    property string name
    property bool nameActive: false
    property string roomText
    property bool modeVisible: true
    property string modeText
    property string modeIcon: "help"
    property bool modeMuted: false
    property string statusText
    property color statusColor: Theme.mutedColor
    property string setpointText
    property bool setpointMuted: false

    property alias dial: dial
    readonly property bool pressed: tap.pressed
    readonly property bool hovered: tap.containsMouse

    signal clicked()

    implicitHeight: Units.dp(68)

    //Only the item itself takes the tap, not the controls that open below it
    MouseArea {
        id: tap
        objectName: "tapArea"
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }

    RowLayout {
        anchors {
            left: parent.left; leftMargin: Units.dp(10)
            right: parent.right; rightMargin: Units.dp(10)
            verticalCenter: parent.verticalCenter
        }
        spacing: Units.dp(8)

        ClimateDial {
            id: dial
            objectName: "dial"
            size: Units.dp(52)
            Layout.alignment: Qt.AlignVCenter
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Units.dp(5)

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(8)

                ScrollingText {
                    objectName: "itemName"
                    Layout.fillWidth: true
                    color: root.nameActive ? Theme.yellowColor : Theme.blueColor
                    font { family: root.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                    text: root.name
                }

                Text {
                    objectName: "roomTemp"
                    visible: text !== ""
                    color: Theme.whiteColor
                    font { family: root.fontFamily; weight: Font.Light; pointSize: 12 }
                    text: root.roomText
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(8)

                Rectangle {
                    id: pill
                    objectName: "modePill"
                    visible: root.modeVisible
                    implicitHeight: Units.dp(22)
                    implicitWidth: pillRow.implicitWidth + Units.dp(14)
                    radius: Units.dp(7)
                    color: root.modeMuted ? Qt.rgba(1, 1, 1, 0.04) : Theme.colorAlpha(Theme.blueColor, 0.12)
                    border.width: 1
                    border.color: root.modeMuted ? Theme.colorAlpha(Theme.mutedColor, 0.45)
                                                 : Theme.colorAlpha(Theme.blueColor, 0.5)

                    Row {
                        id: pillRow
                        anchors { left: parent.left; leftMargin: Units.dp(6); verticalCenter: parent.verticalCenter }
                        spacing: Units.dp(4)

                        ClimateIcon {
                            objectName: "modeIcon"
                            name: root.modeIcon
                            size: Units.dp(14)
                            color: root.modeMuted ? Theme.mutedColor : Theme.blueColor
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            objectName: "modeText"
                            color: root.modeMuted ? Theme.mutedColor : Theme.blueColor
                            font { family: root.fontFamily; pointSize: 10 }
                            text: root.modeText
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Text {
                    objectName: "statusText"
                    Layout.fillWidth: true
                    color: root.statusColor
                    font { family: root.fontFamily; pointSize: 10.5 }
                    elide: Text.ElideRight
                    text: root.statusText
                }

                Text {
                    objectName: "setpointText"
                    color: root.setpointMuted ? Theme.mutedColor : Theme.yellowColor
                    font { family: root.fontFamily; pointSize: 12 }
                    text: root.setpointText
                }
            }
        }

        ClimateIcon {
            objectName: "chevron"
            name: "chevronR"
            size: Units.dp(18)
            color: Theme.mutedColor
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
