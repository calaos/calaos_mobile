import QtQuick
import QtQuick.Controls as QuickControls
import QtQuick.Layouts
import SharedComponents
import "../quickflux"

//Entry of the settings. Box and Event log lead straight to their own page,
//a sub-page there would only hold one button.
SettingsPage {
    id: settingsPage

    title: qsTr("Settings")
    glyph: "tune"

    signal favoriteAddClicked()
    signal favoriteEditClicked()
    signal boxUpdatesClicked()
    signal subPageOpened()

    function openSubPage(component) {
        subPageOpened()
        QuickControls.StackView.view.push(component)
    }

    component TileGrid: GridLayout {
        x: Units.dp(16)
        width: settingsPage.width - Units.dp(32)
        columns: 2
        columnSpacing: Units.dp(12)
        rowSpacing: Units.dp(12)
    }

    //The tiles of a row share the height of the one that wraps its label
    component Tile: SettingsEntry {
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    ListGroupHeader { title: qsTr("This phone") }

    TileGrid {
        Tile {
            objectName: "settingsEntryDevice"
            title: qsTr("Device")
            glyph: "phone"
            onButtonClicked: settingsPage.openSubPage(deviceView)
        }
        Tile {
            objectName: "settingsEntryConnection"
            title: qsTr("Connection")
            glyph: "link"
            onButtonClicked: settingsPage.openSubPage(connectionView)
        }
        Tile {
            objectName: "settingsEntryFavorites"
            title: qsTr("Favorites")
            glyph: "star"
            wide: true
            Layout.columnSpan: 2
            onButtonClicked: settingsPage.openSubPage(favoritesView)
        }
    }

    ListGroupHeader { gap: Units.dp(16); title: qsTr("Box") }

    TileGrid {
        Tile {
            objectName: "settingsEntryBox"
            title: qsTr("Box")
            glyph: "dns"
            external: true
            onButtonClicked: settingsPage.boxUpdatesClicked()
        }
        Tile {
            objectName: "settingsEntryEventLog"
            title: qsTr("Event log")
            glyph: "history"
            external: true
            onButtonClicked: AppActions.openEventLog()
        }
    }

    Component {
        id: deviceView

        SettingsDeviceView {
        }
    }

    Component {
        id: connectionView

        SettingsConnectionView {
        }
    }

    Component {
        id: favoritesView

        SettingsFavoritesView {
            onFavoriteAddClicked: settingsPage.favoriteAddClicked()
            onFavoriteEditClicked: settingsPage.favoriteEditClicked()
        }
    }
}
