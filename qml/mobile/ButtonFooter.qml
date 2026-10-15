import QtQuick
import SharedComponents

BorderImage {

    property string icon: ""
    property alias buttonLabel: txt.text
    signal buttonClicked()

    height: Units.dp(40)
    width: ic.width + txt.implicitWidth + Units.dp(20) < Units.dp(120)?
               Units.dp(120):
               ic.width + txt.implicitWidth + Units.dp(20)

    source: calaosApp.getPictureSized("button_footer_bg")

    border {
        left: Units.dp(40)
        right: Units.dp(40)
        top: Units.dp(10)
        bottom: Units.dp(10)
    }

    Image {
        id: ic

        source: icon
        width: icon === ""?0: Units.dp(34)
        height: Units.dp(34)

        anchors {
            left: parent.left; leftMargin: Units.dp(3)
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: txt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors {
            left: ic.right; leftMargin: Units.dp(5)
            verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: buttonClicked()
    }
}
