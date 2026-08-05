import QtQuick
import SharedComponents
import QtQuick.Layouts

Item {
    id: panel

    function stepLabel(step) {
        if (step === "download") return qsTr("Downloading")
        if (step === "unpack") return qsTr("Unpacking")
        if (step === "configure") return qsTr("Configuring")
        if (step === "install") return qsTr("Installing")
        if (step === "pending") return qsTr("Waiting")
        if (step === "done") return qsTr("Done")
        if (step === "error") return qsTr("Error")
        return step
    }

    function stateLabel(s) {
        if (s === "downloading") return qsTr("Downloading")
        if (s === "unpacking") return qsTr("Unpacking")
        if (s === "installing") return qsTr("Installing")
        if (s === "done") return qsTr("Done")
        if (s === "failed") return qsTr("Failed")
        return qsTr("Waiting")
    }

    function stateColor(s) {
        if (s === "done") return Theme.greenColor
        if (s === "failed") return Theme.redColor
        if (isActiveState(s)) return Theme.blueColor
        return Theme.colorAlpha(Theme.whiteColor, 0.4)
    }

    function isActiveState(s) {
        return s === "downloading" || s === "unpacking" || s === "installing"
    }

    function isActiveStep(s) {
        return s === "download" || s === "unpack" || s === "configure" || s === "install"
    }

    Item {
        width: Math.min(parent.width - Units.dp(60), Units.dp(640))

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: parent.top; topMargin: Units.dp(26)
            bottom: parent.bottom; bottomMargin: Units.dp(16)
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Units.dp(12)

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font { family: calaosFont.fontFamilyLight; weight: Font.ExtraLight; pixelSize: Units.dp(24) }
                color: Theme.whiteColor
                text: qsTr("Installing updates…")
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(13) }
                color: Theme.colorAlpha(Theme.yellowColor, 0.7)
                text: qsTr("Do not power off the machine")
            }

            Item { Layout.preferredHeight: Units.dp(8) }

            //Global progress bar
            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(14)

                Rectangle {
                    id: track

                    Layout.fillWidth: true
                    Layout.preferredHeight: Units.dp(12)
                    radius: height / 2
                    color: "transparent"
                    border.color: Theme.colorAlpha(Theme.blueColor, 0.3)
                    border.width: Units.dp(1)

                    Rectangle {
                        anchors {
                            left: parent.left; leftMargin: Units.dp(2)
                            top: parent.top; topMargin: Units.dp(2)
                            bottom: parent.bottom; bottomMargin: Units.dp(2)
                        }

                        radius: height / 2
                        color: Theme.blueColor
                        width: Math.max(height, (track.width - Units.dp(4)) * updateManager.totalPercent / 100)
                        visible: updateManager.totalPercent > 0

                        Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutQuad } }
                    }
                }

                Text {
                    Layout.preferredWidth: Units.dp(64)
                    horizontalAlignment: Text.AlignRight
                    font { family: calaosFont.fontFamilyLight; weight: Font.ExtraLight; pixelSize: Units.dp(22) }
                    color: Theme.blueColor
                    text: updateManager.totalPercent + "%"
                }
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                visible: updateManager.pkgCount > 0
                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(13) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.45)
                text: qsTr("Package %1 of %2").arg(updateManager.pkgIndex).arg(updateManager.pkgCount)
            }

            //Currently processed package
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: parent.width
                spacing: Units.dp(8)
                visible: updateManager.currentPackage !== ""

                Text {
                    Layout.maximumWidth: Units.dp(340)
                    elide: Text.ElideMiddle
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pixelSize: Units.dp(15) }
                    color: Theme.colorAlpha(Theme.whiteColor, 0.85)
                    text: updateManager.currentPackage
                }

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
                    color: updateManager.currentStep === "error" ?
                               Theme.redColor:
                               Theme.colorAlpha(Theme.blueColor, 0.9)
                    text: panel.stepLabel(updateManager.currentStep) +
                          (panel.isActiveStep(updateManager.currentStep) ?
                               " " + updateManager.currentPercent + "%": "")
                }
            }

            Item { Layout.preferredHeight: Units.dp(4) }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Units.dp(1)
                color: Theme.colorAlpha(Theme.whiteColor, 0.1)
            }

            //Per-package progress list
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: pkgList

                    anchors.fill: parent
                    anchors.topMargin: Units.dp(4)
                    spacing: Units.dp(4)

                    model: updateProgressModel

                    delegate: Item {
                        id: pkgRow

                        required property string ppName
                        required property string ppState
                        required property int ppPercent

                        width: pkgList.width - Units.dp(8)
                        height: Units.dp(30)

                        Rectangle {
                            id: pkgDot
                            width: Units.dp(7); height: Units.dp(7)
                            radius: width / 2
                            color: panel.stateColor(pkgRow.ppState)

                            anchors {
                                left: parent.left
                                verticalCenter: parent.verticalCenter
                            }
                        }

                        Text {
                            id: pkgName
                            elide: Text.ElideMiddle
                            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(14) }
                            color: pkgRow.ppState === "pending" ?
                                       Theme.colorAlpha(Theme.whiteColor, 0.4):
                                       Theme.colorAlpha(Theme.whiteColor, 0.85)
                            text: pkgRow.ppName

                            anchors {
                                left: pkgDot.right; leftMargin: Units.dp(10)
                                right: pkgState.left; rightMargin: Units.dp(10)
                                verticalCenter: parent.verticalCenter
                                verticalCenterOffset: panel.isActiveState(pkgRow.ppState) ? -Units.dp(4) : 0
                            }
                        }

                        Text {
                            id: pkgState
                            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(12) }
                            color: panel.stateColor(pkgRow.ppState)
                            text: panel.stateLabel(pkgRow.ppState)

                            anchors {
                                right: parent.right
                                verticalCenter: parent.verticalCenter
                            }
                        }

                        //Thin progress bar for the package being processed
                        Rectangle {
                            id: pkgTrack
                            visible: panel.isActiveState(pkgRow.ppState)
                            height: Units.dp(3)
                            color: "transparent"
                            border.color: Theme.colorAlpha(Theme.blueColor, 0.3)
                            border.width: 1

                            anchors {
                                left: pkgName.left
                                right: pkgState.left; rightMargin: Units.dp(10)
                                bottom: parent.bottom; bottomMargin: Units.dp(3)
                            }

                            Rectangle {
                                anchors {
                                    left: parent.left; leftMargin: 1
                                    top: parent.top; topMargin: 1
                                    bottom: parent.bottom; bottomMargin: 1
                                }

                                color: Theme.blueColor
                                width: (pkgTrack.width - 2) * pkgRow.ppPercent / 100
                            }
                        }
                    }
                }

                ScrollBar { listObject: pkgList }
            }
        }
    }

    //Keep the package being processed visible in the list
    Connections {
        target: updateManager
        function onPkgIndexChanged() {
            if (pkgList.count > 0) {
                var i = Math.max(0, Math.min(updateManager.pkgIndex - 1, pkgList.count - 1))
                pkgList.positionViewAtIndex(i, ListView.Contain)
            }
        }
    }
}
