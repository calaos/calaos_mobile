import QtQuick
import QtQuick.Layouts
import SharedComponents
import Qt5Compat.GraphicalEffects

Dialog {
    id: dialogTabList

    property var tabModel: null
    property int activeTabIndex: 0
    property int maxTabs: 10

    signal tabSelected(int index)
    signal tabClosed(int index)
    signal newTabRequested()

    title: qsTr("Tabs")
    text: qsTr("Manage your open tabs")
    hasActions: true
    positiveButtonText: tabModel && tabModel.count < maxTabs ? qsTr("New Tab") : ""
    positiveButtonEnabled: tabModel ? tabModel.count < maxTabs : true
    negativeButtonText: qsTr("Close")

    onAccepted: {
        newTabRequested()
    }

    onRejected: {
        close()
    }

    Repeater {
        model: tabModel

        Item {
            width: parent.width
            height: Units.dp(56)

            Rectangle {
                anchors.fill: parent
                anchors.margins: Units.dp(2)
                color: "transparent"
                border.color: "#3AB4D7"
                border.width: index === dialogTabList.activeTabIndex ? Units.dp(1) : 0
                radius: Units.dp(4)
                opacity: index === dialogTabList.activeTabIndex ? 0.6 : 0.0

                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Units.dp(2)
                color: itemMouseArea.pressed ? "#3AB4D7" : "transparent"
                opacity: itemMouseArea.pressed ? 0.15 : 0.0
                radius: Units.dp(4)
            }

            Row {
                anchors {
                    left: parent.left
                    leftMargin: Units.dp(12)
                    right: closeBtn.left
                    rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
                spacing: Units.dp(6)

                // Tab number indicator
                Rectangle {
                    width: Units.dp(24)
                    height: Units.dp(24)
                    radius: Units.dp(12)
                    color: index === dialogTabList.activeTabIndex ? "#3AB4D7" : "transparent"
                    border.color: "#3AB4D7"
                    border.width: Units.dp(1)
                    opacity: index === dialogTabList.activeTabIndex ? 1.0 : 0.4
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: (index + 1).toString()
                        font.pixelSize: Units.dp(11)
                        font.family: calaosFont.fontFamily
                        color: index === dialogTabList.activeTabIndex ? "#171717" : "#e7e7e7"
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - Units.dp(34)
                    spacing: Units.dp(2)

                    Text {
                        width: parent.width
                        text: model.title || qsTr("New Tab")
                        font.pixelSize: Units.dp(14)
                        font.family: calaosFont.fontFamily
                        font.weight: Font.Light
                        color: "#e7e7e7"
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    Text {
                        width: parent.width
                        text: model.url || ""
                        font.pixelSize: Units.dp(11)
                        font.family: calaosFont.fontFamily
                        font.weight: Font.ExtraLight
                        color: "#e7e7e7"
                        opacity: 0.4
                        elide: Text.ElideMiddle
                        maximumLineCount: 1
                    }
                }
            }

            // Close button
            Item {
                id: closeBtn
                width: Units.dp(36)
                height: Units.dp(36)
                anchors {
                    right: parent.right
                    rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }

                // Don't show close on last tab
                visible: tabModel ? tabModel.count > 1 : false

                Image {
                    id: iconClear
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/img/button_action_del.png"
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    onClicked: dialogTabList.tabClosed(index)
                }
            }

            MouseArea {
                id: itemMouseArea
                anchors {
                    fill: parent
                    rightMargin: closeBtn.visible ? closeBtn.width + Units.dp(8) : 0
                }
                onClicked: {
                    dialogTabList.tabSelected(index)
                    dialogTabList.close()
                }
            }

            // Separator
            Rectangle {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    right: parent.right
                    leftMargin: Units.dp(12)
                    rightMargin: Units.dp(12)
                }
                height: Units.dp(1)
                color: "#3AB4D7"
                opacity: 0.1
                visible: index < (tabModel ? tabModel.count - 1 : 0)
            }
        }
    }
}
