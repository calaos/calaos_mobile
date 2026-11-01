import QtQuick
import Calaos
import SharedComponents
import QtQuick.Layouts
import "../quickflux"

Item {
    id: eventLogView

    readonly property bool failed: eventLogModel.error && !eventLogModel.loading

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: listViewLog
        objectName: "eventLogList"
        model: eventLogModel

        y: header.height
        width: parent.width
        height: parent.height - header.height

        delegate: default_delegate

        section.property: "evDate"
        section.criteria: ViewSection.FullString
        section.delegate: sectionHeading
        section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

        onAtYEndChanged: {
            if (listViewLog.atYEnd)
                eventLogModel.loadMore()
        }

        onDragEnded: if (pullHeader.refresh) { eventLogModel.refresh() }

        PullRefreshHeader {
            id: pullHeader
            listview: listViewLog
            y: -listViewLog.contentY - height
        }
    }

    Connections {
        target: eventLogModel

        //A page merged into the last rows leaves the list at its end: atYEnd
        //does not change and nothing else would ask for the next one.
        function onLoadingChanged() {
            if (eventLogModel.loading || eventLogModel.error)
                return
            listViewLog.forceLayout()
            if (listViewLog.atYEnd)
                eventLogModel.loadMore()
        }
    }

    //Over the list rather than its footer: a footer moves atYEnd, and it
    //would stay below the fold when the page was asked for from further up.
    Rectangle {
        objectName: "eventLogMore"
        anchors { left: listViewLog.left; right: listViewLog.right; bottom: listViewLog.bottom }
        height: Units.dp(56)
        color: Theme.backgroundColor
        visible: eventLogModel.loadingMore && listViewLog.count > 0

        Row {
            anchors.centerIn: parent
            spacing: Units.dp(12)

            LoadingSpinner {
                size: Units.dp(20)
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.mutedColor
                font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
                text: qsTr("Loading earlier events…")
            }
        }
    }

    Column {
        objectName: "eventLogState"
        x: Units.dp(32)
        y: header.height + Units.dp(56)
        width: parent.width - 2 * x
        spacing: Units.dp(12)
        visible: listViewLog.count === 0

        LoadingSpinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: eventLogModel.loading
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Units.dp(64)
            height: width
            radius: width / 2
            color: Theme.colorAlpha(eventLogView.failed ? Theme.redColor : Theme.blueColor, 0.14)
            visible: !eventLogModel.loading

            SvgGlyph {
                anchors.centerIn: parent
                size: Units.dp(32)
                name: eventLogView.failed ? "error_outline" : "history"
            }
        }

        Text {
            objectName: "eventLogMessage"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            textFormat: Text.RichText
            color: Theme.whiteColor
            font { family: calaosFont.fontFamily; pointSize: 12 }
            text: eventLogModel.loading ? qsTr("Loading…") :
                  eventLogView.failed ? qsTr("Could not load the events.") :
                  qsTr("No events in log.<br>Calaos Server <b>v3.1</b> is required.")
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.mutedColor
            font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
            text: qsTr("Check the connection to the box.")
            visible: eventLogView.failed
        }

        Item {
            width: parent.width
            height: retryButton.height + Units.dp(8)
            visible: eventLogView.failed

            SettingsButton {
                id: retryButton
                objectName: "eventLogRetry"
                small: true
                glyph: "refresh"
                text: qsTr("Retry")
                anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                onButtonClicked: eventLogModel.refresh()
            }
        }
    }

    ScrollBar { listObject: listViewLog }

    ViewHeader {
        id: header
        headerLabel: qsTr("Event log")
        glyph: "history"

        ItemButtonAction {
            iconSource: "icon_filter"

            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: Units.dp(10)
            }

            //onButtonClicked: AppActions.openEventPushViewer(evNotifText, evPictureUrl)

//            visible: listViewLog.count > 0
            visible: false
        }
    }

    Component {
        id: sectionHeading

        //Opaque: it stays at the top while the events scroll under it
        Rectangle {
            z: 99
            width: listViewLog.width
            height: groupTitle.height
            color: Theme.backgroundColor

            ListGroupHeader {
                id: groupTitle
                title: section
            }
        }
    }

    Component {
        id: default_delegate

        Item {
            width: ListView.view.width
            height: Math.max(Units.dp(44),
                             Math.max(timeColumn.implicitHeight, textColumn.implicitHeight) + Units.dp(16))

            Column {
                id: timeColumn
                x: Units.dp(16)
                y: Units.dp(8)
                width: Units.dp(60)
                spacing: Units.dp(4)

                Text {
                    topPadding: Units.dp(3)
                    color: Theme.yellowColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
                    text: evTime
                }

                Rectangle {
                    objectName: "eventCount"
                    visible: evCount > 1
                    width: Math.max(Units.dp(32), countLabel.implicitWidth + Units.dp(16))
                    height: Units.dp(24)
                    radius: Units.dp(12)
                    color: Theme.colorAlpha(Theme.blueColor, 0.18)

                    Accessible.role: Accessible.StaticText
                    Accessible.name: qsTr("%1 times").arg(evCount)

                    Text {
                        id: countLabel
                        anchors.centerIn: parent
                        color: Theme.blueColor
                        font { family: calaosFont.fontFamily; bold: true; pixelSize: Units.dp(13) }
                        text: evCount + "x"
                    }
                }
            }

            Column {
                id: textColumn
                x: timeColumn.x + timeColumn.width + Units.dp(12)
                y: Units.dp(8)
                width: parent.width - x - Units.dp(16)
                spacing: Units.dp(4)

                RowLayout {
                    width: parent.width
                    spacing: Units.dp(8)

                    IconItem {
                        source: calaosApp.getPictureSized(evIconSource)
                        visible: evIconSource != ""
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        color: Theme.blueColor
                        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                        text: evTitle
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Flow {
                    width: parent.width
                    visible: evType == Common.EventIoChanged

                    Text {
                        width: Math.min(implicitWidth, parent.width)
                        elide: Text.ElideRight
                        color: Theme.whiteColor
                        font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 10 }
                        text: evRoomName
                    }
                    Text {
                        color: Theme.whiteColor
                        font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 10 }
                        text: " · "
                        visible: evRoomName != "" && evActionText != ""
                    }
                    Text {
                        width: Math.min(implicitWidth, parent.width)
                        elide: Text.ElideRight
                        color: Theme.yellowColor
                        font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 10 }
                        text: evActionText
                    }
                }

                RowLayout {
                    width: parent.width
                    spacing: Units.dp(8)

                    visible: evType == Common.EventPush

                    Text {
                        color: Theme.whiteColor
                        font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 10 }
                        text: evNotifText
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    ItemButtonAction {
                        iconSource: "icon_camera_title"
                        visible: evHasPicture
                        onButtonClicked: AppActions.openEventPushViewer(evNotifText, evPictureUrl)
                    }
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Theme.borderColor
            }
        }
    }
}
