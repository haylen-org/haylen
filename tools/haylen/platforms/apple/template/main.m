#import "haylen/platform/apple/HaylenMain.h"

// Starts the app with the package that the project copies to the folder "app" of its resources. The native bridge handlers of the app and its plugins register before this, when their classes load.
int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
