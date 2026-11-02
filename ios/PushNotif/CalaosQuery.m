//
//  CalaosQuery.m
//  PushNotif
//

#import "CalaosQuery.h"

//Unreserved characters of RFC 3986, spelled out in ASCII: the same set
//QUrl::toPercentEncoding() keeps in the app.
static NSString *const kUnreserved = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";

static NSString *encodedValue(id value)
{
    static NSCharacterSet *allowed;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        allowed = [NSCharacterSet characterSetWithCharactersInString:kUnreserved];
    });
    //pic_uid comes from JSON and may be a number, which %@ used to print.
    NSString *text = [value isKindOfClass:[NSString class]] ? value : (value ? [value description] : @"");
    return [text stringByAddingPercentEncodingWithAllowedCharacters:allowed] ?: @"";
}

NSString *CalaosQueryURL(NSString *host, NSString *user, NSString *pass,
                         NSString *action, NSString *key, id value)
{
    return [NSString stringWithFormat:@"%@?cn_user=%@&cn_pass=%@&action=%@&%@=%@",
            host, encodedValue(user), encodedValue(pass), encodedValue(action), key, encodedValue(value)];
}
