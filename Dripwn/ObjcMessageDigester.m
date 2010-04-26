#import "ObjcMessageDigester.h"
#import <openssl/evp.h>

@implementation ObjcMessageDigester

+ (NSData *)digestData:(NSData *)someData {
    id digester = [[[self alloc] init] autorelease];
    [digester updateDigest:someData];
    return [digester finalizes];
}

+ (const void *)type {
    return EVP_md_null();
}

- (id)init {
    self = [super init];
    if (self) {
        _context = EVP_MD_CTX_create();
        if (_context == NULL) {
            [self release];
            return nil;
        }
        [self reset];
    }
    return self;
}

- (void)dealloc {
    EVP_MD_CTX_destroy(_context);
    [super dealloc];
}

- (void)updateDigest:(NSData *)someData {
    if (EVP_DigestUpdate(_context, [someData bytes], [someData length]) != 1) {
        [NSException raise:NSGenericException format:@"Unable to update digest."];
    }
}

- (NSData *)finalizes {
    NSMutableData *md = [NSMutableData dataWithLength:EVP_MAX_MD_SIZE];
    unsigned int l = 0;
    EVP_DigestFinal_ex(_context, [md mutableBytes], &l);
    [md setLength:l];
    return md;
}

- (void)reset {
    if (EVP_DigestInit_ex(_context, [[self class] type], NULL) != 1) {
        [NSException raise:NSGenericException format:@"Unable to initialize digest context."];
    }
}

@end

@implementation ObjcMD5MessageDigester

+ (const void *)type {
    return EVP_md5();
}

@end
