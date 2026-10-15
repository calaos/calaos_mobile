pragma Singleton
import QtQuick

//Records what the shared components dispatch, so that a test can read it back
QtObject {
    property var log: []

    function dispatch(type, message) {
        log.push({ type: type, message: message })
    }

    function clear() {
        log = []
    }
}
