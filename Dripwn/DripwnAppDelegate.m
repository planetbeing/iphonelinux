//
//  DripwnAppDelegate.m
//  Dripwn
//
//  Created by James Munnelly on 23/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import "DripwnAppDelegate.h"
#import "IPSW.h"

@implementation DripwnAppDelegate

@synthesize window;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	// Insert code here to initialize your application 
}

- (IBAction) startExtraction:(id) sender {
	ipsw = [[IPSW alloc] init];
	[ipsw startExtraction:sender];
	[ipsw release];
}

- (void) dealloc {
	if(ipsw)
		[ipsw release];
	[super dealloc];
}

@end
