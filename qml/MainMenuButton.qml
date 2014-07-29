import QtQuick 2.0

Item {

    property string iconBase
    property string iconGlow
    property string iconBloom
    property bool selected: false
    property alias label: txt.text
    signal buttonClicked()

    width: 142 / 2 * calaosApp.density
    height: 94 / 2 * calaosApp.density

    Image {
        id: bg
        source: iconBase
        anchors.fill: parent
    }

    Image {
        id: glow
        source: iconGlow
        anchors.fill: parent

        opacity: selected?1:0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { easing.type: Easing.OutQuad; duration: 100 } }
    }

    Image {
        id: bloom
        source: iconBloom
        anchors.fill: parent

        opacity: 0
    }

    Text {
        id: txt
        color: "#e7e7e7"
        font { bold: false; pointSize: 10 * calaosApp.density }
        anchors {
            centerIn: parent
            verticalCenterOffset: 12
        }
    }

    SequentialAnimation {
        id: animBloom
        running: false
        NumberAnimation {
            target: bloom
            property: "opacity"
            easing.type: Easing.OutQuad;
            duration: 100
            from: 0
            to: 1
        }
        PauseAnimation { duration: 80 }
        NumberAnimation {
            target: bloom
            property: "opacity"
            easing.type: Easing.InQuad;
            duration: 30
            from: 1
            to: 0
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            selected = true
            animBloom.start()
            buttonClicked()
        }
    }

}
