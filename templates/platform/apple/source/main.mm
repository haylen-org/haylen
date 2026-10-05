#import "haylen/platform/apple/HaylenMain.h"

// Starts the app with the package that the project copies to Resources/app. Native platform bridge handlers of the app are registered here, with [HaylenBridge registerHandler:handler:] from haylen/platform/apple/HaylenBridge.h, before the runtime starts, and native code of the app sends events to the app with [HaylenBridge emit:payload:] at any time.
int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
