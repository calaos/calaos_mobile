import QtQuick
import SharedComponents

//A line of the settings entry page, leading to a sub-page.
CalaosItemBase {
    id: entry

    property string title

    x: Units.dp(16)
    width: parent.width - Units.dp(32)
    height: Units.dp(56)

    Text {
        anchors {
            left: parent.left; leftMargin: Units.dp(16)
            right: chevron.left; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }
        elide: Text.ElideRight
        color: Theme.blueColor
        font { family: calaosFont.fontFamilyThin; pointSize: 12 }
        text: entry.title
    }

    Text {
        id: chevron
        anchors {
            right: parent.right; rightMargin: Units.dp(16)
            verticalCenter: parent.verticalCenter
        }
        color: Theme.blueColor
        font { family: calaosFont.fontFamilyThin; pointSize: 18 }
        text: "›"
    }
}
