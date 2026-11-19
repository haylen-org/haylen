import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.engine.android_gpu_tests import AndroidGpuTests
from haylen.engine.apple_device_gpu_tests import AppleDeviceGpuTests
from haylen.engine.apple_gpu_tests import AppleGpuTests
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tests import EngineTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.port_test import PortTest
from haylen.engine.suite_shard import SuiteShard
from haylen.engine.ui_tests import UiTests
from haylen.engine.web_app_check import WebAppCheck
from haylen.engine.web_checks import WebChecks
from haylen.engine.web_gpu_tests import WebGpuTests


class TestCommand(Command):
    PATH = ("test",)
    HELP = 'Build and run the engine tests on this machine. With "--gpu" it runs the GPU tests on this machine, the web, Android, the Apple simulators, Mac Catalyst or Apple devices instead, with "--target web" alone the checks of the web in headless browsers, with "--target web" and "--app" the start of an app on the web, with "--ui" the UI tests of an app on an Apple simulator or device or on Android, and with "--port" the app of the test port.'

    DESKTOPS = ("macos", "windows", "linux")
    # The names of `gpu::Device::kApiNames`, which "--graphics-api" takes.
    GRAPHICS_APIS = ("metal", "direct3D12", "direct3D11", "vulkan", "openGl", "openGlEs", "webGl2", "webGpu")
    # The targets of the tests, with the names that the app commands and "haylen devices" use.
    TARGETS = (*DESKTOPS, "web", "android", *AppleGpuTests.SLICES, *AppleDeviceGpuTests.SLICES)

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--target",
            default=Host.system(),
            choices=TestCommand.TARGETS,
            help='The target that the tests build for and run on, this machine by default. The web, Android, the Apple simulators, Mac Catalyst and Apple devices run the GPU tests, the web also its checks, and "--ui" takes "ios-simulator", "tvos-simulator", "visionos-simulator", "ios", "tvos", "visionos" or "android".',
        )
        parser.add_argument("--config", default="Debug", choices=EngineTree.CONFIGURATIONS, help='The configuration of the build tree, or of the app of "--ui" or "--app", "Debug" by default.')
        parser.add_argument("--sanitizers", choices=EngineTree.SANITIZERS, help="Build in a tree of its own with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer.")
        parser.add_argument("--cmake-target", default=EngineTests.TARGET, help=f'The CMake target that the engine tests build before CTest runs them, "{EngineTests.TARGET}" by default.')
        parser.add_argument(
            "--shard",
            type=SuiteShard.parse,
            metavar="index/count",
            help='Run one of the equal parts of the engine tests of this machine, or of the checks of the web with "--target web", such as "2/4", so several machines run them side by side and together run each test once.',
        )
        parser.add_argument(
            "--gpu",
            action="store_true",
            help="Run the GPU tests, the conformance suite of the GPU devices and the images of the 2D renderer, on every device the build compiled that starts there, instead of the engine tests.",
        )
        parser.add_argument(
            "--graphics-api",
            choices=TestCommand.GRAPHICS_APIS,
            help='A name of "gpu::Device::kApiNames", such as "vulkan", which runs the GPU tests on that device alone. With "--target web" and without "--gpu", "webGpu" or "webGl2" names the API that the pages of the checks of the web draw through.',
        )
        parser.add_argument(
            "--device",
            help='The Android device of "--target android", the only one connected or an emulator that it starts by default, the name or identifier of a simulator, a booted one or the first of the family by default, or the device of "--target ios", "tvos" or "visionos". The command "haylen devices" lists them.',
        )
        parser.add_argument(
            "--ui",
            metavar="app",
            help='Run the UI tests of the app of this folder, the Swift tests of its folder "ui-tests/apple" on an Apple simulator or device or the Kotlin tests of its folder "ui-tests/android" on Android, as "--target" and "--device" name, instead of the engine tests.',
        )
        parser.add_argument(
            "--device-farm",
            metavar="devices",
            type=lambda value: tuple(name.strip() for name in value.split(",") if name.strip()),
            default=(),
            help='Run the UI tests of "--ui" on the devices of AWS Device Farm that this list names, separated by commas, such as "Google Pixel 9,Samsung Galaxy S24", with "--target android" or "ios", instead of a device of this machine. The AWS command line takes its credentials from the environment.',
        )
        parser.add_argument(
            "--app",
            metavar="folder",
            help='Build the app of this folder for the web in the configuration of "--config" and check that its page starts it in a headless browser on WebGPU and WebGL2, or on the API of "--graphics-api", instead of the checks of the web. It takes "--target web".',
        )
        parser.add_argument(
            "--port",
            action="store_true",
            help='Build the engine with the test port of "engine/tests/data/port", whose platform and GPU device come from outside the engine, and play its app on it, instead of the engine tests.',
        )

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        context = session.context
        target = arguments.target
        gpu = arguments.gpu or arguments.graphics_api is not None
        # On the web "--graphics-api" names the API of the pages of the checks, and on a desktop it asks for the GPU tests.
        shardable = (target == "web" and not arguments.gpu) or (target in TestCommand.DESKTOPS and not gpu)
        if arguments.shard and (not shardable or arguments.app or arguments.ui or arguments.port):
            raise UnsupportedTarget(
                'The option "--shard" splits the engine tests of this machine or the checks of the web, so it takes neither the GPU tests, "--app", "--ui", "--port" nor a "--target" other than a desktop or "web".'
            )
        if arguments.app:
            if target != "web":
                raise UnsupportedTarget('The option "--app" checks that the page of an app starts on the web, so it takes "--target web".')
            if arguments.gpu or arguments.ui or arguments.port or arguments.sanitizers:
                raise UnsupportedTarget('The option "--app" checks that the page of an app starts, so it takes neither "--gpu", "--ui", "--port" nor "--sanitizers".')
            return WebAppCheck(context, session.pipeline).run(arguments.app, arguments.config, arguments.graphics_api)
        if arguments.ui:
            if gpu or arguments.port:
                raise UnsupportedTarget('The option "--ui" runs the UI tests of an app, so it takes neither "--gpu", "--graphics-api" nor "--port".')
            return UiTests(context, session.pipeline).run(arguments.ui, target, arguments.config, arguments.device, arguments.device_farm)
        if arguments.device_farm:
            raise UnsupportedTarget('The option "--device-farm" runs the UI tests of an app, so it takes "--ui".')
        if arguments.port:
            if gpu or target != Host.system():
                raise UnsupportedTarget('The option "--port" builds the test port for this machine, so it takes neither "--gpu", "--graphics-api" nor "--target".')
            return PortTest(context).run(arguments.config, arguments.sanitizers)

        # The checks of the web draw too, so on the web "--graphics-api" names the API of their pages unless "--gpu" asks for the GPU tests.
        if target == "web":
            if arguments.gpu:
                return WebGpuTests(context).run(arguments.config, arguments.graphics_api)
            return WebChecks(context, session.pipeline).run(arguments.graphics_api, arguments.shard)
        if target not in TestCommand.DESKTOPS and not gpu:
            raise UnsupportedTarget(f'The target "{target}" runs only the GPU tests. Add "--gpu", or a name of "gpu::Device::kApiNames" with "--graphics-api".')
        if target == "android":
            return AndroidGpuTests(context).run(arguments.config, arguments.graphics_api, arguments.device)
        if target in AppleGpuTests.SLICES:
            return AppleGpuTests(context).run(target, arguments.config, arguments.graphics_api, arguments.device)
        if target in AppleDeviceGpuTests.SLICES:
            return AppleDeviceGpuTests(context).run(target, arguments.config, arguments.graphics_api, arguments.device)

        if target != Host.system():
            raise UnsupportedTarget(f'The tests of "{target}" run on a "{target}" machine, and this one runs "{Host.system()}".')
        if gpu:
            return DesktopGpuTests(context).run(arguments.config, arguments.sanitizers, arguments.graphics_api)
        return EngineTests(context).run(arguments.config, arguments.sanitizers, arguments.cmake_target, arguments.shard)
