//
//  NotificationService.m
//  PushNotif
//
//  Created by Raoul Hecky on 22/02/2018.
//

#import "NotificationService.h"
#import <fcntl.h>
#import <sys/file.h>
#import <unistd.h>
#import "ios/SimpleKeychain/A0SimpleKeychain.h"
#import "ios/SimpleKeychain/A0SimpleKeychain+KeyPair.h"

@interface NotificationService ()

@property (nonatomic, strong) void (^contentHandler)(UNNotificationContent *contentToDeliver);
@property (nonatomic, strong) UNMutableNotificationContent *bestAttemptContent;

@end

//Shared with the app (ios/HardwareUtils.mm).
static NSString *const kAppGroup = @"group.fr.calaos.CalaosMobile";
//Unread notifications shown on the app icon. Incremented here for every push,
//reset to 0 by the app (launch, become active, push presented in the
//foreground) and removed by "reset all data".
static NSString *const kBadgeCountKey = @"calaos.badge_count";
//flock() file in the App Group container, best effort exclusion between the
//read-increment-write here, the app's reset (separate process) and concurrent
//requests in this process. Same name in HardwareUtils.mm.
static NSString *const kBadgeLockFile = @"calaos.badge_count.lock";

@implementation NotificationService

- (void)didReceiveNotificationRequest:(UNNotificationRequest *)request withContentHandler:(void (^)(UNNotificationContent * _Nonnull))contentHandler {
    self.contentHandler = contentHandler;
    self.bestAttemptContent = [request.content mutableCopy];

    //First, so that every path below (and serviceExtensionTimeWillExpire)
    //delivers the badge, with or without a picture.
    self.bestAttemptContent.badge = @([self incrementBadgeCount]);
    
    //Check for attachment
    NSDictionary *userInfo = request.content.userInfo;
    if (userInfo == nil) {
        [self contentComplete];
        return;
    }
    
    NSString *uuid = userInfo[@"event_uuid"];
    if (uuid == nil) {
        [self contentComplete];
        return;
    }

    //Logged out on purpose in the app: the stored credentials stay for the
    //login screen but are not used here (same rule as Android's
    //PushPictureWorker). The text of the notification is shown as is.
    if (![self isSessionActive]) {
        [self contentComplete];
        return;
    }
    
    // load the attachment
    [self loadAttachmentForUuid:uuid
              completionHandler:^(UNNotificationAttachment *attachment) {
                  if (attachment) {
                      self.bestAttemptContent.attachments = [NSArray arrayWithObject:attachment];
                  }
                  [self contentComplete];
              }];
    
    // Modify the notification content here...
    //self.bestAttemptContent.title = [NSString stringWithFormat:@"%@ [modified]", self.bestAttemptContent.title];
}

- (void)serviceExtensionTimeWillExpire {
    // Called just before the extension will be terminated by the system.
    // Use this as an opportunity to deliver your "best attempt" at modified content, otherwise the original push payload will be used.
    self.contentHandler(self.bestAttemptContent);
}

- (void)contentComplete {
    self.contentHandler(self.bestAttemptContent);
}

