#import "HardwareUtils_iOS.h"
#import <UIKit/UIKit.h>
#import "Reachability.h"
#import "KeychainItemWrapper.h"
#import "AlertPrompt.h"
#import "../src/Common.h"

// override QIOSApplicationDelegate to get
// launch options via didFinishLaunchingWithOptions.
// and performActionForShortcutItem
@interface QIOSApplicationDelegate
@end

@interface QIOSApplicationDelegate(AppDelegate)
@end

@implementation QIOSApplicationDelegate (AppDelegate)

//- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
//{
//    Q_UNUSED(application);

//    HardwareUtils_iOS *o = (HardwareUtils_iOS *)HardwareUtils::Instance();
//    o->handleApplicationDidFinishLaunching(launchOptions);

//    return YES;
//}

- (void)application:(UIApplication *)application performActionForShortcutItem:(UIApplicationShortcutItem *)shortcutItem completionHandler:(void (^)(BOOL))completionHandler
{
    Q_UNUSED(application);

    HardwareUtils_iOS *o = (HardwareUtils_iOS *)HardwareUtils::Instance();
    o->handlePerformActionForShortcutItem(shortcutItem);
}
@end

@interface HWClass : NSObject
{
@private
HardwareUtils_iOS *hwobj;
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

- (void)alertView:(UIAlertView *)alertView willDismissWithButtonIndex:(NSInteger)buttonIndex
{
    if (buttonIndex != [alertView cancelButtonIndex])
    {
        QString val = QString::fromNSString([(AlertPrompt *)alertView enteredText]);
        hwobj->emitDialogTextValid(val);
    }
    else
    {
        hwobj->emitDialogCancel();
    }
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

void HardwareUtils_iOS::resetAuthKeychain()
{
    [authItem setObject:Common::getDemoUser().toNSString() forKey:(id)kSecAttrAccount];
    [authItem setObject:Common::getDemoPass().toNSString() forKey:(id)kSecValueData];
}

void HardwareUtils_iOS::inputTextDialog(const QString &title, const QString &message)
{
    QString cancel(tr("Cancel"));
    QString valid(tr("Confirm"));

    AlertPrompt *prompt = [AlertPrompt alloc];
    prompt = [prompt initWithTitle:title.toNSString() message:message.toNSString() delegate:hwclass cancelButtonTitle:cancel.toNSString() okButtonTitle:valid.toNSString()];
    [prompt show];
    [prompt release];
}

void HardwareUtils_iOS::handleApplicationDidFinishLaunching(void *n)
{
    NSDictionary *launchOptions = (NSDictionary *)n;
    qDebug() << "handleApplicationDidFinishLaunching: " << launchOptions;

    //This gets if our app has been launched with options by clicking on a quick action
    UIApplicationShortcutItem *shortcutItem = [launchOptions objectForKey:UIApplicationLaunchOptionsShortcutItemKey];
    if(shortcutItem)
    {
        startedWithOpt = true;
        ioStartShortcut = QString::fromNSString(shortcutItem.type);
    }
}

void HardwareUtils_iOS::handlePerformActionForShortcutItem(void *shortcut)
{
    qDebug() << "handlePerformActionForShortcutItem: " << shortcut;
    UIApplicationShortcutItem *shortcutItem = (UIApplicationShortcutItem *)shortcut;
    if(shortcutItem)
    {
        startedWithOpt = true;
        ioStartShortcut = QString::fromNSString(shortcutItem.type);
    }
}

QString HardwareUtils_iOS::getStartOption(const QString &key)
{
    if (key == "scenario")
        return ioStartShortcut;
    return QString();
}

void HardwareUtils_iOS::setQuickLinks(QVariantList quicklinks)
{
    NSArray <UIApplicationShortcutItem *> *existingShortcutItems = [[UIApplication sharedApplication] shortcutItems];
    NSMutableArray <UIApplicationShortcutItem *> *updatedShortcutItems = [existingShortcutItems mutableCopy];
    [updatedShortcutItems removeAllObjects];

    for (int i = 0;i < quicklinks.count() && i < 4;i++)
    {
        QVariantMap link = quicklinks.at(i).toMap();

        UIApplicationShortcutIcon *icon = [UIApplicationShortcutIcon iconWithTemplateImageName: @"scenario.png"];
        UIApplicationShortcutItem *item = [[UIApplicationShortcutItem alloc]initWithType: link["id"].toString().toNSString()
                                                                          localizedTitle: link["name"].toString().toNSString()
                                                                       localizedSubtitle: nil
                                                                                    icon: icon
                                                                                userInfo: nil];
        [updatedShortcutItems addObject: item];
    }

    [[UIApplication sharedApplication] setShortcutItems: updatedShortcutItems];
}
