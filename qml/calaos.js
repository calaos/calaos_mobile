.pragma library

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
