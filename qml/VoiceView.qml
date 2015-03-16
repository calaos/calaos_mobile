import QtQuick 2.2

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
        color: "#e7e7e7"
        font { bold: false; pointSize: 12 }
        anchors.centerIn: listViewFav
        text: qsTr("Listening...")
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
}
