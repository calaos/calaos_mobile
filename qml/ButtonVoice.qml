import QtQuick 2.3
import Calaos 1.0

Item {

    signal buttonClicked()

    width: ico.implicitWidth
    height: ico.implicitHeight

    Rectangle {
        color: voiceApi.voiceStatus === Common.VoiceStatusFailure?Qt.rgba(235, 84, 84, 1.0):
               voiceApi.voiceStatus === Common.VoiceStatusRecording?"#3ab4d7":"010101"
        Behavior on color { ColorAnimation { duration: 100 } }
        anchors.centerIn: parent
        width: calaosApp.dp(47)
        height: calaosApp.dp(53)
    }

    Image {
        id: ico
        source: calaosApp.getPictureSized("button_voice")
        anchors.centerIn: parent
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            console.log("button voice click: " + voiceApi.voiceStatus)
            if (voiceApi.voiceStatus === Common.VoiceStatusIdle ||
                voiceApi.voiceStatus === Common.VoiceStatusFailure)
                voiceApi.startVoiceRecord()
            else if (voiceApi.voiceStatus === Common.VoiceStatusRecording)
                voiceApi.stopRecording()
        }
    }
}

