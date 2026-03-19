import QtQuick
import SharedComponents

Item {
    id: speedDial

    signal bookmarkClicked(string url)

    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#171717"
    }

    Text {
        id: titleText
        text: qsTr("New Tab")
        anchors {
            top: parent.top
            topMargin: Units.dp(40)
            horizontalCenter: parent.horizontalCenter
        }
        font.pixelSize: Units.dp(22)
        font.family: calaosFont.fontFamily
        font.weight: Font.Light
        color: "#3AB4D7"
    }

    Text {
        id: emptyText
        visible: bookmarkModel.bookmarks.length === 0
        text: qsTr("No bookmarks yet.\nAdd bookmarks from the browser menu.")
        anchors.centerIn: parent
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Units.dp(16)
        font.family: calaosFont.fontFamily
        font.weight: Font.ExtraLight
        color: "#e7e7e7"
        opacity: 0.5
    }

    GridView {
        id: gridView
        visible: bookmarkModel.bookmarks.length > 0
        anchors {
            top: titleText.bottom
            topMargin: Units.dp(30)
            bottom: parent.bottom
            bottomMargin: Units.dp(20)
            horizontalCenter: parent.horizontalCenter
        }
        width: Math.min(parent.width - Units.dp(40), cellWidth * Math.min(columns, count))

        property int columns: Math.floor((parent.width - Units.dp(40)) / cellWidth)

        cellWidth: Units.dp(180)
        cellHeight: Units.dp(110)
        clip: true

        model: bookmarkModel.bookmarks

        delegate: Item {
            width: gridView.cellWidth
            height: gridView.cellHeight

            Rectangle {
                id: tileRect
                anchors {
                    fill: parent
                    margins: Units.dp(6)
                }
                color: "transparent"
                border.color: "#3AB4D7"
                border.width: Units.dp(1)
                radius: Units.dp(4)
                opacity: tileMouseArea.pressed ? 1.0 : 0.25

                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }
            }

            Rectangle {
                anchors {
                    fill: parent
                    margins: Units.dp(6)
                }
                color: "#3AB4D7"
                radius: Units.dp(4)
                opacity: tileMouseArea.pressed ? 0.15 : 0.0

                Behavior on opacity {
                    NumberAnimation { duration: 150 }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: Units.dp(4)
                width: parent.width - Units.dp(24)

                Text {
                    width: parent.width
                    text: modelData.title || modelData.url
                    font.pixelSize: Units.dp(13)
                    font.family: calaosFont.fontFamily
                    font.weight: Font.Light
                    color: "#e7e7e7"
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    text: modelData.url
                    font.pixelSize: Units.dp(10)
                    font.family: calaosFont.fontFamily
                    font.weight: Font.ExtraLight
                    color: "#e7e7e7"
                    opacity: 0.4
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                    maximumLineCount: 1
                    visible: modelData.title !== ""
                }
            }

            MouseArea {
                id: tileMouseArea
                anchors.fill: parent
                onClicked: speedDial.bookmarkClicked(modelData.url)
            }
        }
    }
}
