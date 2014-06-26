import QtQuick 2.0

BorderImage {
    property variant modelData

    source: "qrc:/img/back_items_home.png"
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Image {
        id: icon

        source: "qrc:/img/icon_scenario.png"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        fillMode: Image.PreserveAspectFit
        height: sourceSize.height * calaosApp.density
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { bold: false; pointSize: 12 * calaosApp.density }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btplay.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    function flashAnim() {
        name.color = "#ffda5a"
        timer.start()
    }

    Timer {
        id: timer
        running: false
        interval: 1500
        onTriggered: name.color = "#3ab4d7"
        repeat: false
    }

    ItemButtonAction {
        id: btplay
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_play.png"

        onButtonClicked: {
            flashAnim()
            modelData.sendTrue()
        }
    }
}
