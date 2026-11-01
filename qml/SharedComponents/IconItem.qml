import QtQuick
import QtQuick.Layouts
import "."

Image {
    width: Units.dp(22)
    height: Units.dp(22)

    // A layout ignores width/height and takes the native size of the source: a
    // 44x44 SVG came out twice as big as the 22x22 PNGs.
    Layout.preferredWidth: width
    Layout.preferredHeight: height

    // Vector sources are rasterised at device pixels so they stay sharp on HiDPI.
    sourceSize: Qt.size(width * Screen.devicePixelRatio, height * Screen.devicePixelRatio)
}
