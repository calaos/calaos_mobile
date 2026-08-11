//the animated icon is loaded from a nested component, bind it to this file's
//scope so it can read the properties of the root item
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."

//Shared skeleton of the binary (on/off) IO widgets: IOLight, IOOutlet, IOPump,
//IOBoiler and IOHeater. They all draw the same row (icon, name, sensor status,
//two action buttons) and only differ by their artwork, so each wrapper only has
//to set the properties below.
ItemBase {
    id: root

    property variant modelData

    readonly property bool deviceOn: root.modelData.stateBool

    //icon: either a frame by frame animation (countImage frames, iconOn holds a
    //%1 placeholder for the frame number) or a plain image swapped on state
    //change
    property bool animatedIcon: false
    property int countImage: 9
    property string iconOn
    property string iconOff
    //fixed image drawn behind the animated icon (the body of the pump)
    property string iconBackground: ""
    //spin the animated icon while the device is on, only meaningful on a pump
    property bool spinningIcon: false

    //action buttons: either a full button artwork (imageSource, as the light
    //buttons do) or an svg glyph drawn over the default button background
    //(iconSource). ic_outlet_on/off.svg are, despite their name, the generic
    //power marks (filled ring / empty ring) used by every device but the light.
    property string buttonImageOn: "button_empty"
    property string buttonImageOff: "button_empty"
    property string buttonIconOn: ""
    property string buttonIconOff: ""

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        IconItem {
            id: icon

            source: calaosApp.getPictureSized(root.animatedIcon?
                                                  root.iconBackground:
                                                  (root.deviceOn? root.iconOn: root.iconOff))

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            Loader {
                active: root.animatedIcon

                sourceComponent: AnimatedIcon {
                    id: animatedIcon

                    countImage: root.countImage
                    imageFilenameOn: root.iconOn
                    imageFilenameOff: root.iconOff

                    iconState: root.deviceOn

                    onAnimationFinished: {
                        if (root.spinningIcon)
                            animRot.running = animatedIcon.iconState
                    }

                    RotationAnimation {
                        id: animRot
                        target: animatedIcon;
                        from: 0;
                        to: 360;
                        duration: 1000
                        running: false
                        loops: Animation.Infinite
                    }
                }
            }
        }

        ScrollingText {
            color: root.deviceOn? Theme.yellowColor: Theme.blueColor
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: root.modelData.ioName
            clip: true
            Layout.fillWidth: true

            Behavior on color { ColorAnimation { duration: 200 } }
        }

        SensorStatusIcon {
            sensor: root.modelData
        }

        //buttons go from deactivate to activate, left to right: in a RowLayout
        //the declaration order is the visual order, unlike the anchors this
        //code used to rely on
        ItemButtonAction {
            id: btoff
            imageSource: root.buttonImageOff
            iconSource: root.buttonIconOff

            onButtonClicked: {
                root.feedbackAnim()
                root.modelData.sendFalse()
            }
        }

        ItemButtonAction {
            id: bton
            imageSource: root.buttonImageOn
            iconSource: root.buttonIconOn

            onButtonClicked: {
                root.feedbackAnim()
                root.modelData.sendTrue()
            }
        }
    }
}
