from haylen.toolchains.path_tool import PathTool


class BuildTools:
    """The build tools of the path that every build of the engine and of native apps runs."""

    CMAKE = PathTool("cmake", "Install CMake 3.28 or newer from `https://cmake.org/download` or a package manager.")
    CTEST = PathTool("ctest", "It comes with CMake, so install CMake 3.28 or newer from `https://cmake.org/download` or a package manager.")
    NINJA = PathTool("ninja", "Install Ninja from `https://ninja-build.org` or a package manager.")
    CLANG_FORMAT = PathTool("clang-format", 'Install the clang-format of LLVM, such as with "brew install clang-format" or "apt install clang-format".')
    PERL = PathTool("perl", "Install Strawberry Perl from `https://strawberryperl.com`, with which OpenSSL configures its build on Windows.")
    NASM = PathTool("nasm", "Install NASM, which Strawberry Perl carries in its folder `c/bin` and which assembles OpenSSL on Windows, and put its folder on the path.")
    UV = PathTool("uv", "Install uv from `https://docs.astral.sh/uv`, which runs the tooling and which the engine tests run the tooling with.")
