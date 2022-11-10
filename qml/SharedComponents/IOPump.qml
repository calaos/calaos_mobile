import QtQuick
import "."

ItemBase {
    property variant modelData

    IconItem {
        id: iconBg
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        source: calaosApp.getPictureSized("icon_pump_bg")
    }

    AnimatedIcon {
        id: icon

        countImage: 9
        imageFilenameOn: "icon_pump_00%1"
        imageFilenameOff: "icon_pump_off"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        iconState: modelData.stateBool

        onAnimationFinished: {
            if (iconState)
                animRot.running = true
            else
                animRot.running = false
        }

        RotationAnimation {
            id: animRot
            target: icon;
            from: 0;
            to: 360;
            duration: 1000
            running: false
            loops: Animation.Infinite
        }
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btoff.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        iconSource: "qrc:/img/ic_outlet_on.svg"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendTrue()
        }
    }

    ItemButtonAction {
        id: btoff
        anchors {
            right: bton.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        iconSource: "qrc:/img/ic_outlet_off.svg"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
        }
    }
}
