import "."

//Static boiler icon swapped on state change.
IOBinaryDevice {
    iconOn: "icon_boiler_on"
    iconOff: "icon_boiler_off"

    //off first then on: same deactivate to activate order as the buttons
    buttonIconOff: "qrc:/img/ic_outlet_off.svg"
    buttonIconOn: "qrc:/img/ic_outlet_on.svg"
}
