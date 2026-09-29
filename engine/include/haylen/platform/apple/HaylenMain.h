#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Runs the Haylen runtime, which plays the package named on the command line or the one bundled with the app. The main function of an Apple app calls it after registering its native HaylenBridge handlers.
int haylen_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif
