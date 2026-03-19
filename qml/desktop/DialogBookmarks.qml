import QtQuick
import QtQuick.Layouts
import SharedComponents
import Qt5Compat.GraphicalEffects

Dialog {
    id: dialogBookmarks

    signal bookmarkSelected(string url)
    signal addBookmarkRequested()

    title: qsTr("Bookmarks")
    text: qsTr("Manage your bookmarks")
    hasActions: true
    positiveButtonText: qsTr("Add Bookmark")
    negativeButtonText: qsTr("Close")

    onAccepted: {
        addBookmarkRequested()
    }

    onRejected: {
        close()
    }

    Text {
        visible: bookmarkModel.bookmarks.length === 0
        text: qsTr("No bookmarks yet.")
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        font.weight: Font.ExtraLight
        color: "#e7e7e7"
        opacity: 0.5
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
    }

    Repeater {
        model: bookmarkModel.bookmarks

        Item {
            width: parent.width
            height: Units.dp(52)

            Rectangle {
                anchors.fill: parent
                anchors.margins: Units.dp(2)
                color: bookmarkMouseArea.pressed ? "#3AB4D7" : "transparent"
                opacity: bookmarkMouseArea.pressed ? 0.15 : 0.0
                radius: Units.dp(4)
            }

            Column {
                anchors {
                    left: parent.left
                    leftMargin: Units.dp(12)
                    right: deleteBtn.left
                    rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
                spacing: Units.dp(2)

                Text {
                    width: parent.width
                    text: modelData.title || modelData.url
                    font.pixelSize: Units.dp(14)
                    font.family: calaosFont.fontFamily
                    font.weight: Font.Light
                    color: "#e7e7e7"
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    width: parent.width
                    text: modelData.url
                    font.pixelSize: Units.dp(11)
                    font.family: calaosFont.fontFamily
                    font.weight: Font.ExtraLight
                    color: "#e7e7e7"
                    opacity: 0.4
                    elide: Text.ElideMiddle
                    maximumLineCount: 1
                    visible: modelData.title !== ""
                }
            }

            // Delete button
            Item {
                id: deleteBtn
                width: Units.dp(36)
                height: Units.dp(36)
                anchors {
                    right: parent.right
                    rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }

                Image {
                    id: iconClear
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/img/button_action_del.png"
                    anchors.centerIn: parent
                }

                MouseArea {
                    id: deleteBtnMouse
                    anchors.fill: parent
                    onClicked: {
                        bookmarkModel.removeBookmark(index)
                    }
                }
            }

            MouseArea {
                id: bookmarkMouseArea
                anchors {
                    fill: parent
                    rightMargin: deleteBtn.width + Units.dp(8)
                }
                onClicked: {
                    dialogBookmarks.bookmarkSelected(modelData.url)
                    dialogBookmarks.close()
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
                visible: index < bookmarkModel.bookmarks.length - 1
            }
        }
    }
}
