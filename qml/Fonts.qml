import QtQuick 2.0

Item {
    property string fontFamily: "Roboto Light"

    property string fontFamilyThin: "Roboto Thin"
    property string fontFamilyLight: "Roboto Light"
    property string fontFamilyRegular: "Roboto"
    property string fontFamilyMedium: "Roboto Medium"
    property string fontFamilyBold: "Roboto Black"

    FontLoader { id: local_B; source: "qrc:/fonts/Roboto-Black.ttf" }
    FontLoader { id: local_BI; source: "qrc:/fonts/Roboto-BlackItalic.ttf" }
    FontLoader { id: local_Bo; source: "qrc:/fonts/Roboto-Bold.ttf" }
    FontLoader { id: local_BoI; source: "qrc:/fonts/Roboto-BoldItalic.ttf" }
    FontLoader { id: local_I; source: "qrc:/fonts/Roboto-Italic.ttf" }
    FontLoader { id: local_L; source: "qrc:/fonts/Roboto-Light.ttf" }
    FontLoader { id: local_LI; source: "qrc:/fonts/Roboto-LightItalic.ttf" }
    FontLoader { id: local_M; source: "qrc:/fonts/Roboto-Medium.ttf" }
    FontLoader { id: local_MI; source: "qrc:/fonts/Roboto-MediumItalic.ttf" }
    FontLoader { id: local_R; source: "qrc:/fonts/Roboto-Regular.ttf" }
    FontLoader { id: local_T; source: "qrc:/fonts/Roboto-Thin.ttf" }
    FontLoader { id: local_TI; source: "qrc:/fonts/Roboto-ThinItalic.ttf" }
}
