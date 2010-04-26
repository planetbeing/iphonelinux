//
//  DripwnAppDelegate.h
//  Dripwn
//
//  Created by James Munnelly on 23/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "IPSW.h"

@interface DripwnAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow *window;
	IPSW *ipsw;
}

@property (assign) IBOutlet NSWindow *window;
- (IBAction) startExtraction:(id) sender;

@end
