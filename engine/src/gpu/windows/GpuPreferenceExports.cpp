// The variables that the drivers of NVIDIA and AMD read from an executable on a laptop with two GPUs. They hold no class, since the drivers look them up by these exact names in the exports of the executable, and only an executable defines them, because a library that defines them is never asked. Every executable of the engine on Windows compiles this file. The variables start at 0, because the drivers read them when the process starts as well as when the first context of OpenGL opens and take the discrete GPU if either read finds 1, so only `GpuPreference::request` sets them, once the app has said which GPU it wants.
extern "C" {
// The driver of NVIDIA reads a `DWORD`, which is an `unsigned long` on Windows, so the file needs no header of the system.
__declspec(dllexport) unsigned long NvOptimusEnablement = 0;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
