import QtQuick
import SharedComponents

SettingsPage {
    id: favoritesPage

    title: qsTr("Favorites")

    signal favoriteAddClicked()
    signal favoriteEditClicked()

    Item {
        width: favoritesPage.width
        height: Units.dp(40)
        CalaosItemBase {
            objectName: "favoriteAddButton"
            text: qsTr("Add a favorite")
            width: Units.dp(200)
            height: parent.height
            onButtonClicked: favoritesPage.favoriteAddClicked()
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Item {
        width: favoritesPage.width
        height: Units.dp(40)
        CalaosItemBase {
            objectName: "favoriteEditButton"
            text: qsTr("Edit favorites list")
            width: Units.dp(200)
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter
            onButtonClicked: favoritesPage.favoriteEditClicked()
        }
    }
}
