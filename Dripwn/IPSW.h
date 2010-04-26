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
- (BOOL) decodeFirmware;
- (BOOL) unzipIPSW:(NSString *) path;
- (BOOL) decryptImage;
- (BOOL) mountImage;
- (BOOL) unmountImage;
- (NSString *) selectIPSW;

@end
