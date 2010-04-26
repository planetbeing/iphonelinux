#import <Foundation/Foundation.h>

@interface ObjcMessageDigester : NSObject {
    @private
    void *_context;
}

+ (NSData *)digestData:(NSData *)someData;

+ (const void *)type;

- (void)updateDigest:(NSData *)someData;
- (NSData *)finalizes;
- (void)reset;

@end

@interface ObjcMD5MessageDigester : ObjcMessageDigester
@end
