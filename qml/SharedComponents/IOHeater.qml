import "."

//Static heater icon swapped on state change.
IOBinaryDevice {
    iconOn: "icon_heater_on"
    iconOff: "icon_heater_off"

    //off first then on: same deactivate to activate order as the buttons
    buttonIconOff: "qrc:/img/ic_outlet_off.svg"
    buttonIconOn: "qrc:/img/ic_outlet_on.svg"
}
