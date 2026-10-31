import QtQuick
import QtQuick.Layouts
import SharedComponents

//Date and time picked on wheels, in no time zone: the caller says which one.
RowLayout {
    id: wheels

    readonly property int year: yearWheel.value
    readonly property int month: monthWheel.value
    readonly property int day: dayWheel.value
    readonly property int hour: hourWheel.value
    readonly property int minute: minuteWheel.value
    readonly property int second: secondWheel.value

    //changes with any wheel
    readonly property string picked: [year, month, day, hour, minute, second].join("-")

    function setValues(values) {
        yearWheel.setValue(values.year)
        monthWheel.setValue(values.month)
        dayWheel.setValue(values.day)
        hourWheel.setValue(values.hour)
        minuteWheel.setValue(values.minute)
        secondWheel.setValue(values.second)
    }

    spacing: Units.dp(8)

    WheelColumn {
        id: dayWheel
        objectName: "dayWheel"
        title: qsTr("Day")
        from: 1
        //day 0 of the next month is the last day of this one
        to: new Date(wheels.year, wheels.month, 0).getDate()
        Layout.fillWidth: true
    }

    WheelColumn {
        id: monthWheel
        objectName: "monthWheel"
        title: qsTr("Month")
        from: 1
        to: 12
        Layout.fillWidth: true
    }

    WheelColumn {
        id: yearWheel
        objectName: "yearWheel"
        title: qsTr("Year")
        from: 2020
        to: 2100
        digits: 4
        Layout.fillWidth: true
        Layout.rightMargin: Units.dp(16)
    }

    WheelColumn {
        id: hourWheel
        objectName: "hourWheel"
        title: qsTr("Hour")
        to: 23
        Layout.fillWidth: true
    }

    WheelColumn {
        id: minuteWheel
        objectName: "minuteWheel"
        title: qsTr("Minute")
        Layout.fillWidth: true
    }

    WheelColumn {
        id: secondWheel
        objectName: "secondWheel"
        title: qsTr("Second")
        Layout.fillWidth: true
    }
}
