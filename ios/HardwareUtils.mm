#import "HardwareUtils_iOS.h"
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import "Reachability.h"
#import "SimpleKeychain/A0SimpleKeychain.h"
#import "SimpleKeychain/A0SimpleKeychain+KeyPair.h"
#import "AlertPrompt.h"
#import "../src/Common.h"

#define SYSTEM_VERSION_GRATERTHAN_OR_EQUALTO(v)  ([[[UIDevice currentDevice] systemVersion] compare:v options:NSNumericSearch] != NSOrderedAscending)

// override QIOSApplicationDelegate to get
// launch options via didFinishLaunchingWithOptions.
// and performActionForShortcutItem

// This is hidden, so we declare it here to hook into it
@interface QIOSApplicationDelegate
@end

@interface QIOSApplicationDelegate: UIResponder <UIApplicationDelegate,UNUserNotificationCenterDelegate>
@end

@interface QIOSApplicationDelegate(AppDelegate)
@end

@implementation QIOSApplicationDelegate (AppDelegate)

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    Q_UNUSED(application);
    Q_UNUSED(launchOptions);

    // reset badge count
    [UIApplication sharedApplication].applicationIconBadgeNumber = 0;

    // Register to receive notifications from the system
    if (SYSTEM_VERSION_GRATERTHAN_OR_EQUALTO(@"10.0"))
    {
        UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
        center.delegate = self;
        [center requestAuthorizationWithOptions:(UNAuthorizationOptionSound |
                                                 UNAuthorizationOptionAlert |
                                                 UNAuthorizationOptionBadge)
                              completionHandler:^(BOOL granted, NSError * _Nullable error)
        {
            if (!error)
            {
                [[UIApplication sharedApplication] registerForRemoteNotifications];
                NSLog(@"Push registration success.");
            }
            else
            {
                NSLog(@"Push registration FAILED" );
                NSLog(@"ERROR: %@ - %@", error.localizedFailureReason, error.localizedDescription);
                NSLog(@"SUGGESTIONS: %@ - %@", error.localizedRecoveryOptions, error.localizedRecoverySuggestion);
            }
        }];
    }
    else
    {
        if ([application respondsToSelector:@selector(isRegisteredForRemoteNotifications)])
        {
            //iOS 8
            [application registerUserNotificationSettings:
                [UIUserNotificationSettings settingsForTypes:(UIUserNotificationTypeSound |
                                                              UIUserNotificationTypeAlert |
                                                              UIUserNotificationTypeBadge)
                                                  categories:nil]];
            [application registerForRemoteNotifications];
        }
        else
        {
            //iOS 7
            UIUserNotificationType nTypes = UIUserNotificationTypeBadge |
                                            UIRemoteNotificationTypeAlert |
                                            UIRemoteNotificationTypeSound;
            [application registerForRemoteNotificationTypes:nTypes];
        }
    }

//    HardwareUtils_iOS *o = (HardwareUtils_iOS *)HardwareUtils::Instance();
//    o->handleApplicationDidFinishLaunching(launchOptions);

    return YES;
}

- (void)application:(UIApplication *)application performActionForShortcutItem:(UIApplicationShortcutItem *)shortcutItem completionHandler:(void (^)(BOOL))completionHandler
{
    Q_UNUSED(application);
    Q_UNUSED(completionHandler);

    HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
    o->handlePerformActionForShortcutItem(shortcutItem);
}

#pragma mark - Remote Notification Delegate // <= iOS 9.x

- (void)application:(UIApplication*)application didRegisterUserNotificationSettings:(UIUserNotificationSettings *)notificationSettings
{
    [application registerForRemoteNotifications];
}

- (void)application:(UIApplication *)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken
{
    Q_UNUSED(application);

    NSLog(@"%@", deviceToken);

    HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
    o->handleRegisterNotif(deviceToken);
}

- (void)application:(UIApplication *)application didFailToRegisterForRemoteNotificationsWithError:(NSError *)error
{
    Q_UNUSED(application);

    HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
    o->handleRegisterNotifFail(error);

    NSLog(@"Did Fail to Register for Remote Notifications");
    NSLog(@"%@, %@", error, error.localizedDescription);
}

- (void)application:(UIApplication *)application didReceiveRemoteNotification:(nonnull NSDictionary *)userInfo fetchCompletionHandler:(nonnull void (^)(UIBackgroundFetchResult))completionHandler
{
    NSLog(@"Push Notification Information : %@", userInfo);

    NSString *uuid = userInfo[@"event_uuid"];
    if (uuid != nil)
    {
        HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
        if (o->isCalaosConnected())
            o->emitPushNotifReceived(QString::fromNSString(uuid));
        else
            o->handleStartedWithPushNotif(QString::fromNSString(uuid));
    }

    completionHandler(UIBackgroundFetchResultNoData);
}

