import QtQuick
import SharedComponents

Dialog {
    id: dlg

    property int count
    property int calaosCount
    property string summary

    function showDialog(msg) {
        count = msg.hasOwnProperty("count") ? msg.count : 0
        calaosCount = msg.hasOwnProperty("calaosCount") ? msg.calaosCount : 0
        summary = msg.hasOwnProperty("summary") ? msg.summary : ""
        show()
    }

    title: qsTr("Software update available")
    text: {
        var t = qsTr("%n update(s) are available for your system.", "", count)
        if (summary !== "")
            t += "\n\n" + qsTr("Calaos components: %1").arg(summary)
        return t
    }
    hasActions: true
    positiveButtonText: qsTr("View updates")
    negativeButtonText: qsTr("Later")

    //the accept action (navigate to the update page) is handled at the
    //instantiation site in main.qml, where the navigation stack lives
}
