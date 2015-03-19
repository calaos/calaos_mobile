import QtQuick 2.2
import Calaos 1.0

Item {
    property string context

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ButtonVoice {
        id: buttonVoice
        anchors {
            horizontalCenter: parent.horizontalCenter
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: voiceApi.voiceStatus !== Common.VoiceStatusResult?
                                      0:-(buttonVoice.x - calaosApp.dp(10))
        }
        Behavior on anchors.verticalCenterOffset { PropertyAnimation { easing.type: Easing.OutCubic } }
    }
    Text {
        id: textAction
        color: "#e7e7e7"
        font { bold: false; pointSize: 12 }
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: buttonVoice.bottom; topMargin: calaosApp.dp(10)
        }
    }

    Text {
        id: textResult
        color: "#b9b9b9"
        font { bold: false; italic: true; pointSize: 10 }
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: voiceApi.resultText
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: textAction.bottom; topMargin: calaosApp.dp(10)
        }
        visible: voiceApi.voiceStatus === Common.VoiceStatusNotRecognized ||
                 voiceApi.voiceStatus === Common.VoiceStatusResult
    }

//    Flickable {
//        anchors {
//            left: parent.left; right: parent.right
//            top: buttonVoice.bottom; topMargin: calaosApp.dp(10)
//            bottom: parent.bottom
//        }
//        contentWidth: textResult.width
//        contentHeight: textResult.height
//        Text {
//            id: textResult
//            color: "#e7e7e7"
//            font { bold: false; pointSize: 9 }
//            wrapMode: Text.WordWrap
//            text: voiceApi.resultJson
//        }
//    }

    ViewHeader {
        id: header
        voiceButtonVisible: false
        headerLabel: qsTr("Voice")
        iconSource: calaosApp.getPictureSized("voice")
    }

    Connections {
        target: voiceApi
        onVoiceStatusChanged: {
            if (voiceApi.voiceStatus === Common.VoiceStatusIdle)
                textAction.text = ""
            else if (voiceApi.voiceStatus === Common.VoiceStatusFailure)
                textAction.text = qsTr("I don't understand what you said.")
            else if (voiceApi.voiceStatus === Common.VoiceStatusRecording)
                textAction.text = qsTr("Listening")
            else if (voiceApi.voiceStatus === Common.VoiceStatusSending)
                textAction.text = qsTr("Sending")
            else if (voiceApi.voiceStatus === Common.VoiceStatusNotRecognized)
                textAction.text = qsTr("I don't recognize your order")
        }
    }
}
