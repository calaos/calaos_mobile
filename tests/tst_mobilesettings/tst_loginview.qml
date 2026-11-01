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
}
