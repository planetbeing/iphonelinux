//
//  IPSW.m
//  Dripwn
//
//  Created by James Munnelly on 25/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import "IPSW.h"
#import "CocoaCryptoHashing.h"

@implementation IPSW


- (BOOL) startExtraction:(id) sender {
	NSString *ipsw = [self selectIPSW];
	if(!ipsw)
		return NO;
	NSString *resourceBundle = [[NSBundle mainBundle] pathForResource:[ipsw lastPathComponent] ofType:@"plist"];
	if(!resourceBundle) {
		NSLog(@"Invalid file");
		NSAlert *alert = [NSAlert alertWithMessageText:@"Invalid file" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Invalid file selected. You must use an iPhone IPSW firmware version 3.1 or above."];
		[alert runModal];
		return NO;
	}
	NSString *saveFolder = [self chooseFolder];
	NSDictionary *dict = [NSDictionary dictionaryWithContentsOfFile:resourceBundle];	
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:[[NSBundle mainBundle] pathForResource:@"dripwn" ofType:@""]];
	[cmnd setArguments:[NSArray arrayWithObjects:ipsw,[dict objectForKey:@"RootFilesystemKey"],[dict objectForKey:@"RootFilesystem"],nil]];
	[cmnd setCurrentDirectoryPath:saveFolder];
	[cmnd launch];
	[cmnd waitUntilExit];
	
	if ([cmnd terminationStatus] != 0)
	{
		return NO;
	}
	[cmnd release];
	
	NSAlert *alert = [NSAlert alertWithMessageText:@"Complete" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Succesfully extracted Zephyr firmware files."];
	[alert runModal];
	return YES;
}

- (NSString *) selectIPSW {
	NSOpenPanel* openDlg = [NSOpenPanel openPanel];
	[openDlg setCanChooseFiles:YES];
	[openDlg setCanChooseDirectories:NO];
	[openDlg setAllowsMultipleSelection:NO];
	
	if ([openDlg runModalForDirectory:nil file:nil] == NSOKButton)
	{
		NSArray* files = [openDlg filenames];
		return [files objectAtIndex:0];
	}
	return nil;
}

- (NSString *) chooseFolder {
	NSOpenPanel* saveDlg = [NSOpenPanel openPanel];
	[saveDlg setTitle:@"Save to..."];
	[saveDlg setMessage:@"Select where to save the Zephyr firmware files."];
	[saveDlg setCanChooseDirectories:YES];
	[saveDlg setAllowsMultipleSelection:NO];
	
	if ([saveDlg runModalForDirectory:nil file:nil] == NSOKButton)
	{
		NSArray* files = [saveDlg filenames];
		return [files objectAtIndex:0];
	}
	return nil;
}

@end
