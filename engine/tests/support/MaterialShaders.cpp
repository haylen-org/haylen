#include "support/MaterialShaders.hpp"

#include <Poco/Process.h>

#include <stdexcept>

#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::test {

// Compiles the material `source` with `haylen app shaders`, the way apps compile theirs, with the shader compiler of the build. The tooling of the checkout runs through uv, which starts without a shell, so no shell reinterprets the quotes of the paths.
std::vector<std::uint8_t> MaterialShaders::compile(const std::string& name, const char* source) {
    const TemporaryDirectory app;
    app.write("haylen.json", R"({"name": "Shaders", "identifier": "com.haylen.shaders", "version": "1.0.0"})");
    app.write("content/shaders/" + name + ".glsl", source);
    const Poco::Process::Env env{{"HAYLEN_SHADER_COMPILER", HAYLEN_SHADER_COMPILER}};

    const std::vector<std::string> arguments{"run", "--project", HAYLEN_TOOLING_ROOT, "--frozen", "haylen", "app", "shaders", app.getPath().string()};
    if (Poco::Process::launch(HAYLEN_UV, arguments, "", nullptr, nullptr, nullptr, env).wait() != 0) {
        throw std::runtime_error("The command \"haylen app shaders\" failed.");
    }

    return TestFiles::read(app.getPath() / "content" / "shaders" / (name + ".shader"));
}

// The tint material is compiled once for every test that needs it.
const std::vector<std::uint8_t>& MaterialShaders::tint() {
    static const std::vector<std::uint8_t> compiled = compile("tint", kTintSource);
    return compiled;
}

} // namespace haylen::test
