import QtQuick
import QtQuick.Layouts
import "."

Image {
    width: Units.dp(22)
    height: Units.dp(22)

    // A layout ignores width/height and uses the implicit size of the source.
    Layout.preferredWidth: width
    Layout.preferredHeight: height

    // Logical size: Qt multiplies by the device pixel ratio itself.
    sourceSize: Qt.size(width, height)
}
