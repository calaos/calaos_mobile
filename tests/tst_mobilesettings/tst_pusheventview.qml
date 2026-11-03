import QtQuick
import QtTest
import Calaos
import QuickFlux
import SharedComponents
import "../../qml/mobile"

//The picture of an event opened from a push: evPicUid reaches the view
//before evHasPicture, and the picture still has to be asked for, once.
TestCase {
    id: tc
    name: "PushEventView"
    when: windowShown
    visible: true
    width: 360
    height: 640

    property int pictureCalls: 0

    QtObject {
        id: calaosApp
        function getPictureSized(pic) { return "qrc:/img/" + pic + ".png" }
    }

    Component {
        id: pictureComponent
        QtObject {
            property string url: ""
            property bool loading: false
            property bool failed: false
            function release() {}
            function retry() {}
        }
    }

    Component {
        id: modelComponent
        QtObject {
            property string evNotifText: ""
            property string evPicUid: ""
            property bool evHasPicture: false
            property bool loading: false
        }
    }

    QtObject {
        id: eventLogModel
        property var lastModel: null
        property var requested: []
        function loadEvent(uuid) {
            lastModel = modelComponent.createObject(tc)
            return lastModel
        }
        function loadPicture(uid) {
            tc.pictureCalls++
            requested.push(uid)
            return pictureComponent.createObject(tc)
        }
    }

    Component {
        id: viewComponent
        PushEventView { anchors.fill: parent }
    }

    function init() {
        pictureCalls = 0
        eventLogModel.requested = []
    }

    function test_picuid_before_haspicture() {
        var view = createTemporaryObject(viewComponent, tc)
        verify(view)
        view.event_uuid = "u1"
        var m = eventLogModel.lastModel
        m.evPicUid = "pic-1"
        compare(pictureCalls, 0)
        m.evHasPicture = true
        compare(pictureCalls, 1)
        compare(eventLogModel.requested[0], "pic-1")
        verify(view.picture !== null)
    }

    function test_haspicture_before_picuid() {
        var view = createTemporaryObject(viewComponent, tc)
        view.event_uuid = "u2"
        var m = eventLogModel.lastModel
        m.evHasPicture = true
        compare(pictureCalls, 0)
        m.evPicUid = "pic-2"
        compare(pictureCalls, 1)
        compare(eventLogModel.requested[0], "pic-2")
    }

    function test_no_picture_stays_closed() {
        var view = createTemporaryObject(viewComponent, tc)
        view.event_uuid = "u3"
        var m = eventLogModel.lastModel
        m.evPicUid = "pic-3"
        compare(pictureCalls, 0)
        compare(view.picture, null)
    }

    //Opened from the event list: main.qml declares event_picuid in the
    //Component, so the view is created with the value already set.
    Component {
        id: listViewComponent
        PushEventView { anchors.fill: parent; event_picuid: "list-1" }
    }

    function test_list_path_picuid_set_directly() {
        var view = createTemporaryObject(listViewComponent, tc)
        verify(view)
        compare(pictureCalls, 1)
        compare(eventLogModel.requested[0], "list-1")
        verify(view.picture !== null)
    }

    function test_list_path_without_picture_stays_closed() {
        var view = createTemporaryObject(viewComponent, tc)
        view.event_haspic = false
        view.event_picuid = "list-2"
        compare(pictureCalls, 0)
        compare(view.picture, null)
    }
}
