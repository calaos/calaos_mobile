import QtQuick
import SharedComponents

SvgGlyph {
    id: spinner

    name: "spinner"
    size: Units.dp(32)

    RotationAnimator on rotation {
        running: spinner.visible && !Theme.reduceMotion
        loops: Animation.Infinite
        duration: 1000
        from: 0
        to: 360
    }
}
