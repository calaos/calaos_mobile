import QtQuick 2.4
import SharedComponents 1.0

Item {

    property bool disabled: false
    property bool active: false
    property alias labelTitle: buttonTxt.text
    property alias labelDesc: buttonDesc.text
    property alias icon: iconContent.children

    signal buttonClicked()

    width: itemImg.implicitWidth + Units.dp(34)
    height: itemImg.implicitHeight + Units.dp(34)

    Image {
        source: "qrc:/img/menu_media_item_shadow.png"
        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.bottom
        }
    }

    Image {
        id: itemImg
        source: "qrc:/img/menu_media_item.png"
        opacity: disabled?0.3:1
        anchors.centerIn: parent
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
    }

    Image {
        source: "qrc:/img/menu_media_item_reflect.png"
        anchors.centerIn: parent
    }

    Item {
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: itemImg.top
        }
        width: itemImg.implicitWidth
        height: Units.dp(75)
        clip: true
        opacity: disabled?0.5:1
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }

        Item {
            id: iconContent

            anchors {
                horizontalCenter: parent.horizontalCenter
                verticalCenter: parent.bottom
                verticalCenterOffset: Units.dp(-15)
            }
            width: childrenRect.width
            height: childrenRect.height
        }
    }

    Image {
        id: sep
        source: "qrc:/img/menu_media_item_separator.png"
        opacity: disabled?0:1
        anchors.centerIn: parent
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
    }

    Text {
        id: buttonTxt
        horizontalAlignment: Text.AlignHCenter
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(28)
        }
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(15)
        font.family: calaosFont.fontFamilyLight
        font.weight: Font.ExtraLight
        color: disabled?"#e7e7e7":active?"#34B4D7":"#ffffff"
    }

    Text {
        id: buttonDesc
        horizontalAlignment: Text.AlignHCenter
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(48)
        }
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(10)
        font.family: calaosFont.fontFamilyLight
        font.weight: Font.ExtraLight
        color: "#ffffff"
        opacity: disabled?0.1:0.4
        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
    }

    Image {
        source: "qrc:/img/menu_media_item_glow.png"
        opacity: active?1:0
        anchors.centerIn: parent

    }

    MouseArea {
        anchors.fill: parent
        onPressed: active = true
        onReleased: active = false
        onClicked: buttonClicked()
        enabled: !disabled
    }
}
