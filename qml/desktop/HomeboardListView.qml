import QtQuick
import SharedComponents
import "../quickflux"

ListView {

    delegate: BorderImage {

        source: "qrc:/img/menu_item.png"
        border { left: Units.dp(158); right: Units.dp(28); top: Units.dp(26); bottom: Units.dp(9) }

        BorderImage {
            id: selectImg
            source: "qrc:/img/menu_item_select.png"
            border { left: Units.dp(158); right: Units.dp(28); top: Units.dp(26); bottom: Units.dp(9) }

            opacity: 0

            state: "released"

            states: [
                State { name: "released"; PropertyChanges { target: selectImg; opacity: 0.0 } },
                State { name: "pressed"; PropertyChanges { target: selectImg; opacity: 1.0 } }
            ]
            transitions: [
                Transition {
                    from: "released"
                    to: "pressed"
                    PropertyAnimation { duration: 50; properties: "opacity"; easing.type: Easing.OutCubic }
                },
                Transition {
                    from: "pressed"
                    to: "released"
                    PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.InCubic }
                }
            ]
        }

        Loader {
            id: itIcon

            source: iconSource

            anchors {
                left: parent.left; leftMargin: Units.dp(26)
                bottom: parent.bottom; bottomMargin: Units.dp(9)
            }
        }

        Text {
            id: itTitle

            anchors {
                left: parent.left; leftMargin: Units.dp(165)
                top: parent.top; topMargin: Units.dp(30)
                right: parent.right; rightMargin: Units.dp(26)
            }

            text: title

            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(15)
            font.family: calaosFont.fontFamily
            font.weight: Font.Thin
            color: "#3AB4D7"
        }

        Text {
            id: itDesc

            anchors {
                left: parent.left; leftMargin: Units.dp(165)
                top: itTitle.top; topMargin: Units.dp(25)
                right: parent.right; rightMargin: Units.dp(26)
                bottom: parent.bottom; bottomMargin: Units.dp(26)
            }

            text: description

            wrapMode: Text.WordWrap
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(10)
            font.family: calaosFont.fontFamily
            font.weight: Font.Thin
            color: "#E7E7E7"
        }

        Text {
            id: itMore

            anchors {
                right: parent.right; rightMargin: Units.dp(26)
                verticalCenter: parent.verticalCenter
            }

            text: moreText

            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(10)
            font.family: calaosFont.fontFamily
            horizontalAlignment: Text.AlignRight
            font.weight: Font.Thin
            color: "#FFDA5A"
        }

        MouseArea {
            anchors.fill: parent
            onPressed: selectImg.state = "pressed"
            onReleased: selectImg.state = "released"
            onClicked: AppActions.clickHomeboardItem(action)
        }
    }
}
