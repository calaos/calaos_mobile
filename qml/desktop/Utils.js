//Parent-chain helpers for the two views that reparent themselves onto an
//overlay layer. The reflection helpers that had no call site (rootObject,
//visualRoot, findParent, sceneX, sceneY) and the duration formatters (now
//SharedComponents/calaos.js formatDuration) were removed in T30.

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
