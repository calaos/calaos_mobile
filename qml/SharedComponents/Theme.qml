import QtQuick

pragma Singleton

QtObject {
    id: theme

    property color blueColor: "#3AB4D7"
    property color yellowColor: "#ffda5a"
    property color redColor: "#ff5555"
    property color greenColor: "#5fd35f"
    property color whiteColor: "#e7e7e7"
    property color backgroundColor: "#171717"

    function colorAlpha(c, alpha) {
        var realColor = Qt.darker(c, 1)
        realColor.a = alpha
        return realColor
    }
}
