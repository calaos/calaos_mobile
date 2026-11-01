import QtQuick
import QtQuick.Controls as QuickControls
import "../quickflux"

//Entry of the settings. Box and Event log lead straight to their own page,
//a sub-page there would only hold one button.
SettingsPage {
    id: settingsPage

    title: qsTr("Settings")

    signal favoriteAddClicked()
    signal favoriteEditClicked()
    signal boxUpdatesClicked()
    signal subPageOpened()

    function openSubPage(component) {
        subPageOpened()
        QuickControls.StackView.view.push(component)
    }

    SettingsEntry {
        objectName: "settingsEntryDevice"
        title: qsTr("Device")
        onButtonClicked: settingsPage.openSubPage(deviceView)
    }
    SettingsEntry {
        objectName: "settingsEntryConnection"
        title: qsTr("Connection")
        onButtonClicked: settingsPage.openSubPage(connectionView)
    }
    SettingsEntry {
        objectName: "settingsEntryBox"
        title: qsTr("Box")
        onButtonClicked: settingsPage.boxUpdatesClicked()
    }
    SettingsEntry {
        objectName: "settingsEntryFavorites"
        title: qsTr("Favorites")
        onButtonClicked: settingsPage.openSubPage(favoritesView)
    }
    SettingsEntry {
        objectName: "settingsEntryEventLog"
        title: qsTr("Event log")
        onButtonClicked: AppActions.openEventLog()
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
