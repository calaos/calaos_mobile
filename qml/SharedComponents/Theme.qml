import QtQuick 2.5

pragma Singleton

QtObject {
    id: theme

    property color blueColor: "#3AB4D7"
    property color yellowColor: "#3AB4D7"
    property color whiteColor: "#e7e7e7"

    function colorAlpha(c, alpha) {
        var realColor = Qt.darker(c, 1)
        realColor.a = alpha
        return realColor
    }
}
