#include "platform/web/WebMain.hpp"

#include <emscripten/emscripten.h>

#include "platform/Services.hpp"
#include "platform/sdl/Runtime.hpp"
#include "platform/web/WebServices.hpp"
#include "platform/worker/Runtime.hpp"

// clang-format off
EM_JS(int, haylen_js_hosted_in_worker, (), {
    return Module.haylen.hosting === "worker" ? 1 : 0;
});
// clang-format on

namespace haylen::platform {

int WebMain::run(int argc, char* argv[]) {
    Services::install(WebServices::create());

    if (haylen_js_hosted_in_worker() != 0) {
        return worker::Runtime::run(argc, argv);
    }

    return sdl::Runtime::run(argc, argv);
}

} // namespace haylen::platform

int main(int argc, char* argv[]) {
    return haylen::platform::WebMain::run(argc, argv);
}
