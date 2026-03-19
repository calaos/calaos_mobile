import QtQuick
import SharedComponents

Item {
    id: browserMenu

    signal goToUrl()
    signal openTabs()
    signal openBookmarks()
    signal bookmarkCurrentPage()

    property int tabCount: 0

    // Full screen overlay to catch outside clicks
    MouseArea {
        anchors.fill: parent
        onClicked: browserMenu.visible = false
    }

    // Menu panel
    Rectangle {
        id: menuPanel
        width: Units.dp(220)
        height: menuColumn.height + Units.dp(16)
        color: "#171717"
        border.color: "#3AB4D7"
        border.width: Units.dp(1)
        radius: Units.dp(4)
        opacity: 0.95

        anchors {
            bottom: parent.bottom
            bottomMargin: Units.dp(70)
            horizontalCenter: parent.horizontalCenter
        }

        Column {
            id: menuColumn
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                topMargin: Units.dp(8)
            }

            WebBrowserMenuItem {
                text: qsTr("Go to URL...")
                icon: "qrc:/img/button_action_plus.png"
                onClicked: {
                    browserMenu.visible = false
                    browserMenu.goToUrl()
                }
            }

            WebBrowserMenuItem {
                text: qsTr("Tabs") + " (" + browserMenu.tabCount + ")"
                icon: "qrc:/img/button_more.png"
                onClicked: {
                    browserMenu.visible = false
                    browserMenu.openTabs()
                }
            }

            WebBrowserMenuItem {
                text: qsTr("Bookmarks")
                icon: "qrc:/img/fav.png"
                onClicked: {
                    browserMenu.visible = false
                    browserMenu.openBookmarks()
                }
            }

            // Separator
            Rectangle {
                width: parent.width - Units.dp(16)
                height: Units.dp(1)
                color: "#3AB4D7"
                opacity: 0.2
                anchors.horizontalCenter: parent.horizontalCenter
            }

            WebBrowserMenuItem {
                text: qsTr("Bookmark this page")
                icon: "qrc:/img/fav.png"
                onClicked: {
                    browserMenu.visible = false
                    browserMenu.bookmarkCurrentPage()
                }
            }
        }
    }
}
