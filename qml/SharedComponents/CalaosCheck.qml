import QtQuick 2.4
import QtQuick.Controls 1.4 as Controls
import "."

Controls.CheckBox {
    id: slider

    implicitHeight: 40 * calaosApp.density
    implicitWidth: 200 * calaosApp.density

    style: CalaosCheckStyle { }
}
