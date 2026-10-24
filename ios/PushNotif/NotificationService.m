//
//  NotificationService.m
//  PushNotif
//
//  Created by Raoul Hecky on 22/02/2018.
//

#import "NotificationService.h"
#import <CommonCrypto/CommonDigest.h>
#import <Security/Security.h>
#import <fcntl.h>
#import <sys/file.h>
#import <unistd.h>
#import "ios/SimpleKeychain/A0SimpleKeychain.h"
#import "ios/SimpleKeychain/A0SimpleKeychain+KeyPair.h"

@interface NotificationService () <NSURLSessionTaskDelegate>

@property (nonatomic, strong) void (^contentHandler)(UNNotificationContent *contentToDeliver);
@property (nonatomic, strong) UNMutableNotificationContent *bestAttemptContent;
@property (nonatomic, strong) NSURLSession *session;

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
//host:port -> SHA-256 hex of the SubjectPublicKeyInfo the user confirmed in
//the app (src/TlsPinning.h), written by HardwareUtils_iOS::setTlsPins().
static NSString *const kTlsPinsKey = @"calaos.tls_pins";

//One DER TLV header at pos, same walk as TlsPinning::subjectPublicKeyInfo():
//at most 3 length bytes, bounds written so that nothing can overflow.
static BOOL readTlv(const uint8_t *d, NSUInteger size, NSUInteger pos,
                    uint8_t *tag, NSUInteger *hdr, NSUInteger *len)
{
    if (pos >= size || size - pos < 2)
        return NO;
    *tag = d[pos];
    NSUInteger l = d[pos + 1];
    *hdr = 2;
    if (l & 0x80) {
        NSUInteger n = l & 0x7f;
        if (n == 0 || n > 3 || size - pos - 2 < n)
            return NO;
        l = 0;
        for (NSUInteger i = 0; i < n; i++)
            l = (l << 8) | d[pos + 2 + i];
        *hdr += n;
    }
    if (l > size - pos - *hdr)
        return NO;
    *len = l;
    return YES;
}

//SubjectPublicKeyInfo of a DER certificate, nil when it does not parse.
static NSData *subjectPublicKeyInfo(NSData *der)
{
    const uint8_t *d = der.bytes;
    NSUInteger size = der.length, pos = 0, hdr = 0, len = 0;
    uint8_t tag = 0;

    if (!readTlv(d, size, pos, &tag, &hdr, &len) || tag != 0x30)
        return nil;
    pos = hdr;
    if (!readTlv(d, size, pos, &tag, &hdr, &len) || tag != 0x30)
        return nil;
    pos += hdr;
    NSUInteger tbsEnd = pos + len;

    if (!readTlv(d, size, pos, &tag, &hdr, &len))
        return nil;
    if (tag == 0xa0) {
        pos += hdr + len;
        if (!readTlv(d, size, pos, &tag, &hdr, &len))
            return nil;
    }

    //serialNumber, signature, issuer, validity, subject
    const uint8_t skipped[] = { 0x02, 0x30, 0x30, 0x30, 0x30 };
    for (size_t i = 0; i < sizeof(skipped); i++) {
        if (!readTlv(d, size, pos, &tag, &hdr, &len) || tag != skipped[i])
            return nil;
        pos += hdr + len;
    }

    if (!readTlv(d, size, pos, &tag, &hdr, &len) || tag != 0x30 || pos + hdr + len > tbsEnd)
        return nil;
    return [der subdataWithRange:NSMakeRange(pos, hdr + len)];
}

static NSString *leafSpkiSha256(SecTrustRef trust)
{
    CFArrayRef chain = SecTrustCopyCertificateChain(trust);
    if (chain == NULL)
        return nil;
    NSString *hex = nil;
    if (CFArrayGetCount(chain) > 0) {
        SecCertificateRef leaf = (SecCertificateRef)CFArrayGetValueAtIndex(chain, 0);
        NSData *der = (__bridge_transfer NSData *)SecCertificateCopyData(leaf);
        NSData *spki = subjectPublicKeyInfo(der);
        if (spki != nil) {
            unsigned char digest[CC_SHA256_DIGEST_LENGTH];
            CC_SHA256(spki.bytes, (CC_LONG)spki.length, digest);
            NSMutableString *s = [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH * 2];
            for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++)
                [s appendFormat:@"%02x", digest[i]];
            hex = s;
        }
    }
    CFRelease(chain);
    return hex;
}

//Same key as TlsPinning::hostKey(): lowercase host, IPv6 in brackets,
//explicit port.
static NSString *tlsHostKey(NSString *host, NSInteger port)
{
    NSString *h = host.lowercaseString;
    if ([h containsString:@":"] && ![h hasPrefix:@"["])
        h = [NSString stringWithFormat:@"[%@]", h];
    return [NSString stringWithFormat:@"%@:%ld", h, (long)(port > 0 ? port : 443)];
}

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
    [self contentComplete];
}

//Delivers the content once: after serviceExtensionTimeWillExpire, the
//download completion (another queue) may still come back.
- (void)contentComplete {
    void (^handler)(UNNotificationContent *) = nil;
    @synchronized (self) {
        handler = self.contentHandler;
        self.contentHandler = nil;
    }
    if (handler) {
        //The session retains its delegate (self) until invalidated.
        [self.session finishTasksAndInvalidate];
        handler(self.bestAttemptContent);
    }
}

//The urls carry the credentials: never follow a redirect, which could take
//them to another host.
- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request
 completionHandler:(void (^)(NSURLRequest *))completionHandler {
    completionHandler(nil);
}

//A certificate the system trusts for this host is used as is. Anything else
//needs the key pinned by the app for this host:port; the extension cannot
//ask the user, so without a matching pin the picture is skipped and the
//notification shows its text only.
- (void)URLSession:(NSURLSession *)session didReceiveChallenge:(NSURLAuthenticationChallenge *)challenge
 completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential *))completionHandler {
    NSURLProtectionSpace *space = challenge.protectionSpace;
    if (![space.authenticationMethod isEqualToString:NSURLAuthenticationMethodServerTrust]) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }

    SecTrustRef trust = space.serverTrust;
    if (trust == NULL) {
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
        return;
    }
    NSString *key = tlsHostKey(space.host, space.port);
    NSUserDefaults *sharedDefaults = [[NSUserDefaults alloc] initWithSuiteName:kAppGroup];
    id stored = [sharedDefaults dictionaryForKey:kTlsPinsKey][key];
    NSString *pin = ([stored isKindOfClass:[NSString class]] && [(NSString *)stored length] > 0) ? stored : nil;
    NSString *peer = leafSpkiSha256(trust);
    BOOL pinMatches = pin != nil && peer != nil && [pin caseInsensitiveCompare:peer] == NSOrderedSame;

    //A pinned host keeps its key even behind a certificate the system
    //trusts: a mis-issued one would be trusted too.
    if (SecTrustEvaluateWithError(trust, NULL) && (pin == nil || pinMatches)) {
        completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, nil);
        return;
    }
    if (pinMatches) {
        completionHandler(NSURLSessionAuthChallengeUseCredential, [NSURLCredential credentialForTrust:trust]);
        return;
    }

    NSLog(@"PushNotif: certificate of %@ is not trusted or not the pinned key (SPKI sha256 %@), no picture", key, peer);
    completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, nil);
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
    
    NSURLSession *session = [NSURLSession sessionWithConfiguration:[NSURLSessionConfiguration defaultSessionConfiguration]
                                                          delegate:self
                                                     delegateQueue:nil];
    self.session = session;
    
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

