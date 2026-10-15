import "."

//Animated outlet icon, no spinning: only the pump spins.
IOBinaryDevice {
    animatedIcon: true
    iconOn: "icon_outlet_00%1"
    iconOff: "icon_outlet_off"

    //off first then on: same deactivate to activate order as the buttons
    buttonIconOff: "qrc:/img/ic_outlet_off.svg"
    buttonIconOn: "qrc:/img/ic_outlet_on.svg"
}
