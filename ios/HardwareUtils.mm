#import "HardwareUtils_iOS.h"
#import <UIKit/UIKit.h>
#import "Reachability.h"
#import "KeychainItemWrapper.h"

@interface HWClass : NSObject
{
@private
HardwareUtils *hwobj;
}

-(id)initWithHwObject:(HardwareUtils_iOS *)obj;

@end

@implementation HWClass

-(id)initWithHwObject:(HardwareUtils_iOS *)obj
{
    self = [super init];
    if (self)
    {
        hwobj = obj;
    }
    return self;
}

-(void)reachabilityChanged:(NSNotification*)notif
{
    Q_UNUSED(notif)
    hwobj->emitNetworkStatusChanged();
}

-(void)applicationWillResignActive:(NSNotification*)notif
{
    Q_UNUSED(notif)
    hwobj->emitApplicationActiveChanged(false);
}

-(void)applicationDidBecomeActive:(NSNotification*)notif
{
    Q_UNUSED(notif)
    hwobj->emitApplicationActiveChanged(true);
}

@end

static HWClass *hwclass;
static Reachability *reach;
static KeychainItemWrapper *authItem;

HardwareUtils_iOS::HardwareUtils_iOS(QObject *parent):
    HardwareUtils(parent)
{
    hwclass = [[HWClass alloc] initWithHwObject: this];

    // This is the item wrapper used to access password information stored in the KeyChain
    authItem = [[KeychainItemWrapper alloc] initWithIdentifier:@"Password" accessGroup:nil];

    // Allocate a reachability object
    reach = [Reachability reachabilityForInternetConnection];

    // Here we set up a NSNotification observer.
    [[NSNotificationCenter defaultCenter] addObserver:hwclass
                                             selector:@selector(reachabilityChanged:)
                                                 name:kReachabilityChangedNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:hwclass
                                             selector:@selector(applicationWillResignActive:)
                                                 name:UIApplicationWillResignActiveNotification
                                               object:nil];

    [[NSNotificationCenter defaultCenter] addObserver:hwclass
                                             selector:@selector(applicationDidBecomeActive:)
                                                 name:UIApplicationDidBecomeActiveNotification
                                               object:nil];

    [reach startNotifier];
}

HardwareUtils_iOS::~HardwareUtils_iOS()
{
    [[NSNotificationCenter defaultCenter] removeObserver:hwclass];
    [reach release];
    [hwclass release];
    [authItem release];
}

void HardwareUtils_iOS::showAlertMessage(QString title, QString message, QString buttontext)
{
    UIAlertView *errorAlert = [[UIAlertView alloc]
                    initWithTitle:title.toNSString()
                          message:message.toNSString()
                         delegate:nil
                cancelButtonTitle:buttontext.toNSString()
                otherButtonTitles:nil];
    [errorAlert show];
    [errorAlert release];
}

int HardwareUtils_iOS::getNetworkStatus()
{
    return (int)[reach currentReachabilityStatus];
}

void HardwareUtils_iOS::showNetworkActivity(bool en)
{
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:en];
}

void HardwareUtils_iOS::loadAuthKeychain(QString &email, QString &pass)
{
    email = QString::fromNSString([authItem objectForKey:(id)kSecAttrAccount]);
    pass = QString::fromNSString([authItem objectForKey:(id)kSecValueData]);
}

void HardwareUtils_iOS::saveAuthKeychain(const QString &email, const QString &pass)
{
    [authItem setObject:email.toNSString() forKey:(id)kSecAttrAccount];
    [authItem setObject:pass.toNSString() forKey:(id)kSecValueData];
}
