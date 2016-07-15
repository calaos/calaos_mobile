import QtQuick 2.5
import SharedComponents 1.0

Image {
    property bool next: true

    source: next?"qrc:/img/button_round_next.png":
                 "qrc:/img/button_round_previous.png"
}
