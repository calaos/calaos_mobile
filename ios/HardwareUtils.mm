#import "HardwareUtils_iOS.h"
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <objc/runtime.h>
#import <fcntl.h>
#import <sys/file.h>
#import <unistd.h>
#import "Reachability.h"
#import "SimpleKeychain/A0SimpleKeychain.h"
#import "SimpleKeychain/A0SimpleKeychain+KeyPair.h"
#import "../src/Common.h"

/*
 Application delegate integration (IOS-2).

 Qt owns the UIApplication delegate (QIOSApplicationDelegate, a private class).
 Since Qt 6.12, UIApplicationMain() starts with a placeholder delegate
 (QIOSDynamicSceneDelegate, src/entrypoint/qtentrypoint_ios.mm) and main() runs
 from UIApplicationDidFinishLaunchingNotification; QIOSIntegration only installs
 QIOSApplicationDelegate while QGuiApplication is constructed. Neither class
 implements application:didFinishLaunchingWithOptions:, the push callbacks or
 the Quick Action callbacks, but QIOSApplicationDelegate is no longer the
 delegate at launch: the category the app used to put on it was never sent
 didFinishLaunchingWithOptions:, so the app never registered with APNs.
 Qt has no public API for the APNs token or remote notifications.

 Instead:
 - the launch work (badge reset, notification center delegate, APNs
   registration) runs from HardwareUtils_iOS::platformInit(), i.e. from main()
   before the event loop starts, still inside the launch sequence;
 - notification taps and foreground presentation go through an app-owned
   UNUserNotificationCenter delegate (CalaosNotificationDelegate);
 - the callbacks UIKit only sends to the application delegate (APNs token,
   remote notification) or to the window scene delegate (Quick Actions) are
   installed at runtime on the classes Qt uses (installHook()): added when the
   class does not implement the selector, otherwise swizzled with the previous
   implementation still called, so a future Qt implementation keeps working.
*/

//Shared with the PushNotif extension (NotificationService.m).
static NSString *const kAppGroup = @"group.fr.calaos.CalaosMobile";
//False after an explicit logout, see HardwareUtils::setSessionActive().
static NSString *const kSessionActiveKey = @"calaos.session_active";
//Unread notifications on the app icon, incremented by the extension (IOS-3).
static NSString *const kBadgeCountKey = @"calaos.badge_count";
//flock() file in the App Group container, same name as in the extension.
static NSString *const kBadgeLockFile = @"calaos.badge_count.lock";

