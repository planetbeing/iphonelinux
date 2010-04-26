//
//  IPSW.m
//  Dripwn
//
//  Created by James Munnelly on 25/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import "IPSW.h"
#import "NSData+Base64.h"
#import "NSData+MessageDigest.h"

@implementation IPSW


// TODO: Add MD5 hash checking for IPSW

- (BOOL) startExtraction:(id) sender {
	NSString *correctHash = @"5±Â&ÏinO²KèÓn";
	NSString *ipsw = [self selectIPSW];
	
	if(ipsw == nil)
		return NO;
	NSData *dta = [NSData dataWithContentsOfFile:ipsw];
	NSData *digest = [dta messageDigest5];
	
	unsigned char bytes[16];
	[digest getBytes:bytes length:16];
    NSString *h = [[NSString alloc] initWithBytes:bytes length:16 encoding:NSASCIIStringEncoding];
	if(![h isEqualToString:correctHash]) {
		NSLog(@"Invalid file");
		NSAlert *alert = [NSAlert alertWithMessageText:@"Invalid file" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Invalid file selected. You must use iPhone1,1_3.1_7C144_Restore.ipsw"];
		[alert runModal];
		return NO;
	}
	if(![self unzipIPSW:ipsw]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error unzipping" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Unknown error unzipping IPSW file."];
		[alert runModal];
		return NO;
	}
	if(![self decryptImage]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error decrypting image" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"That's interesting, this shouldn't be possible. Perhaps the wrong architecture."];
		[alert runModal];
		return NO;
	}
	if(![self mountImage]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error mounting image" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"The root filesystem could not be mounted."];
		[alert runModal];
		return NO;
	}
	if(![self decodeFirmware]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Couldn't decode firmware" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Error decoding base64 firmware files!"];
		[alert runModal];
		return NO;
	}
	if(![self unmountImage]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error unmounting image." defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"This isn't too much of a problem, your firmware files should already be created. Reboot to unmount IPSW."];
		[alert runModal];
		return NO;
	}
	[[NSFileManager defaultManager] removeItemAtPath:@"/tmp/ipsw/" error:nil];
	NSAlert *alert = [NSAlert alertWithMessageText:@"Done!" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Two firmware files, all ready for iDroids usage! I placed them on your desktop for you.."];
	[alert runModal];
	NSLog(@"Firmware succesfully decrypted!");
	return YES;
}

- (BOOL) decodeFirmware {
	NSString *file = [NSString stringWithContentsOfFile:@"/Volumes/Northstar7C144.iPhoneOS/usr/share/firmware/multitouch/iPhone.mtprops" encoding:NSASCIIStringEncoding error:nil];
	
	NSString *curDir = NSHomeDirectory();
	
	NSRange range = [file rangeOfString:@"<data>"];
	int start = range.location+range.length;
	NSRange r = [file rangeOfString:@"</data>"];
	int end = r.location;
	NSRange newRange;
	newRange.location = start;
	newRange.length = end-start;
	NSString *base64Encoded = [file substringWithRange:newRange];
	NSData *toWrite = [NSData dataWithBase64EncodedString:base64Encoded];
	[toWrite writeToFile:[NSString stringWithFormat:@"%@/Desktop/zephyr_aspeed.bin", curDir] atomically:YES];
	
	int newStart = newRange.location+newRange.length+7;
	file = [file substringFromIndex:newStart];
	range = [file rangeOfString:@"<data>"];
	start = range.location+range.length;
	r = [file rangeOfString:@"</data>"];
	end = r.location;
	newRange.location = start;
	newRange.length = end-start;
	base64Encoded = [file substringWithRange:newRange];
	toWrite = [NSData dataWithBase64EncodedString:base64Encoded];
	[toWrite writeToFile:[NSString stringWithFormat:@"%@/Desktop/zephyr_main.bin", curDir] atomically:YES];
	
	return YES;
}

- (BOOL) unzipIPSW:(NSString *) path {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:@"/usr/bin/ditto"];
	[cmnd setArguments:[NSArray arrayWithObjects:
						@"-v",@"-x",@"-k",@"--rsrc",path,@"/tmp/ipsw/",nil]];
	[cmnd launch];
	[cmnd waitUntilExit];
	
	// Handle the task's termination status
	if ([cmnd terminationStatus] != 0)
	{
		return NO;
	}
	
	// You *did* remember to wash behind your ears ...
	// ... right?
	[cmnd release];
	return YES;
}

- (BOOL) decryptImage {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:[[NSBundle mainBundle] pathForResource:@"vfdecrypt" ofType:nil]];
	[cmnd setArguments:[NSArray arrayWithObjects:
						@"-i", @"/tmp/ipsw/018-5343-086.dmg", @"-o", @"/tmp/ipsw/decryptedfs.dmg", @"-k", @"DBE476ED0D8C1ECF7CD514463F2CA5A6F71B6F244D98EBAA9203FD527C1ECBF2BB5F143F",nil]];
	[cmnd launch];
	[cmnd waitUntilExit];
	
	// Handle the task's termination status
	if ([cmnd terminationStatus] != 0)
	{
		return NO;
	}
	
	// You *did* remember to wash behind your ears ...
	// ... right?
	[cmnd release];
	return YES;
}

- (BOOL) mountImage {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:@"/usr/bin/hdiutil"];
	[cmnd setArguments:[NSArray arrayWithObjects: @"attach", @"-noverify", @"-nobrowse", @"-noautoopen", @"/tmp/ipsw/decryptedfs.dmg",nil]];
	[cmnd launch];
	[cmnd waitUntilExit];
	
	// Handle the task's termination status
	if ([cmnd terminationStatus] != 0)
	{
		return NO;
	}
	
	// You *did* remember to wash behind your ears ...
	// ... right?
	[cmnd release];
	return YES;
}

- (BOOL) unmountImage {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:@"/usr/bin/hdiutil"];
	[cmnd setArguments:[NSArray arrayWithObjects: @"detach", @"/Volumes/Northstar7C144.iPhoneOS",nil]];
	[cmnd launch];
	[cmnd waitUntilExit];
	
	// Handle the task's termination status
	if ([cmnd terminationStatus] != 0)
	{
		return NO;
	}
	
	// You *did* remember to wash behind your ears ...
	// ... right?
	[cmnd release];
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

@end
