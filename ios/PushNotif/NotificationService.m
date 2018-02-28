//
//  NotificationService.m
//  PushNotif
//
//  Created by Raoul Hecky on 22/02/2018.
//

#import "NotificationService.h"
#import "ios/SimpleKeychain/A0SimpleKeychain.h"
#import "ios/SimpleKeychain/A0SimpleKeychain+KeyPair.h"

@interface NotificationService ()

@property (nonatomic, strong) void (^contentHandler)(UNNotificationContent *contentToDeliver);
@property (nonatomic, strong) UNMutableNotificationContent *bestAttemptContent;

@end

@implementation NotificationService

- (void)didReceiveNotificationRequest:(UNNotificationRequest *)request withContentHandler:(void (^)(UNNotificationContent * _Nonnull))contentHandler {
    self.contentHandler = contentHandler;
    self.bestAttemptContent = [request.content mutableCopy];
    
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

- (NSString *)getCalaosHost {
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName: @"group.fr.calaos.CalaosMobile"];
    
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

