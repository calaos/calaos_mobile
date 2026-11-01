import QtQuick
import SharedComponents

//An icon of img/settings. They carry their colour: the effects that tint an
//image do not show on the software backend.
Image {
    property string name
    property real size: Units.dp(24)

    source: name === "" ? "" : "qrc:/img/settings/" + name + ".svg"
    width: size
    height: size
    sourceSize: Qt.size(size, size)
    fillMode: Image.PreserveAspectFit
}
