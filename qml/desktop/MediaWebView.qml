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
    // Single source of truth: one row of tabListModel per tab, holding both the
    // metadata displayed by DialogTabList and the WebEngineView instance that
    // renders it. There is no parallel array to keep in sync, so a tab's view
    // and its title/url can no longer drift apart.
    property int activeTabIndex: -1

    // Owner of the tab limit. DialogTabList takes it from here (required
    // property) so the toolbar and the dialog can never disagree.
    readonly property int maxTabs: 10

    // Search engine used when the typed text is not already a URL. The query is
    // appended percent-encoded, so any prefix ending with "=" works.
    property string searchEngineUrl: "https://www.google.com/search?q="

    ListModel {
        id: tabListModel
        // Each element: { tabId: int, title: string, url: string, webView: WebEngineView }
    }

    property int _nextTabId: 0

    // ── Convenience: active WebEngineView ──────────────────────────
    // Depends only on activeTabIndex and tabListModel.count, both of which
    // notify, so the binding re-evaluates on every tab add/remove/switch.
    readonly property var activeWebView: (activeTabIndex >= 0 && activeTabIndex < tabListModel.count)
                                         ? tabListModel.get(activeTabIndex).webView : null

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
            visible: false   // replaced by a binding on activeWebView in createTab()
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
        color: Theme.blueColor
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
                var text = (message.text || "").trim()
                if (text.length === 0) return
                if (!text.startsWith("http://") && !text.startsWith("https://")) {
                    // Percent-encode the query: spaces, '&', '#', '+' and
                    // non-ASCII would otherwise break out of the query string.
                    text = root.searchEngineUrl + encodeURIComponent(text)
                }
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
                                    qsTr("Enter the URL to open"),
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
        if (tabListModel.count >= root.maxTabs) return

        var wv = webViewComponent.createObject(webViewContainer, {})
        if (!wv) {
            // Creation can fail (out of memory, broken profile, ...). Bail out
            // rather than appending a row that has no view behind it.
            console.warn("MediaWebView: WebEngineView creation failed, tab not created")
            return
        }

        var tabId = root._nextTabId++

        // Tag the WebEngineView for debugging/testing purposes only; the model
        // row below is what actually identifies the tab.
        wv.objectName = "tab_" + tabId
        wv.url = "about:blank"

        // Visibility is derived from the single source of truth rather than
        // toggled by hand on every switch/close, so exactly one view is visible
        // at any time. Installed here (and not as a binding inside the
        // Component) to avoid reaching for the outer `root` id from a nested
        // component.
        wv.visible = Qt.binding(function() { return root.activeWebView === wv })

        // Metadata and view enter the model in one single append: there is no
        // window during which the two could be out of sync.
        tabListModel.append({ "tabId": tabId,
                              "title": "",
                              "url": "about:blank",
                              "webView": wv })

        root.switchToTab(tabListModel.count - 1)
    }

    function switchToTab(index) {
        if (index < 0 || index >= tabListModel.count) return

        root.activeTabIndex = index

        var wv = tabListModel.get(index).webView
        if (wv)
            wv.forceActiveFocus()
    }

    function closeTab(index) {
        if (index < 0 || index >= tabListModel.count) return
        if (tabListModel.count <= 1) return // never close the last tab

        // Work out the tab that takes over *before* mutating the model, so no
        // index has to be patched up after the fact.
        var newIndex = root.activeTabIndex
        if (index < newIndex)
            newIndex--                                     // rows shifted down
        else if (index === newIndex && index === tabListModel.count - 1)
            newIndex--                                     // closed the last one

        var wv = tabListModel.get(index).webView

        // The view reference lives in the row being removed, so metadata and
        // view always disappear together.
        tabListModel.remove(index)

        // A WebEngineView holds a render process: release it explicitly rather
        // than waiting for the container to be torn down.
        if (wv)
            wv.destroy()

        root.switchToTab(Math.max(0, Math.min(newIndex, tabListModel.count - 1)))
    }

    function updateTabModel(wv) {
        for (var i = 0; i < tabListModel.count; i++) {
            if (tabListModel.get(i).webView === wv) {
                tabListModel.setProperty(i, "title", wv.title || "")
                tabListModel.setProperty(i, "url", wv.url.toString())
                return
            }
        }
    }

    // ── Create first tab on completion ─────────────────────────────
    Component.onCompleted: {
        createTab()
    }
}
