function rootObject() {
    var next = parent
    while (next && next.parent)
        next = next.parent
    return next
}

function visualRoot() {
    var root = rootObject()
    if(root.hasOwnProperty("privateWindow"))
        return root.privateWindow
    return root;
}

function findParent(child, propertyName) {
    if (!child)
        return null
    var next = child.parent
    while (next && !next.hasOwnProperty(propertyName))
        next = next.parent
    return next
}

function sceneX(item) {
    // Binding may cause that this function is evaluated even when item is undefined,
    // but in that case the Binding isn't active however so we can safely return 0
    var x = 0
    if (item) {
        x = item.x
        var p = item.parent
        while (p) {
            x += p.x
            p = p.parent
        }
    }
    return x
}

function sceneY(item) {
    // Binding may cause that this function is evaluated even when item is undefined,
    // but in that case the Binding isn't active however so we can safely return 0
    var y = 0
    if (item) {
        y = item.y
        var p = item.parent
        while (p) {
            y += p.y
            p = p.parent
        }
    }
    return y
}

function findRoot(o) {
    while (o.parent) {
        o = o.parent
    }
    return o
}

function findRootChild(obj, objectName) {
    obj = findRoot(obj);

    var childs = new Array(0);
    childs.push(obj);
    while (childs.length > 0) {
        if (childs[0].objectName == objectName) {
            return childs[0];
        }
        for (var i in childs[0].data) {
            childs.push(childs[0].data[i]);
        }
        childs.splice(0, 1);
    }
    return null;
}

function timeToString(s) {
    var hours = Math.floor((s %= 86400) / 3600);
    var min = Math.floor((s %= 3600) / 60);
    var sec = s % 60;
    var res = "";

    if (hours == 1) {
        res += hours + " " + qsTr("hour") + " ";
    }
    if (hours > 1) {
        res += hours + " " + qsTr("hours") + " ";
    }
    if (min == 1) {
        res += min + " " + qsTr("minute") + " ";
    }
    if (min > 1) {
        res += min + " " + qsTr("minutes") + " ";
    }
    if (sec == 1) {
        res += sec + " " + qsTr("second") + " ";
    }
    if (sec > 1) {
        res += sec + " " + qsTr("seconds") + " ";
    }

    //trimmed
    return res.replace(/\s*$/, '').replace(/^\s*/, '');
}