namespace {

HardwareUtils_iOS *iosUtils()
{
    return static_cast<HardwareUtils_iOS *>(HardwareUtils::Instance());
}

//Installs block as the implementation of sel on cls. Returns the
//implementation the class responded with before (its own or an inherited
//one), which the hook must call, or nullptr when there was none.
//Must be called once per class and selector: a second call would chain the
//hook to itself.
IMP installHook(Class cls, SEL sel, Protocol *proto, id block)
{
    Method m = class_getInstanceMethod(cls, sel);
    if (!m)
    {
        const char *types = protocol_getMethodDescription(proto, sel, NO, YES).types;
        if (!types)
        {
            qWarning() << "No type encoding for" << sel_getName(sel) << "in"
                       << protocol_getName(proto) << ": not hooked";
            return nullptr;
        }
        class_addMethod(cls, sel, imp_implementationWithBlock(block), types);
        return nullptr;
    }

    IMP imp = imp_implementationWithBlock(block);
    IMP previous = method_getImplementation(m);
    //Inherited: add an override on cls. Defined on cls itself: replace it.
    if (!class_addMethod(cls, sel, imp, method_getTypeEncoding(m)))
        method_setImplementation(m, imp);
    return previous;
}

IMP qtDidRegisterForRemote = nullptr;
IMP qtDidFailToRegisterForRemote = nullptr;
IMP qtDidReceiveRemote = nullptr;
IMP qtSceneWillConnect = nullptr;
IMP qtPerformShortcut = nullptr;

void hookApplicationDelegate()
{
    static bool installed = false;
    if (installed)
        return;

    UIApplication *app = [UIApplication sharedApplication];
    id<UIApplicationDelegate> delegate = app.delegate;
    if (!delegate)
    {
        qWarning() << "No application delegate: push notifications disabled";
        return;
    }

    installed = true;
    Class cls = object_getClass(delegate);
    if (![NSStringFromClass(cls) isEqualToString:@"QIOSApplicationDelegate"])
        qWarning() << "Unexpected application delegate class"
                   << QString::fromNSString(NSStringFromClass(cls)) << ", hooked anyway";
    Protocol *proto = @protocol(UIApplicationDelegate);

    const SEL didRegister = @selector(application:didRegisterForRemoteNotificationsWithDeviceToken:);
    qtDidRegisterForRemote = installHook(cls, didRegister, proto,
        ^(id self_, UIApplication *application, NSData *deviceToken)
    {
        if (qtDidRegisterForRemote)
            reinterpret_cast<void (*)(id, SEL, UIApplication *, NSData *)>(qtDidRegisterForRemote)(
                self_, didRegister, application, deviceToken);

        //No log of the token: it is a per-device credential (logged redacted
        //by HardwareUtils::setDeviceToken()).
        iosUtils()->handleRegisterNotif(deviceToken);
    });

    const SEL didFail = @selector(application:didFailToRegisterForRemoteNotificationsWithError:);
    qtDidFailToRegisterForRemote = installHook(cls, didFail, proto,
        ^(id self_, UIApplication *application, NSError *error)
    {
        if (qtDidFailToRegisterForRemote)
            reinterpret_cast<void (*)(id, SEL, UIApplication *, NSError *)>(qtDidFailToRegisterForRemote)(
                self_, didFail, application, error);

        iosUtils()->handleRegisterNotifFail(error);
    });

    const SEL didReceive = @selector(application:didReceiveRemoteNotification:fetchCompletionHandler:);
    qtDidReceiveRemote = installHook(cls, didReceive, proto,
        ^(id self_, UIApplication *application, NSDictionary *userInfo,
          void (^completionHandler)(UIBackgroundFetchResult))
    {
        NSString *uuid = userInfo[@"event_uuid"];
        if (uuid != nil)
        {
            //Not a tap: a push *arriving* (iOS hands remote notifications to
            //the app here, in the foreground or woken in the background). The
            //app has always opened its event like a tap: right away if logged
            //in, otherwise once home is loaded again. Kept as is on purpose
            //(product behaviour predating FIX-3), see doc/ios.md section 4.
            HardwareUtils::Instance()->handleNotificationOpened(QString::fromNSString(uuid));
        }

        //The completion handler must be called once: by Qt if it handles the
        //notification too, by us otherwise.
        if (qtDidReceiveRemote)
            reinterpret_cast<void (*)(id, SEL, UIApplication *, NSDictionary *,
                                      void (^)(UIBackgroundFetchResult))>(qtDidReceiveRemote)(
                self_, didReceive, application, userInfo, completionHandler);
        else
            completionHandler(UIBackgroundFetchResultNoData);
    });

    //UIApplication may cache which optional methods its delegate implements
    //when the delegate is set (the token callback is looked up at call time,
    //nothing guarantees it for the others): set it again so that the methods
    //added above are seen. The property is assign, Qt keeps the object alive.
    app.delegate = nil;
    app.delegate = delegate;
}

void hookSceneDelegate()
{
    static bool installed = false;
    if (installed)
        return;
    installed = true;

    //Qt makes this class the delegate of every application window scene
    //(QIOSApplicationDelegate application:configurationForConnectingSceneSession:options:).
    //With scenes, UIKit hands Quick Actions to the scene delegate only, never
    //to application:performActionForShortcutItem:completionHandler:.
    Class cls = NSClassFromString(@"QIOSWindowSceneDelegate");
    if (!cls)
    {
        qWarning() << "Qt window scene delegate not found: Quick Actions disabled";
        return;
    }

    Protocol *proto = @protocol(UIWindowSceneDelegate);

    const SEL willConnect = @selector(scene:willConnectToSession:options:);
    qtSceneWillConnect = installHook(cls, willConnect, proto,
        ^(id self_, UIScene *scene, UISceneSession *session, UISceneConnectionOptions *options)
    {
        if (qtSceneWillConnect)
            reinterpret_cast<void (*)(id, SEL, UIScene *, UISceneSession *, UISceneConnectionOptions *)>(
                qtSceneWillConnect)(self_, willConnect, scene, session, options);

        //Launched from a Quick Action: the item only comes with the connection
        //options, windowScene:performActionForShortcutItem: is not called.
        if (options.shortcutItem)
            iosUtils()->handlePerformActionForShortcutItem(options.shortcutItem);

        //A notification tap that launched the app (options.notificationResponse)
        //is not handled here: UIKit sends it to the notification center
        //delegate (CalaosNotificationDelegate); handling it here as well opens
        //the event twice when that callback arrives after the login.
    });

    const SEL performShortcut = @selector(windowScene:performActionForShortcutItem:completionHandler:);
    qtPerformShortcut = installHook(cls, performShortcut, proto,
        ^(id self_, UIWindowScene *windowScene, UIApplicationShortcutItem *shortcutItem,
          void (^completionHandler)(BOOL))
    {
        iosUtils()->handlePerformActionForShortcutItem(shortcutItem);

        if (qtPerformShortcut)
            reinterpret_cast<void (*)(id, SEL, UIWindowScene *, UIApplicationShortcutItem *,
                                      void (^)(BOOL))>(qtPerformShortcut)(
                self_, performShortcut, windowScene, shortcutItem, completionHandler);
        else
            completionHandler(YES);
    });
}

//Runs block on the unread counter under the lock the extension takes for its
//read-increment-write. Best effort: called on the main thread, so it never
//waits for an extension that holds the lock (about 10 ms at most, then the
//block runs unlocked); without the lock, or with cfprefsd reordering writes,
//an increment racing with the block can be lost (badge off by one until the
//next reset).
void withBadgeCountLocked(void (^block)(NSUserDefaults *sharedDefaults))
{
    int fd = -1;
    NSURL *container = [[NSFileManager defaultManager]
        containerURLForSecurityApplicationGroupIdentifier:kAppGroup];
    if (container != nil)
    {
        NSString *lockPath = [container URLByAppendingPathComponent:kBadgeLockFile].path;
        fd = open(lockPath.fileSystemRepresentation, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    }

    bool locked = false;
    for (int attempt = 0; fd >= 0 && !locked && attempt < 5; attempt++)
    {
        if (attempt > 0)
            usleep(2000);
        locked = flock(fd, LOCK_EX | LOCK_NB) == 0;
    }

    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    block(sharedDefaults);
    [sharedDefaults release];

    if (locked)
        flock(fd, LOCK_UN);
    if (fd >= 0)
        close(fd);
}

//The user is looking at the app: nothing is unread any more.
void resetBadge()
{
    withBadgeCountLocked(^(NSUserDefaults *sharedDefaults) {
        [sharedDefaults setInteger:0 forKey:kBadgeCountKey];
    });
    [[UNUserNotificationCenter currentNotificationCenter] setBadgeCount:0 withCompletionHandler:nil];
}

} // namespace

#pragma mark - UNUserNotificationCenter delegate

@interface CalaosNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation CalaosNotificationDelegate

//Called when a notification is delivered to a foreground app.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center willPresentNotification:(UNNotification *)notification withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler
{
    Q_UNUSED(center);

    NSString *uuid = notification.request.content.userInfo[@"event_uuid"];
    if (uuid != nil)
        iosUtils()->emitPushNotifReceived(QString::fromNSString(uuid));

    //The extension already counted it, but the user sees it in the app: no
    //badge (the content's badge is not applied without the Badge option).
    resetBadge();

    completionHandler(UNNotificationPresentationOptionNone);
}

//Called to let your app know which action was selected by the user for a given notification.
- (void)userNotificationCenter:(UNUserNotificationCenter *)center didReceiveNotificationResponse:(UNNotificationResponse *)response withCompletionHandler:(void (^)(void))completionHandler
{
    Q_UNUSED(center);

    NSString *uuid = response.notification.request.content.userInfo[@"event_uuid"];
    if (uuid != nil)
    {
        //Opened now if logged in, otherwise once home is loaded again (a tap
        //from the background arrives while the session is down: the app
        //logs out when it resigns active).
        HardwareUtils::Instance()->handleNotificationOpened(QString::fromNSString(uuid));
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
-(void)showAlertTitle:(NSString *)title message:(NSString *)message button:(NSString *)button;
-(void)showInputDialogTitle:(NSString *)title message:(NSString *)message cancel:(NSString *)cancel confirm:(NSString *)confirm;

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
    resetBadge();
    hwobj->emitApplicationActiveChanged(true);
}

-(UIViewController *)rootViewController
{
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes.allObjects) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            UIWindowScene *ws = (UIWindowScene *)scene;
            UIViewController *rootVC = ws.windows.firstObject.rootViewController;
            if (rootVC) return rootVC;
        }
    }
    return nil;
}

