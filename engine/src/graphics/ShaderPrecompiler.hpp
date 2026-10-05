#pragma once

#include <string>
#include <vector>

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {

// Compiles shader sources of the active backend on the I/O pool ahead of the programs that the frame thread makes from them. The Metal backend compiles the source of every stage when its program is made, a tenth of a second or more the first time, and the system keeps each result in its compiler cache, where a program made while its sources still compile waits only for what is left. The other backends compile when the program is made, which no other thread can do for them, so they start nothing. Apple platforms compile in `ShaderPrecompiler.mm` and the others build `ShaderPrecompiler.cpp`.
class ShaderPrecompiler final {
  public:
    static void start(core::JobSystem& jobs, std::vector<std::string> sources);
};

} // namespace haylen::graphics
