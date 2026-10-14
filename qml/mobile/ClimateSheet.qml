import QtQuick
import QtQuick.Layouts
import SharedComponents
import "../SharedComponents/ClimateLogic.js" as Logic

//Settings of a climate device on the phone: a bottom sheet with the dial, the
//setpoint line, the mode pill and "More settings". The mode choice and the
//more settings are second sheets on top of it.
Item {
    id: host

    property string fontFamily: calaosFont.fontFamily
    property alias clock: climate.clock
    readonly property alias climate: climate
    readonly property bool opened: detail.opened
    readonly property bool modeOpened: modeSheet.opened
    readonly property bool moreOpened: moreSheet.opened

    width: 0
    height: 0

    function openFor(io) {
        climate.modelData = io
        detail.open()
    }

    function close() {
        modeSheet.close()
        moreSheet.close()
        detail.close()
    }

    ClimateModel { id: climate }

    BottomSheet {
        id: detail
        objectName: "detailSheet"
        heightRatio: 0.7
        fontFamily: host.fontFamily
        onAboutToHide: climate.flush()

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(12)

                Text {
                    objectName: "sheetName"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    color: Theme.whiteColor
                    font { family: host.fontFamily; pixelSize: Units.dp(22) }
                    text: climate.name
                }

                ClimateModePill {
                    objectName: "modePill"
                    fontFamily: host.fontFamily
                    word: climate.mode !== "" ? climate.modeLabel(climate.mode) : qsTr("Mode unknown")
                    icon: climate.modeIcon(climate.mode)
                    disabled: climate.status.kind === "offline" || climate.caps.modes.length === 0
                    onButtonClicked: modeSheet.open()
                }
            }

            Item { Layout.fillHeight: true }

            ClimateDetail {
                objectName: "detail"
                Layout.alignment: Qt.AlignHCenter
                climate: climate
                fontFamily: host.fontFamily
                dialSize: Math.min(Units.dp(300), detail.width - Units.dp(40))
            }

            Item { Layout.fillHeight: true }

            MouseArea {
                objectName: "moreLink"
                Layout.fillWidth: true
                Layout.preferredHeight: Units.dp(50)
                visible: Logic.hasMore(climate.caps, climate.devState)
                onClicked: moreSheet.open()

                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.1)
                }

                Text {
                    anchors { left: parent.left; leftMargin: Units.dp(12); verticalCenter: parent.verticalCenter }
                    color: Theme.mutedColor
                    font { family: host.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
                    text: qsTr("More settings")
                }

                ClimateIcon {
                    anchors { right: parent.right; rightMargin: Units.dp(4); verticalCenter: parent.verticalCenter }
                    name: "chevronR"
                    size: Units.dp(20)
                    color: Theme.mutedColor
                }
            }
        }
    }

    BottomSheet {
        id: modeSheet
        objectName: "modeSheet"
        title: qsTr("Mode")
        fontFamily: host.fontFamily
        fitHeight: modeRows.implicitHeight

        ClimateRows {
            id: modeRows
            objectName: "modeRows"
            width: parent.width
            fontFamily: host.fontFamily
            choices: climate.caps.modes.map(function(m) {
                return { value: m, label: climate.modeLabel(m), icon: climate.modeIcon(m) }
            })
            current: climate.mode
            onChosen: function(value) {
                climate.chooseMode(value)
                modeSheet.close()
            }
        }
    }

    BottomSheet {
        id: moreSheet
        objectName: "moreSheet"
        title: qsTr("More settings")
        fontFamily: host.fontFamily
        fitHeight: more.implicitHeight

        ClimateMore {
            id: more
            objectName: "more"
            width: parent.width
            climate: climate
            fontFamily: host.fontFamily
        }
    }
}