-(void)showAlertTitle:(NSString *)title message:(NSString *)message button:(NSString *)button
{
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:title
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    UIAlertAction *ok = [UIAlertAction actionWithTitle:button
                                                style:UIAlertActionStyleDefault
                                              handler:nil];
    [alert addAction:ok];
    [[self rootViewController] presentViewController:alert animated:YES completion:nil];
}

-(void)showInputDialogTitle:(NSString *)title message:(NSString *)message cancel:(NSString *)cancel confirm:(NSString *)confirm
{
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:title
                                                                   message:message
                                                            preferredStyle:UIAlertControllerStyleAlert];
    [alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
        textField.autocorrectionType = UITextAutocorrectionTypeNo;
    }];
    HardwareUtils_iOS *hw = hwobj;
    UIAlertAction *confirmAction = [UIAlertAction actionWithTitle:confirm
                                                           style:UIAlertActionStyleDefault
                                                         handler:^(UIAlertAction *action) {
        NSString *text = alert.textFields.firstObject.text ?: @"";
        hw->emitDialogTextValid(QString::fromNSString(text));
    }];
    UIAlertAction *cancelAction = [UIAlertAction actionWithTitle:cancel
                                                          style:UIAlertActionStyleCancel
                                                        handler:^(UIAlertAction *action) {
        hw->emitDialogCancel();
    }];
    [alert addAction:cancelAction];
    [alert addAction:confirmAction];
    [[self rootViewController] presentViewController:alert animated:YES completion:nil];
}

