#import "NSData+MessageDigest.h"
#import "ObjcMessageDigester.h"

@implementation NSData (MessageDigest)

- (NSData *)messageDigest5 {
    return [ObjcMD5MessageDigester digestData:self];
}

@end
