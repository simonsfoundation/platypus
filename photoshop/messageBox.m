#import <Cocoa/Cocoa.h>

enum
{
	kMsgBox_YES = 1,
	kMsgBox_NO
};

@interface TempWindow : NSWindow
{
    int _result;
}
@end

@implementation TempWindow

- (id)init
{
    self = [super init];
    [self setStyleMask:NSBorderlessWindowMask];
    [self setMinSize:NSMakeSize(0, 0)];
    [self setMaxSize:NSMakeSize(0, 0)];
    [self setFrame:NSMakeRect(0, 0, 0, 0) display:NO];
    _result = 0;
    return self;
}

- (void)doUserInterface:(NSString *)path arguments:(NSArray *)args
{
    
}

- (int)result
{
    return _result;
}

@end

int messageBox(void *parent, const char *title, const char *message, const char *okText, const char *cancelText)
{
	int result = NSRunAlertPanel([NSString stringWithUTF8String:title], 
			[NSString stringWithUTF8String:message],
			okText ? [NSString stringWithUTF8String:okText] : @"OK",	// default button
			cancelText ? [NSString stringWithUTF8String:cancelText] : nil,	// alternate button
			nil);

	if (result == NSAlertDefaultReturn)
		return kMsgBox_YES;
	if (result == NSAlertAlternateReturn)
		return kMsgBox_NO;

	return kMsgBox_YES;
}

int doUserInterface(CFStringRef command, CFArrayRef arguments)
{
    NSString *cmd = (__bridge NSString *)command;
    NSArray *args = (__bridge NSArray *)arguments;

    NSApplication *app = [NSApplication sharedApplication];
    TempWindow *win = [[TempWindow alloc] init];
    [win makeKeyAndOrderFront:nil];
    
    __block int result = 0;
    dispatch_queue_t taskQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0);
    dispatch_async(taskQueue, ^{
        NSTask *task = [NSTask launchedTaskWithLaunchPath:cmd arguments:args];
        [task waitUntilExit];
        result = [task terminationStatus];
        [app stopModal];
    });
    
    [app runModalForWindow:win];
    [win orderOut:nil];
    //[win release];
    return result;
}

void endUserInterface()
{
}