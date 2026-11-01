import QtQuick
import QtQuick.Controls
import QtTest
import Calaos
import SharedComponents
import "../../qml/mobile"

//The login page: the password is typed in the QML field, in place, and a
//copied one can be pasted.
TestCase {
    id: tc
    name: "LoginView"
    when: windowShown
    visible: true
    width: 420
    height: 900

    property bool isLandscape: false

    QtObject {
        id: calaosApp
        property bool isAndroid: false
        property string username: "user"
        property string password: ""
        property string hostname: "box"
        property bool loginEdited: false
        property int applicationStatus: Common.NotConnected
        function getPictureSized(pic) { return "" }
    }

    Component {
        id: loginComponent
        LoginView { anchors.fill: parent }
    }

    TextInput { id: clipboardSource; visible: false }

    function fieldOf(view, alias) {
        for (var i = 0; i < view.children.length; i++) {
            var found = findIn(view.children[i], alias)
            if (found)
                return found
        }
        return null
    }

    function findIn(item, alias) {
        if (item.placeholderText === alias)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var found = findIn(item.children[i], alias)
            if (found)
                return found
        }
        return null
    }

    function test_passwordTypedInPlace() {
        var view = createTemporaryObject(loginComponent, tc)
        var field = fieldOf(view, "Password")
        verify(field)
        compare(field.echoMode, TextInput.Password)
        verify(field.inputMethodHints & Qt.ImhSensitiveData)
        verify(field.inputMethodHints & Qt.ImhNoPredictiveText)
        verify(field.inputMethodHints & Qt.ImhNoAutoUppercase)

        mouseClick(field)
        tryCompare(field, "activeFocus", true)

        clipboardSource.text = "copied secret"
        clipboardSource.selectAll()
        clipboardSource.copy()
        field.paste()
        compare(view.password, "copied secret")
    }

    // Read by Application when iOS resigns active: a typed or pasted field
    // must not be replaced by the automatic login on return.
    function test_editedFlagFollowsTheUser() {
        calaosApp.loginEdited = false
        var view = createTemporaryObject(loginComponent, tc)
        compare(calaosApp.loginEdited, false)

        var host = fieldOf(view, "Hostname")
        mouseClick(host)
        tryCompare(host, "activeFocus", true)
        keyClick("x")
        compare(calaosApp.loginEdited, true)

        view.syncFromApp()
        compare(calaosApp.loginEdited, false)

        var field = fieldOf(view, "Password")
        mouseClick(field)
        tryCompare(field, "activeFocus", true)
        clipboardSource.text = "copied secret"
        clipboardSource.selectAll()
        clipboardSource.copy()
        field.paste()
        compare(calaosApp.loginEdited, true)
    }
}
