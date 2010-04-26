//
//  IPSW.h
//  Dripwn
//
//  Created by James Munnelly on 25/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface IPSW : NSObject {

}

- (BOOL) startExtraction:(id) sender;
- (BOOL) decodeFirmware:(NSString *) mountName;
- (BOOL) unzipIPSW:(NSString *) path;
- (BOOL) mountImage;
- (BOOL) decryptImage:(NSString *) imageName andKey:(NSString *)key;
- (BOOL) unmountImage:(NSString *)mountName;
- (NSString *) selectIPSW;

@end
