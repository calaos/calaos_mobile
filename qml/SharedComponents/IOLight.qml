import "."

//Animated light bulb, and two buttons that carry their own artwork.
IOBinaryDevice {
    animatedIcon: true
    iconOn: "icon_light_00%1"
    iconOff: "icon_light_off"

    //off first then on: same deactivate to activate order as the buttons
    buttonImageOff: "button_light_off"
    buttonImageOn: "button_light_on"
}