- (void)loadAttachmentForUuid:(NSString *)uuid
            completionHandler:(void(^)(UNNotificationAttachment *))completionHandler {
    
    __block UNNotificationAttachment *attachment = nil;

    A0SimpleKeychain *keychain = [A0SimpleKeychain keychainWithService:@"CalaosMobile" accessGroup:@"9WYC46992U.fr.calaos.CalaosMobile"];
    
    NSString *urlString = [NSString stringWithFormat:@"%@?cn_user=%@&cn_pass=%@&action=eventlog&uuid=%@",
                           [self getCalaosHost],
                           [keychain stringForKey:@"calaos-cn-user"],
                           [keychain stringForKey:@"calaos-cn-pass"],
                           uuid];
    NSURLRequest *eventURL = [NSURLRequest requestWithURL:[NSURL URLWithString:urlString]];
    
    NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]];
    
    NSURLSessionDataTask *dataTask = [session dataTaskWithRequest:eventURL
                                                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error)
                                      {
                                          if (error != nil) {
                                              NSLog(@"%@", error.localizedDescription);
                                              completionHandler(attachment);
                                              return;
                                          }
                                          
                                          NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
                                          if (httpResponse.statusCode == 200)
                                          {
                                              NSError *parseError = nil;
                                              NSDictionary *jsonObject = [NSJSONSerialization JSONObjectWithData:data options:0 error:&parseError];
                                              if (parseError != nil || jsonObject == nil) {
                                                  if (parseError == nil)
                                                      NSLog(@"JSon is not an object");
                                                  else
                                                      NSLog(@"Failed to load calaos event: %@", parseError.localizedDescription);
                                                  completionHandler(nil);
                                                  return;
                                              }
                                              
                                              NSDictionary *jevent = (NSDictionary *)jsonObject;
                                              NSString *picUid = [jevent objectForKey:@"pic_uid"];
                                              if (!picUid) {
                                                  NSLog(@"pic_uid not found");
                                                  completionHandler(nil);
                                                  return;
                                              }
                                              
                                              NSString *picUrlString = [NSString stringWithFormat:@"%@?cn_user=%@&cn_pass=%@&action=event_picture&pic_uid=%@",
                                                                        [self getCalaosHost],
                                                                        [keychain stringForKey:@"calaos-cn-user"],
                                                                        [keychain stringForKey:@"calaos-cn-pass"],
                                                                        picUid];

                                              NSURL *picURL = [NSURL URLWithString:picUrlString];
                                              NSURLSessionDownloadTask *downloadTask = [session downloadTaskWithURL:picURL
                                                                                                  completionHandler:^(NSURL *temporaryFileLocation, NSURLResponse *response, NSError *error)
                                                                                        {
                                                                                            if (error != nil) {
                                                                                                NSLog(@"%@", error.localizedDescription);
                                                                                                completionHandler(attachment);
                                                                                                return;
                                                                                            }
                                                                                            
                                                                                            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;
                                                                                            if (httpResponse.statusCode != 200) {
                                                                                                NSLog(@"Pic request Error, httpResponse.statusCode = %ld", (long)httpResponse.statusCode);
                                                                                                completionHandler(nil);
                                                                                                return;
                                                                                            }
                                                                                            
                                                                                            NSFileManager *fileManager = [NSFileManager defaultManager];
                                                                                            NSURL *localURL = [NSURL fileURLWithPath:[temporaryFileLocation.path stringByAppendingString:@".gif"]];
                                                                                            [fileManager moveItemAtURL:temporaryFileLocation toURL:localURL error:&error];
                                                                                            
                                                                                            NSError *attachmentError = nil;
                                                                                            attachment = [UNNotificationAttachment attachmentWithIdentifier:@"" URL:localURL options:nil error:&attachmentError];
                                                                                            if (attachmentError) {
                                                                                                NSLog(@"%@", attachmentError.localizedDescription);
                                                                                            }
                                                                                            
                                                                                            completionHandler(attachment);
                                                                                        }];
                                              [downloadTask resume];
                                          }
                                          else
                                          {
                                              NSLog(@"Event request Error, httpResponse.statusCode = %ld", (long)httpResponse.statusCode);
                                              completionHandler(nil);
                                          }
                                      }];
    [dataTask resume];
}

//Returns the new unread count. Best effort: without the lock (container
//unavailable, or the app's reset gave up waiting) or with cfprefsd reordering
//writes, a race can lose one increment; a kill after the increment but before
//delivery counts one too many. Either way the badge is off by one until the
//app resets it.
- (NSInteger)incrementBadgeCount {
    int fd = -1;
    NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:kAppGroup];
    if (container != nil) {
        NSString *lockPath = [container URLByAppendingPathComponent:kBadgeLockFile].path;
        fd = open(lockPath.fileSystemRepresentation, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (fd >= 0)
            flock(fd, LOCK_EX);
    }

    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    NSInteger count = MAX([sharedDefaults integerForKey:kBadgeCountKey], 0) + 1;
    [sharedDefaults setInteger:count forKey:kBadgeCountKey];

    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
    return count;
}

//Written by the app (HardwareUtils_iOS::setSessionActive()): false after an
//explicit logout, absent (= active) before the first logout or after a reset.
- (BOOL)isSessionActive {
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    if ([sharedDefaults objectForKey:@"calaos.session_active"] == nil)
        return YES;
    return [sharedDefaults boolForKey:@"calaos.session_active"];
}

- (NSString *)getCalaosHost {
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    
    NSString *h = [sharedDefaults stringForKey:@"calaos.hostname"];
    if ([h hasPrefix:@"http://"] || [h hasPrefix:@"https://"]) {
        return h;
    }
    else if ([h hasPrefix:@"ws://"] || [h hasPrefix:@"wss://"]) {
        return [h stringByReplacingOccurrencesOfString:@"ws" withString:@"http"];
    }
    
    return [NSString stringWithFormat:@"https://%@/api", h];
}

@end

