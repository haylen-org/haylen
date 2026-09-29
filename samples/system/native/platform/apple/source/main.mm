#import "HaylenApp-Swift.h"
#import "haylen/platform/apple/HaylenMain.h"

// Registers the Swift handlers of the sample before the runtime starts.
int main(int argc, char* argv[]) {
    [NativeSampleHandlers registerHandlers];
    return haylen_main(argc, argv);
}
