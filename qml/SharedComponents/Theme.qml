import QtQuick

pragma Singleton

QtObject {
    id: theme

    property color blueColor: "#3AB4D7"
    property color yellowColor: "#ffda5a"
    property color redColor: "#ff5555"
    property color greenColor: "#5fd35f"
    property color whiteColor: "#e7e7e7"
    property color mutedColor: "#868482"
    property color amberColor: "#e0a356"
    property color backgroundColor: "#171717"
    property color borderColor: "#424242"

    //Qt has no query for the system "reduce animations" setting; this is the
    //single switch the continuous animations honour.
    property bool reduceMotion: false

    function colorAlpha(c, alpha) {
        var realColor = Qt.darker(c, 1)
        realColor.a = alpha
        return realColor
    }
}
