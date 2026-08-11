import QtQuick
import SharedComponents
import QtQuick.Layouts
import Calaos

PageScaffold {
    id: page

    title: qsTr("Scenarios")

    footerButtons: FooterButton {
        label: qsTr("Quit")
        icon: "qrc:/img/button_action_quit.png"
        Layout.minimumWidth: width
        onBtClicked: rootWindow.goToDesktop()
    }

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: parent.horizontalCenter
            rightMargin: Units.dp(160)
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }
            clip: true

            ItemListView {
                id: listViewLeft
                model: scenarioModel

                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                showHeader: false
            }
            ScrollBar { listObject: listViewLeft }
        }
    }
}