#pragma mark - UNUserNotificationCenter Delegate // >= iOS 10

//Called when a notification is delivered to a foreground app.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center willPresentNotification:(UNNotification *)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    NSLog(@"User Info = %@", notification.request.content.userInfo);

    NSString *uuid = notification.request.content.userInfo[@"event_uuid"];
    if (uuid != nil)
    {
        HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
        o->emitPushNotifReceived(QString::fromNSString(uuid));
    }

    completionHandler(UNNotificationPresentationOptionNone);
}

//Called to let your app know which action was selected by the user for a given notification.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center didReceiveNotificationResponse:(UNNotificationResponse *)response withCompletionHandler:(void(^)())completionHandler
{
    NSLog(@"User Info = %@", response.notification.request.content.userInfo);

    NSString *uuid = response.notification.request.content.userInfo[@"event_uuid"];
    if (uuid != nil)
    {
        HardwareUtils_iOS *o = dynamic_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
        o->handleStartedWithPushNotif(QString::fromNSString(uuid));
    }
    completionHandler();
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
static A0SimpleKeychain *keychain;

HardwareUtils_iOS::HardwareUtils_iOS(QObject *parent):
    HardwareUtils(parent)
{
    hwclass = [[HWClass alloc] initWithHwObject: this];

    // This is the item wrapper used to access password information stored in the KeyChain
    keychain = [A0SimpleKeychain keychainWithService:@"CalaosMobile" accessGroup:@"9WYC46992U.fr.calaos.CalaosMobile"];

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
    [keychain release];
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

void HardwareUtils_iOS::setConfigOption(QString key, QString value)
{
    if (key == "calaos/host")
    {
        //This is used to share the hostname with our PushNotif Appext. they live in a different sandbox
        NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName: @"group.fr.calaos.CalaosMobile"];
        [sharedDefaults setValue:value.toNSString() forKey:@"calaos.hostname"];
    }
    
    HardwareUtils::setConfigOption(key, value);
}

void HardwareUtils_iOS::loadAuthKeychain(QString &email, QString &pass)
{
    email = QString::fromNSString([keychain stringForKey:@"calaos-cn-user"]);
    pass = QString::fromNSString([keychain stringForKey:@"calaos-cn-pass"]);
}

void HardwareUtils_iOS::saveAuthKeychain(const QString &email, const QString &pass)
{
    [keychain setString:email.toNSString() forKey:@"calaos-cn-user"];
    [keychain setString:pass.toNSString() forKey:@"calaos-cn-pass"];
}

void HardwareUtils_iOS::resetAuthKeychain()
{
    [keychain setString:Common::getDemoUser().toNSString() forKey:@"calaos-cn-user"];
    [keychain setString:Common::getDemoPass().toNSString() forKey:@"calaos-cn-pass"];
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
    NSDictionary *launchOptions = reinterpret_cast<NSDictionary *>(n);
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
    UIApplicationShortcutItem *shortcutItem = reinterpret_cast<UIApplicationShortcutItem *>(shortcut);
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

void HardwareUtils_iOS::handleRegisterNotif(void *data)
{
    NSData *tokenData = reinterpret_cast<NSData *>(data);

    const unsigned *tokenBytes = (const unsigned*)[tokenData bytes];
    NSString *tokenStr = [NSString stringWithFormat:@"%08x%08x%08x%08x%08x%08x%08x%08x",
            ntohl(tokenBytes[0]), ntohl(tokenBytes[1]), ntohl(tokenBytes[2]),
            ntohl(tokenBytes[3]), ntohl(tokenBytes[4]), ntohl(tokenBytes[5]),
            ntohl(tokenBytes[6]), ntohl(tokenBytes[7])];
    deviceToken = QString::fromNSString(tokenStr);

    qDebug() << "Device token is: " << deviceToken;
}

void HardwareUtils_iOS::handleRegisterNotifFail(void *err)
{
    NSError *error = reinterpret_cast<NSError *>(err);
    NSLog(@"Did Fail to Register for Remote Notifications");
    NSLog(@"%@, %@", error, error.localizedDescription);
    
    deviceToken.clear();
}

void HardwareUtils_iOS::handleStartedWithPushNotif(const QString &uuid)
{
    if (!uuid.isEmpty())
        startedWithNotif = true;
    notifUuid = uuid;
}
