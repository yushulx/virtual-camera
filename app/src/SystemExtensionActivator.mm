// SystemExtensionActivator.mm - macOS implementation: submits an
// OSSystemExtensionRequest for the bundled CMIO camera extension
// (VirtualCameraExtension.appex inside VirtualCamera.app/Contents/PlugIns).
//
// Requirements for an unsigned (ad-hoc signed) build:
//   sudo systemextensionsctl developer on
// and the app must be launched from Finder/Terminal with the .appex present.
// For distribution, sign both the app and the extension with a Developer ID
// (see macos/scripts/package.sh) - then no developer mode is needed.

#import <Foundation/Foundation.h>
#import <SystemExtensions/OSSystemExtensionRequest.h>

@interface VCamSystemExtensionDelegate
    : NSObject <OSSystemExtensionRequestDelegate>
@property (copy) void (^completion)(BOOL ok, NSString* message);
@end

@implementation VCamSystemExtensionDelegate

- (void)requestNeedsUserApproval:(OSSystemExtensionRequest*)request
{
    NSLog(@"[VirtualCamera] system extension needs user approval");
}

- (void)request:(OSSystemExtensionRequest*)request
    didFinishWithResult:(OSSystemExtensionRequestResult)result
{
    BOOL ok = (result == OSSystemExtensionRequestResultCompleted);
    NSString* message =
        ok ? @"Camera extension installed. Select \"Virtual Camera\" in your apps."
           : @"The system extension request did not complete.";
    if (self.completion) {
        self.completion(ok, message);
    }
}

- (void)request:(OSSystemExtensionRequest*)request didFailWithError:(NSError*)error
{
    NSString* message =
        [NSString stringWithFormat:@"Camera extension activation failed: %@",
                                   error.localizedDescription];
    if (self.completion) {
        self.completion(NO, message);
    }
}

@end

extern "C" void vcamMacActivateSystemExtension(
    const char* identifier, void (*reply)(int, const char*, void*), void* userData)
{
    NSString* nsIdentifier =
        [NSString stringWithUTF8String:identifier ? identifier : ""];
    VCamSystemExtensionDelegate* delegate = [[VCamSystemExtensionDelegate alloc] init];
    delegate.completion = ^(BOOL ok, NSString* message) {
        reply(ok ? 1 : 0, [message UTF8String], userData);
    };

    OSSystemExtensionRequest* request =
        [OSSystemExtensionRequest activationRequestForExtension:nsIdentifier
                                                            queue:dispatch_get_main_queue()];
    request.delegate = delegate;
    [request submit];
}
