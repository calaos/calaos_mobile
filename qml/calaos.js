.pragma library

function doAjaxRequest (method, url, onFinished, onFailed, userData) {
    var jdata = JSON.stringify(userData);
    var ajax = new XMLHttpRequest ();
    ajax.onreadystatechange = function () {
        switch (ajax.readyState) {
        case XMLHttpRequest.UNSENT:
            console.log ("Ajax.doRequest :", method,  url, "state unsent");
            break;
        case XMLHttpRequest.OPENED:
            console.log ("Ajax.doRequest :", method,  url, "state opened");
            break;
        case XMLHttpRequest.HEADERS_RECEIVED:
            console.log ("Ajax.doRequest :", method,  url, "state headers received");
            break;
        case XMLHttpRequest.LOADING:
            console.log ("Ajax.doRequest :", method,  url, "state loading...");
            break;
        case XMLHttpRequest.DONE:
            console.log ("Ajax.doRequest :", method,  url, "state done !");
            if (ajax.status === 200) {
//                console.log(ajax.responseText);
                onFinished(JSON.parse(ajax.responseText));
            }
            else {
                console.log ("doAjaxRequest :", method,  url, "status not OK !!!", ajax.status, ajax.statusText);
                onFailed();
            }
            break;
        default:
            break;
        }
    }
    console.log ("doAjaxRequest url=", url);
    ajax.open((method || 'GET'), url);
    ajax.setRequestHeader("Accept", "application/json");
    ajax.setRequestHeader("Accept-encoding", "gzip,deflate");
    ajax.setRequestHeader("Accept-charset", "utf-8");
    ajax.setRequestHeader("Content-length", jdata.length);
    ajax.send(jdata);
}

var calaosApiUrl = "https://%1/api.php";

function loadHome(username, password, host, onFinished, onFailed) {

    doAjaxRequest('POST',
                  calaosApiUrl.arg(host),
                  onFinished,
                  onFailed,
                  {
                      "cn_user": username,
                      "cn_pass": password,
                      "action": "get_home"
                  });
}

function getRoomTypeIcon(room) {
    var rname;

    if (room == "salon") rname = "lounge.png";
    else if (room == "lounge") rname = "lounge.png";
    else if (room == "chambre") rname = "bedroom.png";
    else if (room == "bedroom") rname = "bedroom.png";
    else if (room == "cuisine") rname = "kitchen.png";
    else if (room == "kitchen") rname = "kitchen.png";
    else if (room == "bureau") rname = "office.png";
    else if (room == "office") rname = "office.png";
    else if (room == "sam") rname = "diningroom.png";
    else if (room == "diningroom") rname = "diningroom.png";
    else if (room == "cave") rname = "cellar.png";
    else if (room == "cellar") rname = "cellar.png";
    else if (room == "divers") rname = "various.png";
    else if (room == "various") rname = "various.png";
    else if (room == "misc") rname = "various.png";
    else if (room == "exterieur") rname = "outside.png";
    else if (room == "outside") rname = "outside.png";
    else if (room == "sdb") rname = "bathroom.png";
    else if (room == "bathroom") rname = "bathroom.png";
    else if (room == "hall") rname = "corridor.png";
    else if (room == "couloir") rname = "corridor.png";
    else if (room == "corridor") rname = "corridor.png";
    else if (room == "garage") rname = "garage.png";

    else rname = "various.png";

    return rname;
}
