//
//  IPSW.m
//  Dripwn
//
//  Created by James Munnelly on 25/04/2010.
//  Copyright 2010 JamWare. All rights reserved.
//

#import "IPSW.h"
#import "NSData+Base64.h"
#import "CocoaCryptoHashing.h"

@implementation IPSW


- (BOOL) startExtraction:(id) sender {
	NSString *ipsw = [self selectIPSW];
	if(!ipsw)
		return NO;
	NSString *resourceBundle = [[NSBundle mainBundle] pathForResource:[ipsw lastPathComponent] ofType:@"plist"];
	if(!resourceBundle) {
		NSLog(@"Invalid file");
		NSAlert *alert = [NSAlert alertWithMessageText:@"Invalid file" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Invalid file selected. You must use an iPhone 2G IPSW firmware version 3.1 or above."];
		[alert runModal];
		return NO;
	}
	NSDictionary *dict = [NSDictionary dictionaryWithContentsOfFile:resourceBundle];
	NSString *sha1key = [dict objectForKey:@"SHA1"];
	
	NSData *dta = [NSData dataWithContentsOfFile:ipsw];
	NSString *digest = [dta sha1HexHash];
	
	
	if(![digest isEqualToString:sha1key]) {
		NSLog(@"Corrupt file");
		NSAlert *alert = [NSAlert alertWithMessageText:@"Damage file" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"You seem to have the correct filename, but it is damaged. Try to redownload."];
		[alert runModal];
		return NO;
	}
	if(![self unzipIPSW:ipsw]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error unzipping" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Unknown error unzipping IPSW file."];
		[alert runModal];
		return NO;
	}
	if(![self decryptImage:[dict objectForKey:@"RootFilesystem"] andKey:[dict objectForKey:@"RootFilesystemKey"]]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error decrypting image" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"That's interesting, this shouldn't be possible. Perhaps the wrong architecture."];
		[alert runModal];
		return NO;
	}
	if(![self mountImage]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Error mounting image" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"The root filesystem could not be mounted."];
		[alert runModal];
		return NO;
	}
	if(![self decodeFirmware:[dict objectForKey:@"RootFilesystemMountVolume"]]) {
		NSAlert *alert = [NSAlert alertWithMessageText:@"Couldn't decode firmware" defaultButton:@"Okay" alternateButton:nil otherButton:nil informativeTextWithFormat:@"Error decoding base64 firmware files!"];
		[alert runModal];
		return NO;
	}
	if(![self unmountImage:[dict objectForKey:@"RootFilesystemMountVolume"]]) {
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

- (BOOL) decodeFirmware:(NSString *) mountName {
	NSString *file = [NSString stringWithContentsOfFile:[NSString stringWithFormat:@"/Volumes/%@/usr/share/firmware/multitouch/iPhone.mtprops", mountName] encoding:NSASCIIStringEncoding error:nil];
	
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

- (BOOL) decryptImage:(NSString *) imageName andKey:(NSString *)key {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:[[NSBundle mainBundle] pathForResource:@"vfdecrypt" ofType:nil]];
	[cmnd setArguments:[NSArray arrayWithObjects:
						@"-i", [NSString stringWithFormat:@"/tmp/ipsw/%@", imageName], @"-o", @"/tmp/ipsw/decryptedfs.dmg", @"-k", key,nil]];
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

- (BOOL) unmountImage:(NSString *)mountName {
	NSTask *cmnd=[[NSTask alloc] init];
	[cmnd setLaunchPath:@"/usr/bin/hdiutil"];
	[cmnd setArguments:[NSArray arrayWithObjects: @"detach", [NSString stringWithFormat:@"/Volumes/%@", mountName],nil]];
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
