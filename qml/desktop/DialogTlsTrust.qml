import QtQuick
import SharedComponents

//The server certificate cannot be verified (self-signed box, or a certificate
//for another name): the user checks the fingerprint and decides. Shown for a
//first contact and, with a stronger wording, when the key changed since the
//last time it was trusted.
Dialog {
    id: dlg

    property var info: ({})
    readonly property bool keyChanged: info.keyChanged === true
    //Pinned host now behind a CA-valid certificate: trusting it stops
    //pinning the host instead of pinning the new key.
    readonly property bool caValid: info.caValid === true
    //Reached by DNS name, or a valid certificate for another name: what an
    //interception would look like, so nothing here reassures.
    readonly property bool suspicious: info.suspicious === true

    //A tap outside must not count as an answer: the connection waits for one.
    dismissOnTap: false
    minimumWidth: Units.dp(560)

    title: keyChanged ? qsTr("Certificate changed!") : qsTr("Unknown certificate")
    text: keyChanged && caValid ?
              qsTr("The certificate key of %1 has changed since the last connection. "
                   + "The box now presents a certificate from a public authority: if you "
                   + "trust it, this host will no longer be pinned and its renewals will be "
                   + "accepted without asking. It can also mean that someone is intercepting "
                   + "the connection. Only trust it if you know why it changed.").arg(info.host) :
          keyChanged ?
              qsTr("The certificate key of %1 has changed since the last connection. "
                   + "This is expected if the certificate of your Calaos box was "
                   + "regenerated, but it can also mean that someone is intercepting "
                   + "the connection. Only trust it if you know why it changed.").arg(info.host) :
          suspicious ?
              qsTr("The certificate of %1 cannot be verified. A server reached by its "
                   + "domain name, or presenting a certificate issued for another name, "
                   + "normally has a valid certificate: someone may be intercepting the "
                   + "connection. Only trust it if you know this server uses its own "
                   + "certificate.").arg(info.host) :
              qsTr("The certificate of %1 cannot be verified. This is usual for a Calaos "
                   + "box on a home network. Check that the fingerprint below is the one "
                   + "of your box before trusting it.").arg(info.host)
    negativeButtonText: qsTr("Do not connect")
    positiveButtonText: qsTr("Trust")

    onAccepted: calaosApp.answerTlsTrust(true)
    onRejected: calaosApp.answerTlsTrust(false)

    Connections {
        target: calaosApp
        function onTlsTrustRequested(i) {
            dlg.info = i
            dlg.show()
        }
        function onTlsTrustCanceled() {
            dlg.close()
        }
    }

    component Label: Text {
        width: parent.width
        wrapMode: Text.Wrap
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(13); bold: true }
        color: "#3AB4D7"
    }

    component Value: Text {
        width: parent.width
        wrapMode: Text.WrapAnywhere
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(13); weight: Font.Thin }
        color: "#E7E7E7"
    }

    Label { text: qsTr("Key fingerprint (SHA-256)") }
    Value { text: dlg.info.fingerprint || "" }

    Label {
        visible: dlg.keyChanged
        text: qsTr("Previously trusted key")
    }
    Value {
        visible: dlg.keyChanged
        color: "#868482"
        text: dlg.info.previousFingerprint || ""
    }

    Label { text: qsTr("Certificate fingerprint (SHA-256)") }
    Value {
        color: "#868482"
        text: dlg.info.certFingerprint || ""
    }

    Value {
        wrapMode: Text.Wrap
        color: "#868482"
        text: qsTr("Issued to: %1").arg(dlg.info.subject || "-") + "\n" +
              qsTr("Issued by: %1").arg(dlg.info.issuer || "-") + "\n" +
              qsTr("Expires: %1").arg(dlg.info.expiry || "-")
    }
}
