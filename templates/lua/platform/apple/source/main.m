#import "HaylenApp-Swift.h"
#import "haylen/platform/apple/HaylenMain.h"

// Starts the app with the package that the project copies to Resources/app. The file is Objective-C, whose modules are on, so the "@import" lines of the Swift header of the app bring the frameworks of the Swift files of the app and its plugins, which Objective-C++ without modules would skip. The native platform bridge handlers of the app register here, before the runtime starts, with [HaylenBridge registerHandler:handler:] of haylen/platform/apple/HaylenBridge.h or HaylenBridge.register of HaylenBridgeAsync.swift, and native code of the app sends events to the app at any time with [HaylenBridge emit:payload:].
int main(int argc, char* argv[]) {
    [AppBridge registerHandlers];
    return haylen_main(argc, argv);
}
