pragma Singleton
import QtQuick 2.5
import QuickFlux 1.0

KeyTable {

    // KeyTable is an object with properties equal to its key name

    // Desktop actions
    property string clickHomeboardItem;

    property string hideHomeboardMenu;
    property string showHomeboardMenu;

    property string saveWidgetsPosition
    property string resetWidgetsPosition

    property string wakeupScreen
    property string suspendScreen

    property string openCameraSingleView

    property string showMainMenu
    property string hideMainMenu

    property string newLogItem
    property string showRebootDialog

    //Keyboard actions
    property string openAskTextForIo
    property string openKeyboard

    //Keyboard return actions
    property string changeNoteText
    property string addUserInfoEmail
    property string webGoToUrl

    //Mobile actions
    property string openEventLog
    property string openEventPushViewer
    property string openEventPushViewerUuid

    property string showNotificationMsg

    property string showReadOnlyBootDialog
}

