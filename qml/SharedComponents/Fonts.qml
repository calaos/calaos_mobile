import QtQuick
import "."

Item {
    property string fontFamily: local_R.name

    property string fontFamilyThin: local_T.name
    property string fontFamilyLight: local_L.name
    property string fontFamilyRegular: local_R.name
    property string fontFamilyMedium: local_M.name
    property string fontFamilyBold: local_B.name

    property string fontFamilyCalaosLogo: local_CalaosLogo.name
    property string fontFamilyCalaosText: local_CalaosText.name
    property string fontFamilyCalaosTextBold: local_CalaosTextBold.name
    property string fontFamilyRepetitionScrolling: local_RepetitionScrolling.name

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

    FontLoader { id: local_CalaosLogo; source: "qrc:/fonts/calaos_logo.ttf" }
    FontLoader { id: local_CalaosText; source: "qrc:/fonts/calaos_text.ttf" }
    FontLoader { id: local_CalaosTextBold; source: "qrc:/fonts/calaos_text_bold.ttf" }
    FontLoader { id: local_RepetitionScrolling; source: "qrc:/fonts/RepetitionScrolling.ttf" }
}
