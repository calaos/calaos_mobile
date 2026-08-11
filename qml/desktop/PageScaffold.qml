import QtQuick
import SharedComponents
import QtQuick.Layouts

//Chrome of a desktop full screen view: the module_header/module_footer bands,
//their two shadows and the right aligned row of FooterButton. The page content
//is declared inline and anchors against `header`/`footer` like it used to
//against the ids of the same name:
//
//    PageScaffold {
//        id: page
//        title: qsTr("Scenarios")
//        footerButtons: FooterButton { label: qsTr("Quit"); ... }
//
//        BorderImage { anchors { top: page.header.bottom; bottom: page.footer.top } }
//    }
//
//Content is added to the scaffold itself, so it is a sibling of the header and
//the footer (anchors only reach parents and siblings) and it stacks above the
//shadows. The two bands carry z: 1 to stay on top of it, which is where
//declaring them last used to put them.
Item {
    id: scaffold

    //title centered in the header band
    property alias title: headerText.text

    //buttons appended after the spacer, ie. laid out from the right edge
    property alias footerButtons: footerRow.data

    //anchor lines for the content, see the note above
    property alias header: headerImage
    property alias footer: footerImage

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: headerImage.bottom
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footerImage.top
            left: footerImage.left
            right: footerImage.right
        }
        opacity: 0.6
    }

    Image {
        id: headerImage
        z: 1
        source: "qrc:/img/module_header.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Text {
            id: headerText
            anchors.centerIn: parent
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(22)
            font.family: calaosFont.fontFamilyLight
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
        }
    }

    Image {
        id: footerImage
        z: 1
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            id: footerRow

            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }
        }
    }
}