@end

static HWClass *hwclass;
static Reachability *reach;
static A0SimpleKeychain *keychain;
//UNUserNotificationCenter.delegate does not retain it.
static CalaosNotificationDelegate *notificationDelegate;

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

void HardwareUtils_iOS::platformInit(QQmlApplicationEngine *e)
{
    HardwareUtils::platformInit(e);

    //QGuiApplication exists: Qt has installed its application delegate. Still
    //inside the launch sequence (main() runs from
    //UIApplicationDidFinishLaunchingNotification, before the event loop), so
    //the scene is not connected yet and the notification center delegate is
    //set in time for a tap that launched the app.
    hookApplicationDelegate();
    hookSceneDelegate();

    UIApplication *app = [UIApplication sharedApplication];
    UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

    //Status bar style: UIStatusBarStyle in AppInfo.plist
    //(UIViewControllerBasedStatusBarAppearance = false).
    resetBadge();

    notificationDelegate = [[CalaosNotificationDelegate alloc] init];
    center.delegate = notificationDelegate;

    [center requestAuthorizationWithOptions:(UNAuthorizationOptionSound |
                                             UNAuthorizationOptionAlert |
                                             UNAuthorizationOptionBadge)
                          completionHandler:^(BOOL granted, NSError * _Nullable error)
    {
        if (error)
            qWarning() << "Notification authorization failed:"
                       << QString::fromNSString(error.localizedDescription);
        else if (!granted)
            qDebug() << "Notification authorization denied by the user";
    }];

    //The APNs token does not depend on the user's authorization: register on
    //every launch, the token comes back through the hooked delegate.
    [app registerForRemoteNotifications];
}

void HardwareUtils_iOS::showAlertMessage(QString title, QString message, QString buttontext)
{
    [hwclass showAlertTitle:title.toNSString()
                   message:message.toNSString()
                    button:buttontext.toNSString()];
}

int HardwareUtils_iOS::getNetworkStatus()
{
    return (int)[reach currentReachabilityStatus];
}

QString HardwareUtils_iOS::getDeviceName()
{
    //Since iOS 16 this is the generic model ("iPhone") unless the app holds
    //the user-assigned-device-name entitlement, which it does not request.
    return QString::fromNSString([UIDevice currentDevice].name);
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

    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    [sharedDefaults removeObjectForKey:kSessionActiveKey];
    [sharedDefaults release];

    withBadgeCountLocked(^(NSUserDefaults *defaults) {
        [defaults removeObjectForKey:kBadgeCountKey];
    });
    [[UNUserNotificationCenter currentNotificationCenter] setBadgeCount:0 withCompletionHandler:nil];
}

void HardwareUtils_iOS::setSessionActive(bool active)
{
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    [sharedDefaults setBool:active ? YES : NO forKey:kSessionActiveKey];
    [sharedDefaults release];
}

bool HardwareUtils_iOS::isSessionActive()
{
    //Absent (fresh install, update, reset): active.
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    const bool active = [sharedDefaults objectForKey:kSessionActiveKey] == nil ||
                        [sharedDefaults boolForKey:kSessionActiveKey];
    [sharedDefaults release];
    return active;
}

void HardwareUtils_iOS::inputTextDialog(const QString &title, const QString &message)
{
    QString cancel(tr("Cancel"));
    QString valid(tr("Confirm"));
    [hwclass showInputDialogTitle:title.toNSString()
                         message:message.toNSString()
                          cancel:cancel.toNSString()
                         confirm:valid.toNSString()];
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

    //Hex of every byte: APNs tokens have no fixed length (32 bytes on
    //devices today, 80 on the iOS 27 simulator).
    const unsigned char *bytes = static_cast<const unsigned char *>(tokenData.bytes);
    NSMutableString *tokenStr = [NSMutableString stringWithCapacity:tokenData.length * 2];
    for (NSUInteger i = 0; i < tokenData.length; i++)
        [tokenStr appendFormat:@"%02x", bytes[i]];
    //Emits deviceTokenChanged() when the token is new or rotated, so that a
    //token reported after the login still reaches calaos_server.
    setDeviceToken(QString::fromNSString(tokenStr));
}

void HardwareUtils_iOS::handleRegisterNotifFail(void *err)
{
    NSError *error = reinterpret_cast<NSError *>(err);
    qWarning() << "Failed to register for remote notifications:"
               << QString::fromNSString(error.localizedDescription);

    clearDeviceToken();
}
