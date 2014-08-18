import QtQuick 2.0

Item {

    anchors.fill: parent

    ViewHeader {
        headerLabel: qsTr("Favorites")
        iconSource: calaosApp.getPictureSized("fav")
    }
}
