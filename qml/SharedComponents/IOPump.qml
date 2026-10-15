import "."

//Animated impeller spinning over the fixed body of the pump.
IOBinaryDevice {
    animatedIcon: true
    iconBackground: "icon_pump_bg"
    iconOn: "icon_pump_00%1"
    iconOff: "icon_pump_off"
    spinningIcon: true

    //off first then on: same deactivate to activate order as the buttons
    buttonIconOff: "qrc:/img/ic_outlet_off.svg"
    buttonIconOn: "qrc:/img/ic_outlet_on.svg"
}
