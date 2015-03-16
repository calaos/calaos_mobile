import QtQuick 2.2
import Calaos 1.0

Item {
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
        }
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
                textAction.text = qsTr("Failed!")
            else if (voiceApi.voiceStatus === Common.VoiceStatusRecording)
                textAction.text = qsTr("Listening")
            else if (voiceApi.voiceStatus === Common.VoiceStatusSending)
                textAction.text = qsTr("Sending")
        }
    }
}
