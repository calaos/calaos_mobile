import QtQuick
import Calaos
import SharedComponents
import QtQuick.Layouts

PageScaffold {
    id: page

    //Copy of the clicked row: the model may merge or reset under the detail.
    property var selected: null
    property QtObject picture: null
    property bool photoOpen: false

    readonly property bool failed: eventLogModel.error && !eventLogModel.loading

    title: qsTr("Event log")

    function select(row) {
        if (picture)
            picture.release()
        picture = null
        selected = row
        photoOpen = false
        if (row && row.hasPicture && row.picUid !== "")
            picture = eventLogModel.loadPicture(row.picUid)
    }

    //Fresh data on every visit: nothing loads the log at login on the kiosk.
    Component.onCompleted: eventLogModel.refresh()
    Component.onDestruction: if (picture) picture.release()

    //The kiosk keeps the page open across a reconnection, and nothing else
    //reloads the log after reset().
    Connections {
        target: calaosApp
        function onApplicationStatusChanged() {
            if (calaosApp.applicationStatus === Common.LoggedIn)
                eventLogModel.refresh()
        }
    }

    //A page merged into the last rows leaves the list at its end: atYEnd does
    //not change and nothing else would ask for the next one.
    Connections {
        target: eventLogModel
        function onLoadingChanged() {
            if (eventLogModel.loading || eventLogModel.error)
                return
            logList.forceLayout()
            if (logList.atYEnd)
                eventLogModel.loadMore()
        }
    }

    footerButtons: [
        FooterButton {
            label: qsTr("Back")
            icon: "qrc:/img/button_action_back.png"
            Layout.minimumWidth: width
            objectName: "eventBack"
            onBtClicked: {
                if (page.photoOpen)
                    page.photoOpen = false
                else
                    rootWindow.handleBack()
            }
        },

        FooterButton {
            label: qsTr("Quit")
            icon: "qrc:/img/button_action_quit.png"
            Layout.minimumWidth: width
            onBtClicked: rootWindow.goToDesktop()
        }
    ]

    BorderImage {
        id: listFrame
        source: "qrc:/img/standard_list_decoration.png"
        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }
        anchors {
            top: page.header.bottom
            bottom: page.footer.top
            left: parent.left
            margins: Units.dp(20)
        }
        width: parent.width * 0.58

        ListView {
            id: logList
            objectName: "eventLogList"
            anchors { fill: parent; topMargin: Units.dp(30); bottomMargin: Units.dp(30) }
            anchors.leftMargin: Units.dp(24)
            anchors.rightMargin: Units.dp(24)
            clip: true
            model: eventLogModel

            section.property: "evDate"
            section.criteria: ViewSection.FullString
            section.delegate: Text {
                width: logList.width
                height: Units.dp(30)
                verticalAlignment: Text.AlignVCenter
                color: Theme.blueColor
                font { family: calaosFont.fontFamilyLight; pixelSize: Units.dp(16) }
                text: section
            }

            onAtYEndChanged: if (atYEnd) eventLogModel.loadMore()

            delegate: Item {
                width: logList.width
                height: Math.max(Units.dp(52), rowText.implicitHeight + Units.dp(16))

                readonly property bool current: page.selected !== null &&
                                                page.selected.key === evTime + "|" + evTitle + "|" + evPicUid

                Rectangle {
                    anchors.fill: parent
                    color: Theme.colorAlpha(Theme.blueColor, 0.16)
                    visible: parent.current
                }

                Text {
                    id: timeLabel
                    x: Units.dp(8)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Units.dp(64)
                    color: Theme.yellowColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(16) }
                    text: evTime
                }

                Column {
                    id: rowText
                    anchors {
                        left: timeLabel.right; right: countBadge.left
                        leftMargin: Units.dp(8); rightMargin: Units.dp(8)
                        verticalCenter: parent.verticalCenter
                    }
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        color: Theme.whiteColor
                        font { family: calaosFont.fontFamily; pixelSize: Units.dp(17) }
                        text: evTitle
                    }
                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        color: Theme.mutedColor
                        font { family: calaosFont.fontFamilyLight; pixelSize: Units.dp(14) }
                        text: evType == Common.EventPush ? evNotifText :
                              evRoomName + (evRoomName != "" && evActionText != "" ? " · " : "") + evActionText
                    }
                }

                Rectangle {
                    id: countBadge
                    objectName: "eventCount"
                    anchors { right: cam.left; rightMargin: Units.dp(6); verticalCenter: parent.verticalCenter }
                    visible: evCount > 1
                    width: visible ? Math.max(Units.dp(32), countLabel.implicitWidth + Units.dp(14)) : 0
                    height: Units.dp(24)
                    radius: height / 2
                    color: Theme.colorAlpha(Theme.blueColor, 0.18)
                    Text {
                        id: countLabel
                        anchors.centerIn: parent
                        color: Theme.blueColor
                        font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
                        text: evCount + "x"
                    }
                }

                Image {
                    id: cam
                    anchors { right: parent.right; rightMargin: Units.dp(8); verticalCenter: parent.verticalCenter }
                    width: visible ? implicitWidth : 0
                    visible: evHasPicture
                    source: "qrc:/img/icon_camera_title.png"
                }

                Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 1
                    color: Theme.borderColor
                }

                MouseArea {
                    objectName: "eventRow"
                    anchors.fill: parent
                    onClicked: page.select({
                        key: evTime + "|" + evTitle + "|" + evPicUid,
                        date: evDate, time: evTime, title: evTitle,
                        isPush: evType == Common.EventPush,
                        room: evRoomName, action: evActionText, notif: evNotifText,
                        count: evCount, hasPicture: evHasPicture, picUid: evPicUid
                    })
                }
            }

            ScrollBar { listObject: logList }
        }

        Column {
            objectName: "eventLogState"
            anchors.centerIn: parent
            width: parent.width - Units.dp(80)
            spacing: Units.dp(12)
            visible: logList.count === 0

            Text {
                objectName: "eventLogMessage"
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                color: Theme.whiteColor
                font { family: calaosFont.fontFamily; pixelSize: Units.dp(20) }
                text: eventLogModel.loading || calaosApp.applicationStatus !== Common.LoggedIn ? qsTr("Loading…") :
                      page.failed ? qsTr("Could not load the events.") :
                      qsTr("No events in log.<br>Calaos Server <b>v3.1</b> is required.")
            }

            CalaosButton {
                objectName: "eventLogRetry"
                visible: page.failed
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Retry")
                height: Units.dp(48)
                onButtonClicked: eventLogModel.refresh()
            }
        }

        Text {
            objectName: "eventLogMore"
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: Units.dp(8) }
            visible: eventLogModel.loadingMore && logList.count > 0
            color: Theme.mutedColor
            font { family: calaosFont.fontFamily; pixelSize: Units.dp(14) }
            text: qsTr("Loading earlier events…")
        }
    }

    Item {
        id: detail
        objectName: "eventDetail"
        anchors {
            top: page.header.bottom
            bottom: page.footer.top
            left: listFrame.right
            right: parent.right
            margins: Units.dp(20)
        }

        Text {
            anchors.centerIn: parent
            visible: page.selected === null
            color: Theme.mutedColor
            font { family: calaosFont.fontFamilyLight; pixelSize: Units.dp(20) }
            text: qsTr("Select an event")
        }

        Column {
            visible: page.selected !== null
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: Units.dp(30) }
            spacing: Units.dp(10)

            Text {
                color: Theme.yellowColor
                font { family: calaosFont.fontFamily; pixelSize: Units.dp(18) }
                text: page.selected ? page.selected.date + "  " + page.selected.time +
                                      (page.selected.count > 1 ? "  (" + page.selected.count + "x)" : "") : ""
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.blueColor
                font { family: calaosFont.fontFamily; pixelSize: Units.dp(24) }
                text: page.selected ? page.selected.title : ""
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.whiteColor
                font { family: calaosFont.fontFamilyLight; pixelSize: Units.dp(18) }
                text: !page.selected ? "" :
                      page.selected.isPush ? page.selected.notif :
                      page.selected.room + (page.selected.room != "" && page.selected.action != "" ? " · " : "") + page.selected.action
            }

            Item {
                id: thumb
                objectName: "eventThumb"
                visible: page.picture !== null
                width: parent.width
                height: Units.dp(200)

                Rectangle { anchors.fill: parent; color: Theme.backgroundColor; border.color: Theme.borderColor }

                Image {
                    id: thumbImage
                    anchors { fill: parent; margins: 1 }
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    source: page.picture ? page.picture.url : ""
                }

                Text {
                    anchors.centerIn: parent
                    color: Theme.mutedColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(16) }
                    visible: page.picture && page.picture.loading
                    text: qsTr("Loading…")
                }

                Column {
                    anchors.centerIn: parent
                    spacing: Units.dp(8)
                    visible: page.picture && page.picture.failed
                    Text {
                        color: Theme.redColor
                        font { family: calaosFont.fontFamily; pixelSize: Units.dp(16) }
                        text: qsTr("The picture could not be loaded.")
                    }
                    CalaosButton {
                        objectName: "eventPictureRetry"
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Retry")
                        height: Units.dp(48)
                        onButtonClicked: page.picture.retry()
                    }
                }

                MouseArea {
                    objectName: "eventThumbArea"
                    anchors.fill: parent
                    enabled: thumbImage.status === Image.Ready
                    onClicked: page.photoOpen = true
                }
            }
        }
    }

    //In the page: the desktop popups are not children of it and would not
    //stack above the header band. The footer stays uncovered so that Back is
    //still there, and closes the photo first.
    Rectangle {
        id: photo
        objectName: "eventPhoto"
        anchors { top: page.header.bottom; bottom: page.footer.top; left: parent.left; right: parent.right }
        z: 10
        color: Theme.colorAlpha(Theme.backgroundColor, 0.97)
        visible: page.photoOpen

        onVisibleChanged: if (visible) forceActiveFocus()

        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
                page.photoOpen = false
                event.accepted = true
            }
        }

        Image {
            anchors { fill: parent; margins: Units.dp(20) }
            fillMode: Image.PreserveAspectFit
            source: page.photoOpen && page.picture ? page.picture.url : ""
        }

        MouseArea {
            objectName: "eventPhotoBackdrop"
            anchors.fill: parent
            onClicked: page.photoOpen = false
        }

        CalaosButton {
            objectName: "eventPhotoClose"
            anchors { top: parent.top; right: parent.right; margins: Units.dp(20) }
            width: Units.dp(120)
            text: qsTr("Close")
            height: Units.dp(48)
            onButtonClicked: page.photoOpen = false
        }
    }
}
