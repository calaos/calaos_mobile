import QtQuick
// Kept on Qt5Compat deliberately: MultiEffect cannot reproduce ColorOverlay.
// ColorOverlay replaces the source RGB by the tint and keeps the alpha, so the
// glyph comes out as a flat white silhouette. MultiEffect's colorization
// multiplies the tint by the source *luminance* instead, so a dark glyph stays
// dark; the only knob that flattens luminance (brightness: 1.0) saturates the
// antialiased edge pixels to opaque white and destroys the outline. See T29.
import Qt5Compat.GraphicalEffects
import "."

Item {

    property string imageSource: "button_empty"
    property string iconSource: ""

    signal buttonClicked()

    width: Units.dp(37)
    height: Units.dp(31)

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
