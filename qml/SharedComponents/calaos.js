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
        console.log("Error loading component SingleShotTimer.qml:", component.errorString());
    }
}

