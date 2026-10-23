import QtQuick
import QtQuick.Layouts
import SharedComponents

//The server certificate cannot be verified (self-signed box, or a certificate
//for another name): the user checks the fingerprint and decides. Shown for a
//first contact and, with a stronger wording, when the key changed since the
//last time it was trusted. Closing the sheet any other way than "Trust" is a
//refusal, so the question is answered exactly once.
Item {
    id: host

    property string fontFamily: calaosFont.fontFamily
    property var info: ({})
    readonly property bool keyChanged: info.keyChanged === true
    //Reached by DNS name, or a valid certificate for another name: what an
    //interception would look like, so nothing here reassures.
    readonly property bool suspicious: info.suspicious === true
    //Trusting a changed or suspicious certificate takes a second tap.
    readonly property bool needsConfirm: keyChanged || suspicious
    property bool trustArmed: false
    readonly property bool opened: sheet.opened

    //Set once the question got its answer, so onClosed does not refuse it
    //a second time.
    property bool answered: true

    width: 0
    height: 0

    component Body: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.whiteColor
        font { family: host.fontFamily; pixelSize: Units.dp(14) }
    }

    component Label: Text {
        Layout.fillWidth: true
        Layout.topMargin: Units.dp(4)
        wrapMode: Text.Wrap
        color: Theme.blueColor
        font { family: host.fontFamily; bold: true; pixelSize: Units.dp(13) }
    }

    component Fingerprint: Text {
        Layout.fillWidth: true
        wrapMode: Text.WrapAnywhere
        color: Theme.whiteColor
        font { family: host.fontFamily; pixelSize: Units.dp(13) }
    }

    //95 characters do not fit a phone line: two lines of 16 bytes.
    function splitFingerprint(fp) {
        if (!fp || fp.length < 48)
            return fp || ""
        return fp.substring(0, 47) + "\n" + fp.substring(48)
    }

    function answer(accept) {
        if (answered)
            return
        answered = true
        calaosApp.answerTlsTrust(accept)
        sheet.close()
    }

    Connections {
        target: calaosApp
        function onTlsTrustRequested(i) {
            host.info = i
            host.answered = false
            host.trustArmed = false
            sheet.open()
        }
        function onTlsTrustCanceled() {
            host.answered = true
            sheet.close()
        }
    }

    BottomSheet {
        id: sheet
        objectName: "tlsTrustSheet"
        closeOnLogout: false
        fontFamily: host.fontFamily
        title: host.keyChanged ? qsTr("Certificate changed!") : qsTr("Unknown certificate")
        fitHeight: column.implicitHeight
        onClosed: host.answer(false)

        SheetScroll {
            anchors.fill: parent
            implicitContentHeight: column.implicitHeight

            ColumnLayout {
                id: column
                width: parent.width
                spacing: Units.dp(8)

                Body {
                    objectName: "tlsTrustText"
                    color: host.needsConfirm ? Theme.amberColor : Theme.whiteColor
                    text: host.keyChanged ?
                              qsTr("The certificate key of %1 has changed since the last connection. "
                                   + "This is expected if the certificate of your Calaos box was "
                                   + "regenerated, but it can also mean that someone is intercepting "
                                   + "the connection. Only trust it if you know why it changed.").arg(host.info.host) :
                          host.suspicious ?
                              qsTr("The certificate of %1 cannot be verified. A server reached by its "
                                   + "domain name, or presenting a certificate issued for another name, "
                                   + "normally has a valid certificate: someone may be intercepting the "
                                   + "connection. Only trust it if you know this server uses its own "
                                   + "certificate.").arg(host.info.host) :
                              qsTr("The certificate of %1 cannot be verified. This is usual for a Calaos "
                                   + "box on a home network. Check that the fingerprint below is the one "
                                   + "of your box before trusting it.").arg(host.info.host)
                }

                Label { text: qsTr("Key fingerprint (SHA-256)") }
                Fingerprint { text: host.splitFingerprint(host.info.fingerprint) }

                Label {
                    visible: host.keyChanged
                    text: qsTr("Previously trusted key")
                }
                Fingerprint {
                    visible: host.keyChanged
                    color: Theme.mutedColor
                    text: host.splitFingerprint(host.info.previousFingerprint)
                }

                Label { text: qsTr("Certificate fingerprint (SHA-256)") }
                Fingerprint {
                    color: Theme.mutedColor
                    text: host.splitFingerprint(host.info.certFingerprint)
                }

                Body {
                    Layout.topMargin: Units.dp(4)
                    color: Theme.mutedColor
                    text: qsTr("Issued to: %1").arg(host.info.subject || "-") + "\n" +
                          qsTr("Issued by: %1").arg(host.info.issuer || "-") + "\n" +
                          qsTr("Expires: %1").arg(host.info.expiry || "-")
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(8)
                    spacing: Units.dp(12)

                    CalaosItemBase {
                        objectName: "tlsRefuseButton"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Units.dp(40)
                        text: qsTr("Do not connect")
                        onButtonClicked: host.answer(false)
                    }

                    CalaosItemBase {
                        objectName: "tlsTrustButton"
                        Layout.fillWidth: true
                        Layout.preferredHeight: Units.dp(40)
                        //De-emphasised when the certificate is suspicious:
                        //"Do not connect" is the obvious choice.
                        opacity: host.needsConfirm && !host.trustArmed ? 0.5 : 1
                        text: host.trustArmed ? qsTr("Tap again to trust") : qsTr("Trust")
                        onButtonClicked: {
                            if (host.needsConfirm && !host.trustArmed)
                                host.trustArmed = true
                            else
                                host.answer(true)
                        }
                    }
                }
            }
        }
    }
}
