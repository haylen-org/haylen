import subprocess

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree


class Benchmarks:
    """Builds a benchmark of the engine in Release in the tree of this machine and runs it here: the sprite benchmark in a window of the player, the benchmarks of the core, algorithms, procedural generation, physics, the UI, rendering, particles, content, nodes and animation on the CPU, and the Lua sprite benchmark. The benchmarks that draw run on the null device of the headless host, on the graphics API that `api` names, or once on every API of the build with `every_api`."""

    CPU = {
        "core": "haylen-core-benchmark",
        "algorithms": "haylen-algorithm-benchmark",
        "procedural": "haylen-procedural-benchmark",
        "physics": "haylen-physics-benchmark",
        "ui": "haylen-ui-benchmark",
        "render": "haylen-render-benchmark",
        "particles": "haylen-particle-benchmark",
        "content": "haylen-content-benchmark",
        "nodes": "haylen-node-benchmark",
        "animation": "haylen-animation-benchmark",
    }
    SPRITES = "haylen-sprite-benchmark"
    LUA = "haylen-lua-benchmark"
    SUITES = ("sprites", *CPU, "lua")
    DRAWING = frozenset({"sprites", "physics", "ui", "render", "particles", "nodes", "animation"})
    # The exit status of a benchmark whose device cannot start on this machine, `BenchmarkGpu::kUnavailable`.
    UNAVAILABLE = 3
    CONFIGURATION = "Release"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, suite: str, api: str | None, every_api: bool) -> dict[str, object]:
        if (api or every_api) and suite not in Benchmarks.DRAWING:
            names = ", ".join(f'"{name}"' for name in sorted(Benchmarks.DRAWING))
            raise UnsupportedTarget(f'The "{suite}" benchmark draws nothing, so it takes neither "--gpu" nor "--graphics-api". The benchmarks that draw are {names}.')

        tree = EngineTree(self.context, EngineTree.host_name(Benchmarks.CONFIGURATION))
        tree.ensure(tree.command(Benchmarks.CONFIGURATION, *EngineTree.host_options()))
        if suite == "lua":
            tree.build([Benchmarks.LUA], Benchmarks.CONFIGURATION)
            self.context.terminal.step('Running the "lua" benchmark')
            self.context.process.run([tree.binary(Benchmarks.LUA), Repository.ENGINE / "bench" / "lua-benchmark"], stream=True)
            return {"suite": suite, "graphicsApis": []}

        target = Benchmarks.CPU.get(suite, Benchmarks.SPRITES)
        tree.build([target], Benchmarks.CONFIGURATION)
        # The sprite benchmark is an app of `haylen_add_app`, whose executable lies in a folder of its own with its package next to it.
        executable = tree.folder / "bin" / target / Host.executable(target) if target == Benchmarks.SPRITES else tree.binary(target)
        apis = self.graphics_apis(tree, api, every_api)
        started = self.run_on(suite, executable, apis)
        return {"suite": suite, "graphicsApis": started}

    def graphics_apis(self, tree: EngineTree, api: str | None, every_api: bool) -> list[str | None]:
        """The APIs that a benchmark runs on: the one that `api` names, every API that the tree compiled with `every_api`, or else none, which leaves the headless benchmarks on the null device and the window of the sprite benchmark on the first API that starts."""
        if api:
            return [api]
        if not every_api:
            return [None]
        compiled = tree.tree.cache_value("HAYLEN_GRAPHICS_APIS")
        if not compiled:
            raise BuildFailed(f'The CMake cache of `{Repository.shown(tree.folder)}` names no "HAYLEN_GRAPHICS_APIS". Configure the tree again with "haylen engine configure --config Release".')
        return list(compiled.split(";"))

    def run_on(self, suite: str, executable: object, apis: list[str | None]) -> list[str | None]:
        """Runs a benchmark once per API, each printing its own table, and goes past an API whose device cannot start on this machine, which the benchmark explains."""
        started: list[str | None] = []
        for api in apis:
            self.context.terminal.step(f'Running the "{suite}" benchmark on "{api}"' if api else f'Running the "{suite}" benchmark')
            command = [str(executable), *(["--graphics-api", api] if api else [])]
            if self.context.terminal.verbose:
                self.context.terminal.command(command)
            code = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=self.context.terminal.tool_output, check=False).returncode
            if code == Benchmarks.UNAVAILABLE:
                self.context.terminal.warning(f'The device of "{api}" cannot start on this machine, as the output above says, so the "{suite}" benchmark skips it.')
                continue
            if code != 0:
                raise BuildFailed(f'The "{suite}" benchmark failed with exit code {code}, and its output above shows why.')
            started.append(api)
        if not started:
            raise BuildFailed(f'No device that the "{suite}" benchmark asked for starts on this machine.')
        return started
