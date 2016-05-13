import QtQuick 2.0

pragma Singleton

QtObject {
    id: units

    property real pixelDensity: 96.0
    property real defaultDpi: 96.0
    property real cachedValue: pixelDensity / defaultDpi

    function inst() {
        return units;
    }

    function dp(number) {
        return Math.round(number * cachedValue);
    }
}
