import QtQuick 2.0

ItemBase {
    property variant modelData

    height: 130 * calaosApp.density

    Text {
        id: name
        color: !modelData.stateShutterBool?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            right: btup.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40 * calaosApp.density
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: btdown
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40 * calaosApp.density
        }
        imageSource: "button_down2"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendDown()
        }
    }

    ItemButtonAction {
        id: btstop
        anchors {
            right: btdown.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40 * calaosApp.density
        }
        imageSource: "button_stop"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendStop()
        }
    }

    ItemButtonAction {
        id: btup
        anchors {
            right: btstop.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40 * calaosApp.density
        }
        imageSource: "button_up2"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendUp()
        }
    }

    Text {
        id: stateTxt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 11 }
        text: modelData.stateShutterTxt
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: windowBg.right; leftMargin: 8 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            top: name.bottom; topMargin: 18 * calaosApp.density
        }
    }

    Text {
        id: stateTxtAct
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 11 }
        text: modelData.stateShutterTxtAction
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: windowBg.right; leftMargin: 8 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            top: stateTxt.bottom; topMargin: 8 * calaosApp.density
        }
    }

    property int shutterPos: modelData.stateShutterPos

    onShutterPosChanged: {
        // shutterPos can be 0 <-> 100
        // picture should be -45 <-> 0
        var val = Math.round(shutterPos * 45 * calaosApp.density / 100) - 45 * calaosApp.density
         shutter.anchors.verticalCenterOffset = val
    }

    Item {
        anchors {
            horizontalCenterOffset: 7 * calaosApp.density
            centerIn: windowBg
        }
        clip: true
        height: shutter.height
        width: shutter.width
        Image {
            id: shutter
            source: calaosApp.getPictureSized("part_shutter2")
            anchors.centerIn: parent
        }
    }

    Image {
        id: windowBg
        source: calaosApp.getPictureSized("part_shutter")
        anchors {
            left: parent.left; leftMargin: 14 * calaosApp.density
            top: name.bottom; topMargin: 8 * calaosApp.density
        }
    }

}
