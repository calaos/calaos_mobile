.pragma library
.import QtQuick as QtQuick

function getRoomTypeIcon(room) {
    var rname;

    if (room === "salon") rname = "lounge";
    else if (room === "lounge") rname = "lounge";
    else if (room === "chambre") rname = "bedroom";
    else if (room === "bedroom") rname = "bedroom";
    else if (room === "cuisine") rname = "kitchen";
    else if (room === "kitchen") rname = "kitchen";
    else if (room === "bureau") rname = "office";
    else if (room === "office") rname = "office";
    else if (room === "sam") rname = "diningroom";
    else if (room === "diningroom") rname = "diningroom";
    else if (room === "cave") rname = "cellar";
    else if (room === "cellar") rname = "cellar";
    else if (room === "divers") rname = "various";
    else if (room === "various") rname = "various";
    else if (room === "misc") rname = "various";
    else if (room === "exterieur") rname = "outside";
    else if (room === "outside") rname = "outside";
    else if (room === "sdb") rname = "bathroom";
    else if (room === "bathroom") rname = "bathroom";
    else if (room === "hall") rname = "corridor";
    else if (room === "couloir") rname = "corridor";
    else if (room === "corridor") rname = "corridor";
    else if (room === "garage") rname = "garage";
    else if (room === "fav") rname = "fav";

    else rname = "various";

    return rname;
}

function singleshotTimer(interval, callback) {
    var component = Qt.createComponent("SingleShotTimer.qml")
    if (component.status === QtQuick.Component.Ready || component.status === QtQuick.Component.Error) {
        singleshotTimerCreated(component, interval, callback)
    } else {
        component.statusChanged.connect(function() {
            singleshotTimerCreated(component, interval, callback)
        })
    }
}

function singleshotTimerCreated(component, interval, callback) {
    if (component.status === QtQuick.Component.Ready) {
        var obj = component.createObject(null, { "interval": interval })
        obj.triggered.connect( function () {
            callback();
            obj.destroy();
        } );
        obj.start();
    } else if (component.status === QtQuick.Component.Error) {
        console.error("Error loading component SingleShotTimer.qml:", component.errorString());
    }
}

//Styles understood by formatDuration(). Three call sites wanted three
//renderings of a number of seconds, and each grew its own formatter with its
//own arithmetic and its own translation rules; the style is the difference,
//so it is a parameter rather than a second function.
var Duration = {
    //"04:07", "01:02:33" - a digital clock, for playback positions where the
    //user is reading a moving number.
    Clock: "clock",
    //"1 hour 5 minutes" - every non-zero unit, for a delay the user configured
    //and wants read back exactly.
    Long: "long",
    //"3 days" - the largest non-zero unit alone, for uptimes and other
    //quantities where only the order of magnitude matters.
    Short: "short"
}

function pad2(n) {
    return n < 10 ? "0" + n : "" + n
}

//Every unit as a whole translated sentence with a %1 placeholder. The old
//timeToString() concatenated a number and a bare translated word, which fixes
//the number before the unit and the unit in the nominative: languages that
//order or inflect them differently could not be translated correctly.
function durationUnitText(unit, count) {
    if (unit === "days")
        return count === 1 ? qsTr("%1 day").arg(count) : qsTr("%1 days").arg(count)
    if (unit === "hours")
        return count === 1 ? qsTr("%1 hour").arg(count) : qsTr("%1 hours").arg(count)
    if (unit === "minutes")
        return count === 1 ? qsTr("%1 minute").arg(count) : qsTr("%1 minutes").arg(count)
    return count === 1 ? qsTr("%1 second").arg(count) : qsTr("%1 seconds").arg(count)
}

//The one duration formatter. Callers choose a style instead of a function, so
//the seconds arithmetic and the translated unit names live in a single place.
function formatDuration(seconds, style) {
    var total = Math.floor(Number(seconds))
    if (!isFinite(total) || total < 0)
        total = 0

    if (style === Duration.Clock) {
        var ch = Math.floor(total / 3600)
        var cm = Math.floor((total % 3600) / 60)
        var cs = total % 60
        return ch > 0 ? pad2(ch) + ":" + pad2(cm) + ":" + pad2(cs)
                      : pad2(cm) + ":" + pad2(cs)
    }

    var units = [["days",    Math.floor(total / 86400)],
                 ["hours",   Math.floor((total % 86400) / 3600)],
                 ["minutes", Math.floor((total % 3600) / 60)],
                 ["seconds", total % 60]]

    if (style === Duration.Short) {
        for (var i = 0; i < units.length; i++) {
            if (units[i][1] > 0)
                return durationUnitText(units[i][0], units[i][1])
        }
        //Under a second: say so rather than return nothing.
        return durationUnitText("seconds", 0)
    }

    var parts = []
    for (var j = 0; j < units.length; j++) {
        if (units[j][1] > 0)
            parts.push(durationUnitText(units[j][0], units[j][1]))
    }
    return parts.length > 0 ? parts.join(" ") : durationUnitText("seconds", 0)
}
