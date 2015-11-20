import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls.Private 1.0

AbstractStyle {
    readonly property TextField control: __control

    padding {
        top: 4
        left: Math.round(control.__contentHeight / 3)
        right: control.__contentHeight / 3
        bottom: 4
    }


}

