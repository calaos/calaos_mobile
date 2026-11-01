import QtQuick

SettingsPage {
    id: favoritesPage

    title: qsTr("Favorites")
    glyph: "star"

    signal favoriteAddClicked()
    signal favoriteEditClicked()

    SettingsRows {
        SettingsRow {
            objectName: "favoriteAddButton"
            title: qsTr("Add a favorite")
            glyph: "add"
            chevron: true
            onButtonClicked: favoritesPage.favoriteAddClicked()
        }
        SettingsRow {
            objectName: "favoriteEditButton"
            title: qsTr("Edit favorites list")
            glyph: "edit"
            chevron: true
            onButtonClicked: favoritesPage.favoriteEditClicked()
        }
    }
}
