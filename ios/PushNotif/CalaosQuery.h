//
//  CalaosQuery.h
//  PushNotif
//

#import <Foundation/Foundation.h>

//Query URL of api.php with the credentials, the action and one extra key.
//Every value is percent-encoded; the host is used as is.
FOUNDATION_EXTERN NSString *CalaosQueryURL(NSString *host, NSString *user, NSString *pass,
                                           NSString *action, NSString *key, id value);
