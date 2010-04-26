/*
 * CocoaCryptoHashing.h
 * CocoaCryptoHashing
 */

#import <Foundation/Foundation.h>

@interface NSString (CocoaCryptoHashing)

/*!
 * @method md5Hash
 * @abstract Calculates the MD5 hash from the UTF-8 representation of the specified string  and returns the binary representation
 * @result A NSData object containing the binary representation of the MD5 hash
 */
- (NSData *)md5Hash;

/*!
 * @method md5HexHash
 * @abstract Calculates the MD5 hash from the UTF-8 representation of the specified string and returns the hexadecimal representation
 * @result A NSString object containing the hexadecimal representation of the MD5 hash
 */
- (NSString *)md5HexHash;

/*!
 * @method sha1Hash
 * @abstract Calculates the SHA-1 hash from the UTF-8 representation of the specified string  and returns the binary representation
 * @result A NSData object containing the binary representation of the SHA-1 hash
 */
- (NSData *)sha1Hash;

/*!
 * @method sha1HexHash
 * @abstract Calculates the SHA-1 hash from the UTF-8 representation of the specified string and returns the hexadecimal representation
 * @result A NSString object containing the hexadecimal representation of the SHA-1 hash
 */
- (NSString *)sha1HexHash;

@end

@interface NSData (CocoaCryptoHashing)

/*!
 * @method md5Hash
 * @abstract Calculates the MD5 hash from the data in the specified NSData object  and returns the binary representation
 * @result A NSData object containing the binary representation of the MD5 hash
 */
- (NSData *)md5Hash;

/*!
 * @method md5HexHash
 * @abstract Calculates the MD5 hash from the data in the specified NSData object and returns the hexadecimal representation
 * @result A NSString object containing the hexadecimal representation of the MD5 hash
 */
- (NSString *)md5HexHash;

/*!
 * @method sha1Hash
 * @abstract Calculates the SHA-1 hash from the data in the specified NSData object  and returns the binary representation
 * @result A NSData object containing the binary representation of the SHA-1 hash
 */
- (NSData *)sha1Hash;

/*!
 * @method sha1HexHash
 * @abstract Calculates the SHA-1 hash from the data in the specified NSData object and returns the hexadecimal representation
 * @result A NSString object containing the hexadecimal representation of the SHA-1 hash
 */
- (NSString *)sha1HexHash;

@end
