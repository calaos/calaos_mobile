import QtQuick 2.0

BorderImage {

    property string icon: ""
    property alias buttonLabel: txt.text
    signal buttonClicked()

    height: 40 * calaosApp.density
    width: ic.width + txt.implicitWidth + 20 * calaosApp.density < 120 * calaosApp.density?
               120 * calaosApp.density:
               ic.width + txt.implicitWidth + 20 * calaosApp.density

    source: calaosApp.getPictureSized("button_footer_bg")

    border {
        left: 40 * calaosApp.density
        right: 40 * calaosApp.density
        top: 10 * calaosApp.density
        bottom: 10 * calaosApp.density
    }

    Image {
        id: ic

        source: icon
        width: icon === ""?0: 34 * calaosApp.density
        height: 34 * calaosApp.density

        anchors {
            left: parent.left; leftMargin: 3 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: txt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors {
            left: ic.right; leftMargin: 5 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: buttonClicked()
    }
}
