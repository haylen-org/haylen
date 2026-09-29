#include "haylen/platform/apple/HaylenMain.h"

// Entry point of the Apple executables that CMake builds, such as the desktop player and haylen_add_app targets. Xcode projects made from the Apple template call haylen_main from their own main.mm.
int main(int argc, char* argv[]) {
    return haylen_main(argc, argv);
}
