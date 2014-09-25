#import "../src/HardwareUtils.h"
#import <UIKit/UIKit.h>

HardwareUtils::HardwareUtils(QObject *parent):
    QObject(parent)
{
}

void HardwareUtils::showAlertMessage(QString title, QString message)
{
    UIAlertView *errorAlert = [[UIAlertView alloc]
            initWithTitle:[NSString stringWithUTF8String: title.toUtf8().data()]
            message:[NSString stringWithUTF8String: message.toUtf8().data()]
            delegate:nil
            cancelButtonTitle:@"Ok"
            otherButtonTitles:nil];
    [errorAlert show];
    [errorAlert release];
}
