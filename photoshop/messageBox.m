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
    self = [super initWithContentRect:NSMakeRect(0, 0, 0, 0)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self)
    {
        [self setMinSize:NSMakeSize(0, 0)];
        [self setMaxSize:NSMakeSize(0, 0)];
        _result = 0;
    }
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
    (void)parent;

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = [NSString stringWithUTF8String:title];
    alert.informativeText = [NSString stringWithUTF8String:message];
    [alert addButtonWithTitle:okText ? [NSString stringWithUTF8String:okText] : @"OK"];
    if (cancelText)
        [alert addButtonWithTitle:[NSString stringWithUTF8String:cancelText]];

    NSModalResponse result = [alert runModal];
	if (result == NSAlertFirstButtonReturn)
		return kMsgBox_YES;
	if (result == NSAlertSecondButtonReturn)
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
