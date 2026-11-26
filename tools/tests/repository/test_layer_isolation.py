import re

from tests.support.engine_sources import EngineSources


class LayerIsolationTest:
    """The engine reaches SDL only through its platform layer and each graphics API only through its GPU device, so the Lua API never changes with the platform or the graphics API that draws, Recast and Detour stay behind the navigation of 3D, the 3D node tree reaches the other contexts of 3D only through component types, and 2D, 3D and the GPU layers under them stay apart, so each dimension builds on the GPU layers alone and never on the other."""

    BACKEND = re.compile(r"engine/(src|tests)/3d/physics/jolt/")
    JOLT = re.compile(r"#\s*include\s*<Jolt/")
    BACKEND_HEADER = re.compile(r'#\s*include\s*"(3d/physics/jolt/\w+\.hpp)"')

    def test_only_the_platform_layer_of_sdl_reaches_sdl(self):
        library = re.compile(r'#\s*(include|import)\s*[<"]SDL3/|\bSDL_\w+\s*\(')
        reaching = [name for name, text in EngineSources.read().items() if not re.match(r"engine/(src|tests)/platform/sdl/", name) and library.search(text)]
        assert reaching == []

    def test_only_the_gpu_devices_include_the_headers_of_their_graphics_apis(self):
        api = re.compile(r'#\s*(include|import)\s*[<"](Metal/|QuartzCore/CAMetalLayer|d3d1[12]|dxgi|vulkan/|volk\.h|GLES\d?/|GL/|webgpu/)')
        including = [name for name, text in EngineSources.read().items() if not re.match(r"engine/(src|tests)/gpu/", name) and api.search(text)]
        assert including == []

    def test_the_lua_bindings_know_no_host(self):
        binding = re.compile(r"engine/lua/(include|src)/")
        host = re.compile(r'#\s*include\s*"(gpu/NullDevice|platform/(NativeHost|NativeWindow|Player|Services)\.hpp|platform/(sdl|worker|apple|android|web|windows|linux|posix|headless)/)')
        knowing = [name for name, text in EngineSources.read().items() if binding.match(name) and host.search(text)]
        assert knowing == []

    def test_2d_and_3d_never_depend_on_each_other(self):
        folder = re.compile(r"engine/(?:include/haylen|src|tests|lua/src|lua/tests)/([23]d)/")
        crossing = []
        for name, text in EngineSources.read().items():
            found = folder.match(name)
            if not found:
                continue
            other = "3d" if found.group(1) == "2d" else "2d"
            if re.search(rf'#\s*include\s*"(?:haylen/)?{other}/', text):
                crossing.append(name)
        assert crossing == [], f"These files of one dimension include a header of the other: {sorted(crossing)}."

    def test_gpu_and_graphics_depend_on_no_dimension(self):
        folder = re.compile(r"engine/(?:include/haylen|src)/(?:gpu|graphics)/")
        dimension = re.compile(r'#\s*include\s*"(?:haylen/)?[23]d/')
        depending = sorted(name for name, text in EngineSources.read().items() if folder.match(name) and dimension.search(text))
        assert depending == [], f"These files of the GPU devices or of graphics include a header of 2D or 3D: {depending}."

    def test_the_3d_renderer_depends_on_no_physics_or_navigation(self):
        folder = re.compile(r"engine/(?:include/haylen|src)/3d/graphics/")
        simulation = re.compile(r'#\s*include\s*"(?:haylen/)?3d/(?:physics|navigation)/')
        depending = sorted(name for name, text in EngineSources.read().items() if folder.match(name) and simulation.search(text))
        assert depending == [], f"These files of the 3D renderer include a header of physics or navigation: {depending}."

    def test_the_3d_node_tree_names_no_other_3d_context(self):
        # The contexts of 3D reach the node tree through its component types, so the tree itself never includes one of them.
        folder = re.compile(r"engine/(?:include/haylen|src)/3d/nodes/")
        other = re.compile(r'#\s*include\s*"(?:haylen/)?3d/(?!nodes/)')
        depending = sorted(name for name, text in EngineSources.read().items() if folder.match(name) and other.search(text))
        assert depending == [], f"These files of the 3D node tree include a header of another 3D context: {depending}."

    def test_only_the_private_parts_of_3d_navigation_reach_recast_and_detour(self):
        library = re.compile(r'#\s*include\s*[<"](?:Recast|Detour)\w*\.h[>"]')
        reaching = sorted(name for name, text in EngineSources.read().items() if not name.startswith("engine/src/3d/navigation/") and library.search(text))
        assert reaching == [], f"These files outside the private parts of 3D navigation include a header of Recast or Detour: {reaching}."

    def test_only_the_jolt_backend_reaches_the_headers_of_jolt(self):
        """The headers of Jolt hold inline SIMD code, so only the backend of Jolt and its tests include them, and the headers of the backend that the rest of the engine includes reach nothing of Jolt."""
        sources = EngineSources.read()
        including = sorted(name for name, text in sources.items() if not LayerIsolationTest.BACKEND.match(name) and LayerIsolationTest.JOLT.search(text))
        assert including == [], f"These files outside the backend of Jolt include a header of Jolt: {including}."

        outside = [text for name, text in sources.items() if not LayerIsolationTest.BACKEND.match(name)]
        reached = LayerIsolationTest.reach_backend_headers(sources, outside)
        reaching = sorted(name for name in reached if LayerIsolationTest.JOLT.search(sources[name]))
        assert reaching == [], f"These headers of the backend of Jolt, which the rest of the engine includes, include a header of Jolt: {reaching}."

    @staticmethod
    def reach_backend_headers(sources: dict[str, str], texts: list[str]) -> set[str]:
        """The headers of the backend of Jolt that the texts include, directly or through other headers of the backend."""
        reached: set[str] = set()
        pending = [path for text in texts for path in LayerIsolationTest.BACKEND_HEADER.findall(text)]
        while pending:
            name = f"engine/src/{pending.pop()}"
            if name in reached or name not in sources:
                continue
            reached.add(name)
            pending.extend(LayerIsolationTest.BACKEND_HEADER.findall(sources[name]))
        return reached

    def test_the_public_headers_of_xr_include_no_platform_framework(self):
        framework = re.compile(r'#\s*(?:include|import)\s*[<"](?:SwiftUI/|ARKit/|RealityKit/|CompositorServices/|jni\.h|openxr/)')
        including = sorted(name for name, text in EngineSources.read().items() if name.startswith("engine/include/haylen/xr/") and framework.search(text))
        assert including == [], f"These public headers of XR include a framework of a platform: {including}."
