import QtQuick
import SharedComponents

//Non modal update notice, docked in the top right corner.
//It never dims the screen and never takes the focus: the panel stays fully
//usable while the notice is on screen. It stays until the user answers it.
Item {
    id: toast

    property int count: 0
    property int calaosCount: 0
    property var calaosPackages: []

    property bool shown: false

    //A wall panel is read from a distance, so the notice is sized for legibility
    //rather than for discretion. It still occupies less than a tenth of the screen.
    readonly property int fullWidth: Units.dp(340)

    //Beyond this the list stops being scannable and becomes a wall of names
    readonly property int maxListed: 3

    readonly property var listedPackages: calaosPackages.slice(0, maxListed)
    readonly property int overflowCount: count - listedPackages.length

    signal accepted()
    signal dismissed()

    function showToast(msg) {
        count = msg.hasOwnProperty("count") ? msg.count : 0
        calaosCount = msg.hasOwnProperty("calaosCount") ? msg.calaosCount : 0
        calaosPackages = msg.hasOwnProperty("calaosPackages") ? msg.calaosPackages : []
        shown = true
    }

    function hide() {
        shown = false
    }

    width: fullWidth
    height: panel.height
    visible: shown || panel.opacity > 0

    //Compact action, styled after CalaosItemBase but sized for a notice.
    //`primary` carries the cyan fill, everything else stays a quiet text button.
    component ToastButton: Item {
        id: btn

        property alias text: btnLabel.text
        property bool primary: false
        property bool hovered: false

        signal clicked()

        implicitWidth: btnLabel.implicitWidth + Units.dp(30)
        implicitHeight: Units.dp(34)

        Rectangle {
            id: btnBg

            anchors.fill: parent
            radius: Units.dp(4)
            color: btn.primary ? Theme.colorAlpha(Theme.blueColor, btn.hovered ? 0.2 : 0.12)
                               : "transparent"
            border.width: btn.primary ? Units.dp(1) : 0
            border.color: Theme.colorAlpha(Theme.blueColor, 0.55)

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        //Same press feedback as the rest of the app: a short cyan flash
        Rectangle {
            id: btnFlash

            anchors.fill: parent
            anchors.margins: Units.dp(2)
            radius: Units.dp(3)
            color: Theme.blueColor
            opacity: 0

            Behavior on opacity { NumberAnimation { duration: 120 } }
        }

        Text {
            id: btnLabel

            anchors.centerIn: parent
            font {
                family: calaosFont.fontFamily
                weight: btn.primary ? Font.Medium : Font.Light
                pixelSize: Units.dp(14)
            }
            color: btn.primary ? Theme.blueColor : Theme.colorAlpha(Theme.whiteColor, 0.55)
        }

        MouseArea {
            anchors.fill: parent
            //widen the touch target without widening the button
            anchors.margins: -Units.dp(6)
            hoverEnabled: true

            onEntered: btn.hovered = true
            onExited: btn.hovered = false
            onPressed: btnFlash.opacity = 0.15
            onReleased: btnFlash.opacity = 0
            onCanceled: btnFlash.opacity = 0
            onClicked: btn.clicked()
        }
    }

    Rectangle {
        id: panel

        //The panel unfolds from its rail, so only the width is animated and the
        //content keeps a fixed layout width, never reflowing mid animation.
        clip: true
        width: Units.dp(3)
        opacity: 0

        height: content.implicitHeight + Units.dp(36)

        radius: Units.dp(5)
        border.width: Units.dp(1)
        border.color: Theme.colorAlpha(Theme.blueColor, 0.4)

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1c242a" }
            GradientStop { position: 1.0; color: "#12171a" }
        }

        anchors {
            left: parent.left
            top: parent.top
        }

        //Swallow taps so nothing behind the notice reacts to a missed press
        MouseArea {
            anchors.fill: parent
            onClicked: {}
        }

        //Severity rail. Yellow is what "update available" already means in
        //ConfigUpdateView, the same way the error banner rail is red.
        Rectangle {
            id: rail

            width: Units.dp(3)
            radius: width / 2
            color: Theme.yellowColor

            anchors {
                left: parent.left; leftMargin: Units.dp(1)
                top: parent.top; topMargin: Units.dp(6)
                bottom: parent.bottom; bottomMargin: Units.dp(6)
            }
        }

        Column {
            id: content

            //fixed width: the reveal must not re-wrap the text
            width: toast.fullWidth - Units.dp(20) - Units.dp(18)
            spacing: Units.dp(4)
            opacity: 0

            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                top: parent.top; topMargin: Units.dp(18)
            }

            Text {
                font {
                    family: calaosFont.fontFamily
                    weight: Font.Medium
                    pixelSize: Units.dp(10)
                    letterSpacing: Units.dp(1.6)
                    capitalization: Font.AllUppercase
                }
                color: Theme.colorAlpha(Theme.whiteColor, 0.4)
                text: qsTr("System update")
            }

            Item { width: 1; height: Units.dp(6) }

            Text {
                width: parent.width
                elide: Text.ElideRight
                font {
                    family: calaosFont.fontFamilyLight
                    weight: Font.ExtraLight
                    pixelSize: Units.dp(22)
                }
                color: Theme.colorAlpha(Theme.whiteColor, 0.92)
                text: qsTr("%n update(s) available", "", toast.count)
            }

            Item { width: 1; height: Units.dp(6) }

            //Which Calaos components are involved, using the same status dots
            //as the update page the notice leads to
            Repeater {
                model: toast.listedPackages

                delegate: Item {
                    id: pkgRow

                    required property string modelData

                    width: parent.width
                    height: Units.dp(22)

                    Rectangle {
                        id: pkgDot

                        width: Units.dp(7); height: Units.dp(7)
                        radius: width / 2
                        color: Theme.colorAlpha(Theme.yellowColor, 0.8)

                        anchors {
                            left: parent.left
                            verticalCenter: parent.verticalCenter
                        }
                    }

                    Text {
                        elide: Text.ElideRight
                        font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(14) }
                        color: Theme.colorAlpha(Theme.whiteColor, 0.75)
                        text: pkgRow.modelData

                        anchors {
                            left: pkgDot.right; leftMargin: Units.dp(10)
                            right: parent.right
                            verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Text {
                visible: toast.listedPackages.length > 0 && toast.overflowCount > 0
                leftPadding: Units.dp(17)
                topPadding: Units.dp(2)
                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(12) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.35)
                text: qsTr("+%n other package(s)", "", toast.overflowCount)
            }

            //Nothing from Calaos itself, only distribution packages
            Text {
                visible: toast.listedPackages.length === 0
                width: parent.width
                elide: Text.ElideRight
                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(14) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.6)
                text: qsTr("System and kernel packages")
            }

            Item { width: 1; height: Units.dp(10) }

            Item {
                width: content.width
                height: actions.height

                Row {
                    id: actions

                    anchors.right: parent.right
                    spacing: Units.dp(6)

                    ToastButton {
                        text: qsTr("Later")
                        onClicked: {
                            toast.hide()
                            toast.dismissed()
                        }
                    }

                    ToastButton {
                        text: qsTr("View updates")
                        primary: true
                        onClicked: {
                            toast.hide()
                            toast.accepted()
                        }
                    }
                }
            }
        }

        states: State {
            name: "shown"
            when: toast.shown

            PropertyChanges { target: panel; width: toast.fullWidth; opacity: 1 }
            PropertyChanges { target: content; opacity: 1 }
        }

        //The rail lights up first, then the panel unfolds from it, then the
        //content settles in. One orchestrated moment instead of three effects.
        transitions: [
            Transition {
                to: "shown"

                SequentialAnimation {
                    NumberAnimation { target: panel; property: "opacity"; duration: 130 }
                    NumberAnimation {
                        target: panel; property: "width"
                        duration: 280; easing.type: Easing.OutCubic
                    }
                    NumberAnimation { target: content; property: "opacity"; duration: 200 }
                }
            },
            Transition {
                from: "shown"

                SequentialAnimation {
                    NumberAnimation { target: content; property: "opacity"; duration: 120 }
                    NumberAnimation {
                        target: panel; property: "width"
                        duration: 220; easing.type: Easing.InCubic
                    }
                    NumberAnimation { target: panel; property: "opacity"; duration: 120 }
                }
            }
        ]
    }
}
