import QtQuick
import SharedComponents
import QtQuick.Layouts
import QtWebEngine
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings
import QuickFlux
import "../quickflux"

Item {
    id: root
    property bool hideMainMenu: true

    // ── Tab state ──────────────────────────────────────────────────
    property int activeTabIndex: -1
    property var tabWebViews: []   // parallel JS array of WebEngineView refs
    readonly property int maxTabs: 10

    ListModel {
        id: tabListModel
        // Each element: { tabId: int, title: string, url: string }
    }

    property int _nextTabId: 0

    // ── Convenience: active WebEngineView ──────────────────────────
    readonly property var activeWebView: (activeTabIndex >= 0 && activeTabIndex < tabWebViews.length)
                                         ? tabWebViews[activeTabIndex] : null

    // ── Header / footer shadows ────────────────────────────────────
    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footer.top
            left: footer.left
            right: footer.right
        }
        opacity: 0.6
    }

    // ── WebEngineView component (created dynamically per tab) ──────
    Component {
        id: webViewComponent

        WebEngineView {
            anchors.fill: parent
            visible: false
            profile: webEngineProfile

            // Keep model in sync
            onTitleChanged: root.updateTabModel(this)
            onUrlChanged:   root.updateTabModel(this)
        }
    }

    // ── Container for all WebEngineView instances ──────────────────
    Item {
        id: webViewContainer
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: inputPanel.top
        }

        // SpeedDial overlay (shown when active tab is on about:blank)
        SpeedDialPage {
            id: speedDialPage
            z: 1
            visible: root.activeWebView !== null &&
                     root.activeWebView.url.toString() === "about:blank"
            onBookmarkClicked: (url) => {
                if (root.activeWebView)
                    root.activeWebView.url = url
            }
        }
    }

    // ── Progress bar ───────────────────────────────────────────────
    Rectangle {
        color: "#3AB4D7"
        height: 2
        anchors {
            left: parent.left
            bottom: webViewContainer.bottom
        }
        width: root.activeWebView ? parent.width * root.activeWebView.loadProgress / 100 : 0
        visible: root.activeWebView ? root.activeWebView.loading : false
        opacity: 0.7
    }

    // ── QuickFlux: webGoToUrl navigates active tab ─────────────────
    AppListener {
        Filter {
            type: ActionTypes.webGoToUrl
            onDispatched: (filtertype, message) => {
                if (!root.activeWebView) return
                var text = message.text
                if (!text.startsWith("http://") && !text.startsWith("https://"))
                    text = "https://google.com/search?q=" + text
                root.activeWebView.url = text
            }
        }
    }

    // ── Virtual keyboard ───────────────────────────────────────────
    Rectangle {
        color: "black"
        anchors {
            left: parent.left; right: parent.right
            top: inputPanel.top; bottom: inputPanel.bottom
        }
    }

    InputPanel {
        id: inputPanel
        anchors {
            bottom: footer.top; bottomMargin: 0
            horizontalCenter: parent.horizontalCenter
        }
        state: "hidden"

        width: parent.width * 0.75

        states: [
            State {
                name: "visible"
                PropertyChanges { target: inputPanel; anchors.bottomMargin: 0 }
            },
            State {
                name: "hidden"
                PropertyChanges { target: inputPanel; anchors.bottomMargin: -inputPanel.implicitHeight }
            }
        ]

        transitions: [
            Transition {
                from: "visible"
                to: "hidden"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "hidden"
                to: "visible"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            }
        ]

        // Eat mouse events when hidden to prevent accidental key presses
        MouseArea {
            anchors.fill: parent
            visible: inputPanel.state == "hidden"
        }
    }

    // ── Footer bar ─────────────────────────────────────────────────
    Image {
        id: footer
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            RoundButton {
                next: false
                disabled: root.activeWebView ? !root.activeWebView.canGoBack : true
                scale: 0.8
                onButtonClicked: {
                    if (root.activeWebView) root.activeWebView.goBack()
                }
            }

            RoundButton {
                next: true
                disabled: root.activeWebView ? !root.activeWebView.canGoForward : true
                scale: 0.8
                onButtonClicked: {
                    if (root.activeWebView) root.activeWebView.goForward()
                }
            }

            Item { // spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: ""
                icon: "qrc:/img/button_action_reload.png"
                Layout.minimumWidth: width
                onBtClicked: {
                    if (root.activeWebView) root.activeWebView.reload()
                }
            }

            FooterButton {
                label: qsTr("Menu")
                icon: "qrc:/img/icon_more.png"
                Layout.minimumWidth: width
                onBtClicked: {
                    browserMenu.tabCount = tabListModel.count
                    browserMenu.visible = !browserMenu.visible
                }
            }

            FooterButton {
                label: qsTr("Keyboard")
                icon: "qrc:/img/button_action_clavier.png"
                Layout.minimumWidth: width
                onBtClicked: inputPanel.state === "visible" ? inputPanel.state = "hidden" : inputPanel.state = "visible"
            }

            FooterButton {
                label: qsTr("Back to media")
                icon: "qrc:/img/button_action_back.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.handleBack()
            }

            FooterButton {
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }
    }

    // ── Menu overlay ───────────────────────────────────────────────
    WebBrowserMenu {
        id: browserMenu
        anchors.fill: parent
        visible: false
        z: 10

        onGoToUrl: {
            AppActions.openKeyboard(qsTr("URL"),
                                    qsTr("Enter the url to navigate to"),
                                    "",
                                    TextInput.Normal,
                                    false,
                                    ActionTypes.webGoToUrl)
        }

        onOpenTabs: {
            dialogTabList.activeTabIndex = root.activeTabIndex
            dialogTabList.show()
        }

        onOpenBookmarks: {
            dialogBookmarks.show()
        }

        onBookmarkCurrentPage: {
            if (root.activeWebView &&
                root.activeWebView.url.toString() !== "about:blank") {
                bookmarkModel.addBookmark(root.activeWebView.title,
                                          root.activeWebView.url.toString())
                AppActions.showNotificationMsg(qsTr("Bookmark added"),
                                               root.activeWebView.title,
                                               "", 3000)
            }
        }
    }

    // ── Tab list dialog ────────────────────────────────────────────
    DialogTabList {
        id: dialogTabList
        tabModel: tabListModel
        maxTabs: root.maxTabs

        onTabSelected: (index) => {
            root.switchToTab(index)
        }

        onTabClosed: (index) => {
            root.closeTab(index)
            // Update the dialog's active highlight after close
            dialogTabList.activeTabIndex = root.activeTabIndex
        }

        onNewTabRequested: {
            root.createTab()
            dialogTabList.activeTabIndex = root.activeTabIndex
        }
    }

    // ── Bookmarks dialog ───────────────────────────────────────────
    DialogBookmarks {
        id: dialogBookmarks

        onBookmarkSelected: (url) => {
            if (root.activeWebView)
                root.activeWebView.url = url
        }

        onAddBookmarkRequested: {
            // Open keyboard to type a URL to bookmark
            // We reuse the webGoToUrl action to add it as bookmark after navigation
            if (root.activeWebView &&
                root.activeWebView.url.toString() !== "about:blank") {
                bookmarkModel.addBookmark(root.activeWebView.title,
                                          root.activeWebView.url.toString())
                AppActions.showNotificationMsg(qsTr("Bookmark added"),
                                               root.activeWebView.title,
                                               "", 3000)
            } else {
                AppActions.showNotificationMsg(qsTr("Cannot bookmark"),
                                               qsTr("Navigate to a page first"),
                                               "", 3000)
            }
        }
    }

    // ── Tab management functions ───────────────────────────────────

    function createTab() {
        if (tabListModel.count >= maxTabs) return

        var tabId = _nextTabId++
        var wv = webViewComponent.createObject(webViewContainer, {})
        wv.url = "about:blank"

        // Tag the WebEngineView with its tabId for model sync
        wv.objectName = "tab_" + tabId

        tabWebViews.push(wv)
        tabListModel.append({ "tabId": tabId, "title": "", "url": "about:blank" })

        switchToTab(tabListModel.count - 1)
    }

    function switchToTab(index) {
        if (index < 0 || index >= tabListModel.count) return

        // Hide current active tab
        if (activeTabIndex >= 0 && activeTabIndex < tabWebViews.length) {
            tabWebViews[activeTabIndex].visible = false
        }

        activeTabIndex = index

        // Show new active tab
        tabWebViews[activeTabIndex].visible = true
        tabWebViews[activeTabIndex].forceActiveFocus()
    }

    function closeTab(index) {
        if (index < 0 || index >= tabListModel.count) return
        if (tabListModel.count <= 1) return // never close the last tab

        var wv = tabWebViews[index]

        // Remove from JS array
        tabWebViews.splice(index, 1)

        // Remove from ListModel
        tabListModel.remove(index)

        // Destroy the WebEngineView
        wv.destroy()

        // Adjust active index
        if (activeTabIndex >= tabListModel.count) {
            activeTabIndex = tabListModel.count - 1
        } else if (index < activeTabIndex) {
            activeTabIndex--
        } else if (index === activeTabIndex) {
            // If we closed the active tab, clamp and show the new one at same index
            if (activeTabIndex >= tabListModel.count)
                activeTabIndex = tabListModel.count - 1
        }

        // Ensure one tab is visible
        if (activeTabIndex >= 0 && activeTabIndex < tabWebViews.length) {
            tabWebViews[activeTabIndex].visible = true
            tabWebViews[activeTabIndex].forceActiveFocus()
        }
    }

    function updateTabModel(wv) {
        for (var i = 0; i < tabWebViews.length; i++) {
            if (tabWebViews[i] === wv) {
                tabListModel.set(i, {
                    "title": wv.title || "",
                    "url":   wv.url.toString()
                })
                return
            }
        }
    }

    // ── Create first tab on completion ─────────────────────────────
    Component.onCompleted: {
        createTab()
    }
}
