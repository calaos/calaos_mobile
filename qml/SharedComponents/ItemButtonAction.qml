import QtQuick
import Qt5Compat.GraphicalEffects
import "."

Item {

    property string imageSource: "button_empty"
    property string iconSource: ""

    signal buttonClicked()

    width: 37 * calaosApp.density
    height: 31 * calaosApp.density

    Image {
        id: img

        source: calaosApp.getPictureSized(imageSource)

        anchors.fill: parent

        fillMode: Image.PreserveAspectFit

        Image {
            id: imgIcon

            source: iconSource.endsWith(".svg")?
                        iconSource: calaosApp.getPictureSized(iconSource)
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: Units.dp(18)
            visible: iconSource != ""
        }

        ColorOverlay {
            anchors.fill: imgIcon
            source: imgIcon
            color: "#ffffff"
            visible: iconSource != ""
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                glowAnim.restart()
                buttonClicked()
            }
        }
    }

    Image {
        id: imgGlow

        source: calaosApp.getPictureSized("button_action_glow")

        anchors.fill: img

        fillMode: Image.PreserveAspectFit

        opacity: 0
    }

    SequentialAnimation {
        id: glowAnim
        PropertyAnimation { target: imgGlow; properties: "opacity"; to: 1.0; duration: 100 }
        PropertyAnimation { target: imgGlow; properties: "opacity"; to: 0.0; duration: 800 }
    }
}
