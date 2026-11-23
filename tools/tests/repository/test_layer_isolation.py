import re

from tests.support.engine_sources import EngineSources


class LayerIsolationTest:
    """The engine reaches SDL only through its platform layer and each graphics API only through its GPU device, so the Lua API never changes with the platform or the graphics API that draws."""

    def test_only_the_platform_layer_of_sdl_reaches_sdl(self):
        library = re.compile(r'#\s*(include|import)\s*[<"]SDL3/|\bSDL_\w+\s*\(')
        reaching = [name for name, text in EngineSources.read().items() if not re.match(r"engine/(src|tests)/platform/sdl/", name) and library.search(text)]
        assert reaching == []

    def test_only_the_gpu_devices_include_the_headers_of_their_graphics_apis(self):
        api = re.compile(r'#\s*(include|import)\s*[<"](Metal/|QuartzCore/CAMetalLayer|d3d1[12]|dxgi|vulkan/|volk\.h|GLES\d?/|GL/|webgpu/)')
        including = [name for name, text in EngineSources.read().items() if not re.match(r"engine/(src|tests)/gpu/", name) and api.search(text)]
        assert including == []

    def test_the_lua_bindings_know_no_host(self):
        binding = re.compile(r"engine/(lua/|src/(lua/|.*Lua\.(cpp|hpp)$))")
        host = re.compile(r'#\s*include\s*"(gpu/NullDevice|platform/(NativeHost|NativeWindow|Player|Services)\.hpp|platform/(sdl|worker|apple|android|web|windows|linux|posix|headless)/)')
        knowing = [name for name, text in EngineSources.read().items() if binding.match(name) and host.search(text)]
        assert knowing == []
