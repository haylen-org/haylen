#!/usr/bin/env python3
"""Single entry point to build, run, test and package Haylen and its apps on every platform."""

from __future__ import annotations

import argparse
import contextlib
import copy
import dataclasses
import functools
import hashlib
import http.server
import json
import os
import platform as host_platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
import webbrowser
import zipfile
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parent
BUILD_ROOT = ROOT / "build"
TOOLS_DIR = ROOT / ".tools"
ENGINE_DIR = ROOT / "engine"
SAMPLES_DIR = ROOT / "samples"
TEMPLATES_DIR = ROOT / "templates"
APP_TEMPLATE = TEMPLATES_DIR / "app"
PLUGIN_TEMPLATE = TEMPLATES_DIR / "plugin"
# Every folder here is the project template of one platform, which `make.py new` copies into `platform/<name>` of an app.
PLATFORM_TEMPLATES_DIR = TEMPLATES_DIR / "platform"
ARTIFACTS_DIR = BUILD_ROOT / "artifacts"
ENGINE_BUILDS_DIR = BUILD_ROOT / "engine"
APPS_DIR = BUILD_ROOT / "apps"
CPP_BUILDS_DIR = BUILD_ROOT / "cpp"
ANDROID_LIBRARY_PROJECT = ENGINE_DIR / "platform" / "android"
ENGINE_LOGO = PLATFORM_TEMPLATES_DIR / "web" / "haylen-logo.svg"
# The web runtime loads the AudioWorklet processor of its audio output from next to its script, so every web build ships it beside the `.js` and `.wasm` files.
WEB_AUDIO_WORKLET = "haylen-audio-worklet.js"
DEFAULT_APP = "games/tiny-island"

SHDC_COMMIT = "11d0cf678105d614d675e6d9bd2aaf3eeff12f8c"
# App shaders include the shader library as `haylen/material.glsl`, which sokol-shdc finds from its working directory.
SHADER_LIBRARY_DIR = ENGINE_DIR / "shaders" / "include"
SHADER_SLANGS = "glsl430:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl"
# Every material compiles once per kind of draw and once more for lit canvases, and the engine picks the program of a draw by these names.
SHADER_PROGRAMS = {"sprite": [], "sprite_lit": ["HAYLEN_LIT"], "text": ["HAYLEN_TEXT"], "text_lit": ["HAYLEN_TEXT", "HAYLEN_LIT"], "mesh": ["HAYLEN_MESH"], "mesh_lit": ["HAYLEN_MESH", "HAYLEN_LIT"]}
# The uniform blocks and textures of the shader library, which the engine fills itself.
SHADER_ENGINE_BLOCKS = {"haylen_vs_params", "haylen_lit_params"}
SHADER_ENGINE_TEXTURES = {"sprite_texture"}
SHADER_WATCH_SECONDS = 0.5
GRADLE_VERSION = "9.8.0"
# XcodeGen generates the Apple project again for apps whose plugins add to it. The hash is the one of the `xcodegen.zip` asset of the release.
XCODEGEN_VERSION = "2.46.0"
XCODEGEN_SHA256 = "4d9e34b62172d645eed6457cac13fc222569974098ef4ee9c3368bedf0196806"
ANDROID_NDK_VERSION = "30.0.16248370"
EMSDK_VERSION = "6.0.10"
# The miniaudio library plays through AAudio from Android 8.1 on, and the engine builds it without OpenSL ES, like the `minSdk` of the Android library and template.
ANDROID_MIN_SDK = 27
# The ABIs of the `haylen` Android library, which the native libraries of an app match: 32-bit ARM keeps the Android TV devices that still run it, and `x86_64` serves emulators.
ANDROID_ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64")
# The oldest Apple systems the engine runs on: `std::format` with floating point, which the engine formats text and logs with, reaches their C++ library in iOS and tvOS 16.3 and macOS 13.3.
# Mac Catalyst takes its minimum, the iOS version, from `engine/cmake/haylen-catalyst.toolchain.cmake`.
APPLE_MINIMUM_VERSIONS = {"iOS": "16.3", "tvOS": "16.3", "macOS": "13.3"}

PLATFORMS = ["macos", "linux", "windows", "ios", "tvos", "android", "web", "web-webgl2"]
DESKTOP_PLATFORMS = {"macos", "linux", "windows"}
WEB_PLATFORMS = {"web", "web-webgl2"}
CONFIGS = ["Debug", "Release", "RelWithDebInfo"]
ARTIFACT_PLATFORMS = ["apple", "android", "web", "desktop"]
FORMAT_EXTENSIONS = {".h", ".hpp", ".c", ".cpp", ".m", ".mm"}
FORMAT_ROOTS = ["engine/include", "engine/src", "engine/tests", "samples", "templates"]
# Build outputs inside those roots, such as the native tree Gradle keeps in each Android project, hold generated and third-party code.
FORMAT_SKIPPED_FOLDERS = {".cxx", ".gradle", "build", "_deps"}
# A package is `app.json` with the Lua modules under `source` and the assets under `content`, and nothing else in its folder ships.
PACKAGE_FOLDERS = ("source", "content")
# Build outputs and Finder files that never travel with a copied platform folder or plugin.
COPY_IGNORED = shutil.ignore_patterns(".DS_Store", ".git", "build", ".gradle", ".cxx", ".kotlin")
# Engine files that never reach an artifact, so editing them keeps the artifacts fresh.
ENGINE_HASH_SKIPPED = {"tests", "bench", "build", ".cxx", ".gradle", ".kotlin", ".DS_Store"}
# The benchmarks of `make.py bench` that are plain executables on the CPU, by suite.
CPU_BENCHMARKS = {"algorithms": "haylen-algorithm-benchmark", "procedural": "haylen-procedural-benchmark"}

# The slices of `Haylen.xcframework` and the architectures each one joins with lipo.
APPLE_SLICES = {
    "macos": ["arm64", "x86_64"],
    "ios": ["arm64"],
    "ios-simulator": ["arm64", "x86_64"],
    "ios-maccatalyst": ["arm64", "x86_64"],
    "tvos": ["arm64"],
    "tvos-simulator": ["arm64", "x86_64"],
}

# How each Apple run platform builds the template project: its scheme, the xcodebuild destination and the simulator family it boots.
APPLE_RUNS = {
    "macos": {"scheme": "macOS", "destination": "platform=macOS"},
    "catalyst": {"scheme": "iOS", "destination": "platform=macOS,variant=Mac Catalyst"},
    "ios": {"scheme": "iOS", "destination": "generic/platform=iOS"},
    "ios-simulator": {"scheme": "iOS", "simulator": "iOS"},
    "tvos": {"scheme": "tvOS", "destination": "generic/platform=tvOS"},
    "tvos-simulator": {"scheme": "tvOS", "simulator": "tvOS"},
}

ANDROID_ORIENTATIONS = {"landscape": "sensorLandscape", "portrait": "sensorPortrait", "any": "fullSensor"}
IOS_ORIENTATIONS = {
    "landscape": (["UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"], ["UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"]),
    "portrait": (["UIInterfaceOrientationPortrait"], ["UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown"]),
    "any": (["UIInterfaceOrientationPortrait", "UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"], ["UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown", "UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"]),
}

# Varn names the engine in the unified log of Apple platforms with this subsystem.
APPLE_LOG_SUBSYSTEM = "dev.varn.engine"
LOG_STREAM_GRACE_SECONDS = 1.0

MIME_TYPES = {".wasm": "application/wasm", ".js": "text/javascript", ".mjs": "text/javascript", ".json": "application/json", ".zip": "application/zip", ".html": "text/html", ".css": "text/css", ".svg": "image/svg+xml", ".png": "image/png", ".data": "application/octet-stream"}


class BuildError(RuntimeError):
    pass


def run(command: list, *, cwd: Path = ROOT, env: dict[str, str] | None = None) -> None:
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, env=env, check=True)


def capture(command: list, *, cwd: Path = ROOT) -> str:
    return subprocess.run([str(part) for part in command], cwd=cwd, check=True, capture_output=True, text=True).stdout


def default_jobs() -> int:
    return max(1, (os.cpu_count() or 2) - 1)


def host_name() -> str:
    system = host_platform.system().lower()
    names = {"darwin": "macos", "linux": "linux", "windows": "windows"}
    if system not in names:
        raise BuildError(f'Unsupported host system "{system}".')
    return names[system]


def host_arch() -> str:
    machine = host_platform.machine().lower()
    if machine in {"arm64", "aarch64"}:
        return "arm64"
    if machine in {"x86_64", "amd64"}:
        return "x64"
    raise BuildError(f'Unsupported host architecture "{machine}".')


def executable_name(name: str) -> str:
    return f"{name}.exe" if host_name() == "windows" else name


def engine_version() -> str:
    return (ENGINE_DIR / "VERSION").read_text().strip()


def download(url: str, target: Path, sha256: str | None = None) -> None:
    """Downloads a file and, when a hash is pinned, deletes the download and fails unless its SHA-256 matches."""
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f'Downloading "{url}".', flush=True)
    urllib.request.urlretrieve(url, target)
    if sha256 and hashlib.sha256(target.read_bytes()).hexdigest() != sha256:
        target.unlink()
        raise BuildError(f'The file "{url}" does not match its pinned SHA-256 "{sha256}", so "make.py" deleted the download.')


def ensure_shdc() -> Path:
    executable = executable_name("sokol-shdc")
    target = TOOLS_DIR / executable
    if target.exists():
        return target

    folders = {("macos", "arm64"): "osx_arm64", ("macos", "x64"): "osx", ("linux", "arm64"): "linux_arm64", ("linux", "x64"): "linux", ("windows", "x64"): "win32"}
    folder = folders.get((host_name(), host_arch()))
    if folder is None:
        raise BuildError("The sokol-shdc tool has no prebuilt binary for this host.")

    download(f"https://raw.githubusercontent.com/floooh/sokol-tools-bin/{SHDC_COMMIT}/bin/{folder}/{executable}", target)
    target.chmod(0o755)
    return target


def ensure_gradle() -> Path:
    executable = "gradle.bat" if host_name() == "windows" else "gradle"
    target = TOOLS_DIR / f"gradle-{GRADLE_VERSION}" / "bin" / executable
    if target.exists():
        return target

    archive = TOOLS_DIR / f"gradle-{GRADLE_VERSION}-bin.zip"
    download(f"https://services.gradle.org/distributions/gradle-{GRADLE_VERSION}-bin.zip", archive)
    with zipfile.ZipFile(archive) as package:
        package.extractall(TOOLS_DIR)
    archive.unlink()
    target.chmod(0o755)
    return target


def ensure_xcodegen() -> Path:
    """Returns the pinned XcodeGen, whose release zip unpacks into `.tools/xcodegen` with the setting presets next to its binary."""
    target = TOOLS_DIR / "xcodegen" / "bin" / "xcodegen"
    if target.exists():
        return target

    require_host("apple")
    archive = TOOLS_DIR / "xcodegen.zip"
    download(f"https://github.com/yonaskolb/XcodeGen/releases/download/{XCODEGEN_VERSION}/xcodegen.zip", archive, XCODEGEN_SHA256)
    with zipfile.ZipFile(archive) as package:
        package.extractall(TOOLS_DIR)
    archive.unlink()
    target.chmod(0o755)
    return target


def emsdk_root() -> Path:
    return TOOLS_DIR / "emsdk"


def ensure_emsdk() -> Path:
    root = emsdk_root()
    emcmake = root / "upstream" / "emscripten" / ("emcmake.bat" if host_name() == "windows" else "emcmake")
    if emcmake.exists():
        return emcmake

    if not root.exists():
        run(["git", "clone", "--depth", "1", "https://github.com/emscripten-core/emsdk.git", root])
    script = root / ("emsdk.bat" if host_name() == "windows" else "emsdk")
    run([script, "install", EMSDK_VERSION], cwd=root)
    run([script, "activate", EMSDK_VERSION], cwd=root)
    return emcmake


def android_sdk() -> Path:
    for variable in ("ANDROID_HOME", "ANDROID_SDK_ROOT"):
        value = os.environ.get(variable)
        if value and Path(value).exists():
            return Path(value)
    raise BuildError('The Android SDK was not found. Set "ANDROID_HOME" to its folder.')


def android_ndk() -> Path:
    """The pinned NDK inside the Android SDK, whatever other NDK the environment names, because the engine and its dependencies build against its headers."""
    candidate = android_sdk() / "ndk" / ANDROID_NDK_VERSION
    if candidate.exists():
        return candidate
    raise BuildError(f"The Android NDK {ANDROID_NDK_VERSION} was not found in \"{android_sdk() / 'ndk'}\". Install it with \"sdkmanager 'ndk;{ANDROID_NDK_VERSION}'\".")


def adb() -> Path:
    return android_sdk() / "platform-tools" / executable_name("adb")


def android_options(abi: str) -> list[str]:
    """Returns the CMake options that build for one Android ABI with the NDK, which the engine and the native libraries of apps share."""
    toolchain = android_ndk() / "build" / "cmake" / "android.toolchain.cmake"
    return [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", f"-DANDROID_ABI={abi}", f"-DANDROID_PLATFORM=android-{ANDROID_MIN_SDK}", "-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"]


def require_host(platform_name: str) -> None:
    apple = {"macos", "ios", "tvos", "apple", "ios-simulator", "tvos-simulator", "catalyst"}
    required = "macos" if platform_name in apple else {"linux": "linux", "windows": "windows"}.get(platform_name)
    if required is not None and host_name() != required:
        raise BuildError(f'Builds for "{platform_name}" require a "{required}" host.')


def build_dir(platform_name: str, config: str, sanitizers: str | None = None) -> Path:
    """Returns the build tree of a platform and configuration. Each sanitizer has trees of its own, so switching never rebuilds the plain tree."""
    return BUILD_ROOT / (f"{platform_name}-{config.lower()}" + (f"-{sanitizers}" if sanitizers else ""))


def build_folder_name(folder: Path) -> str:
    """Names the build folder of an app or a C++ project after its folder and a hash of its path, so projects whose folders share a name never share build trees."""
    return f"{folder.name}-{hashlib.sha256(str(folder).encode()).hexdigest()[:8]}"


def requested_backend(args: argparse.Namespace) -> str:
    """Returns the `HAYLEN_RENDER_BACKEND` a build asks for. Each web platform always uses its own backend."""
    if args.platform in WEB_PLATFORMS:
        return "WGPU" if args.platform == "web" else "GLES3"
    return args.backend or "AUTO"


def requested_sanitizers(args: argparse.Namespace) -> str:
    """Returns the `HAYLEN_SANITIZERS` a build asks for: `OFF`, `ADDRESS` or `THREAD`."""
    return (args.sanitizers or "off").upper()


def build_options(platform_name: str, config: str, target: str | None = None, jobs: int | None = None) -> argparse.Namespace:
    return argparse.Namespace(platform=platform_name, config=config, backend=None, xcode=False, sanitizers=None, coverage=False, target=target, jobs=jobs or default_jobs())


def configure_command(args: argparse.Namespace) -> tuple[list, dict[str, str]]:
    require_host(args.platform)
    directory = build_dir(args.platform, args.config, args.sanitizers)
    command = ["cmake", "-S", ROOT, "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}"]
    command += [f"-DHAYLEN_RENDER_BACKEND={requested_backend(args)}", f"-DHAYLEN_SANITIZERS={requested_sanitizers(args)}"]
    env = os.environ.copy()

    if args.platform == "macos":
        command += ["-G", "Xcode"] if args.xcode else ["-G", "Ninja"]
        command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}")
    elif args.platform in {"ios", "tvos"}:
        system = "iOS" if args.platform == "ios" else "tvOS"
        command += ["-G", "Xcode", f"-DCMAKE_SYSTEM_NAME={system}", "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS[system]}", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_PLAYER=OFF"]
    elif args.platform == "android":
        command += ["-G", "Ninja", *android_options("arm64-v8a"), "-DHAYLEN_BUILD_TESTS=OFF"]
    elif args.platform in WEB_PLATFORMS:
        command = [ensure_emsdk(), *command, "-G", "Ninja", "-DHAYLEN_BUILD_TESTS=OFF"]
    elif args.platform == "linux":
        command += ["-G", "Ninja"]
    elif args.platform == "windows":
        command += ["-G", "Ninja"] if shutil.which("ninja") else []

    if getattr(args, "coverage", False):
        command += ["-DHAYLEN_ENABLE_COVERAGE=ON", "-DHAYLEN_BUILD_PLAYER=OFF"]
    return command, env


def command_tools(args: argparse.Namespace) -> None:
    print(ensure_shdc())
    if host_name() == "macos":
        print(ensure_xcodegen())
    if args.emsdk:
        print(ensure_emsdk())
    if args.gradle:
        print(ensure_gradle())


def command_configure(args: argparse.Namespace) -> None:
    command, env = configure_command(args)
    run(command, env=env)


def cmake_cache_value(directory: Path, name: str) -> str:
    prefix = f"{name}:"
    for line in (directory / "CMakeCache.txt").read_text().splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1]
    raise BuildError(f'The variable "{name}" is missing from the CMake cache in "{directory}".')


def ensure_configured(args: argparse.Namespace) -> Path:
    """Configures the build tree when it does not exist yet or when the requested backend differs from the one it was configured with."""
    directory = build_dir(args.platform, args.config, args.sanitizers)
    if not (directory / "CMakeCache.txt").exists() or cmake_cache_value(directory, "HAYLEN_RENDER_BACKEND") != requested_backend(args):
        command_configure(args)
    return directory


def command_build(args: argparse.Namespace) -> None:
    directory = ensure_configured(args)
    command = ["cmake", "--build", directory, "--config", args.config, "--parallel", str(args.jobs)]
    if args.target:
        command += ["--target", args.target]
    run(command)


def command_test(args: argparse.Namespace) -> None:
    args.platform = host_name()
    args.target = "haylen_tests"
    command_build(args)
    run(["ctest", "--test-dir", build_dir(args.platform, args.config, args.sanitizers), "-C", args.config, "--output-on-failure", "--parallel", str(args.jobs)])


def llvm_tool(name: str) -> str:
    if host_name() == "macos":
        return capture(["xcrun", "--find", name]).strip()
    tool = shutil.which(name)
    if tool is None:
        raise BuildError(f'The tool "{name}" was not found. Install LLVM to produce coverage reports.')
    return tool


def command_coverage(args: argparse.Namespace) -> None:
    args.platform = host_name()
    args.config = "Debug"
    args.coverage = True
    args.backend = None
    args.xcode = False
    directory = BUILD_ROOT / "coverage"

    command, env = configure_command(args)
    command[command.index("-B") + 1] = str(directory)
    run(command, env=env)
    run(["cmake", "--build", directory, "--target", "haylen_tests", "--parallel", str(args.jobs)])

    profiles = directory / "coverage"
    shutil.rmtree(profiles, ignore_errors=True)
    run(["ctest", "--test-dir", directory, "--output-on-failure", "--parallel", str(args.jobs)])

    raw = sorted(profiles.glob("*.profraw"))
    if not raw:
        raise BuildError("The test run produced no coverage profiles.")
    merged = profiles / "haylen.profdata"
    run([llvm_tool("llvm-profdata"), "merge", "-sparse", *raw, "-o", merged])

    binary = directory / "bin" / "haylen_tests"
    ignore = r"(_deps|\.cache|engine/tests|engine/src/platform/(apple|android|web|windows|linux|sokol)|generated)"
    report = [llvm_tool("llvm-cov"), "report", binary, f"-instr-profile={merged}", f"-ignore-filename-regex={ignore}"]
    run(report)
    run([llvm_tool("llvm-cov"), "show", binary, f"-instr-profile={merged}", f"-ignore-filename-regex={ignore}", "-format=html", f"-output-dir={profiles / 'html'}"])
    print(profiles / "html" / "index.html")


def format_sources() -> list[Path]:
    files: list[Path] = []
    for root in FORMAT_ROOTS:
        folder = ROOT / root
        if folder.exists():
            files += [path for path in folder.rglob("*") if path.suffix in FORMAT_EXTENSIONS and path.is_file() and not FORMAT_SKIPPED_FOLDERS.intersection(path.relative_to(folder).parts)]
    return sorted(files)


LAMBDA_START = re.compile(r"\]\s*(\([^)]*\))?\s*(mutable\s*)?(noexcept\s*)?(->\s*[\w:<>]+\s*)?\{\s*$")


def unguarded_lambdas(files: list[Path]) -> list[str]:
    """Lists multi-line lambdas outside `clang-format off` regions, since clang-format cannot lay them out acceptably."""
    findings: list[str] = []
    for path in files:
        guarded = False
        for number, line in enumerate(path.read_text().splitlines(), start=1):
            stripped = line.strip()
            if stripped == "// clang-format off":
                guarded = True
            elif stripped == "// clang-format on":
                guarded = False
            elif not guarded and LAMBDA_START.search(stripped) and not stripped.startswith(("//", "[[", "#")):
                findings.append(f'{path.relative_to(ROOT)}:{number}: Multi-line lambda outside "clang-format off".')
    return findings


def command_format(args: argparse.Namespace) -> None:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        raise BuildError('The clang-format tool was not found on "PATH".')

    files = format_sources()
    command = [clang_format, "--style=file"] + (["--dry-run", "--Werror"] if args.check else ["-i"])
    for start in range(0, len(files), 200):
        run(command + files[start : start + 200])

    findings = unguarded_lambdas(files)
    for finding in findings:
        print(finding)
    if findings and args.check:
        raise BuildError(f'{len(findings)} multi-line lambdas need "clang-format off" and "on" markers.')


def command_assets(args: argparse.Namespace) -> None:
    run([sys.executable, ROOT / "tools" / "import_tiny_swords.py", Path(args.archive).expanduser().resolve(), "--destination", SAMPLES_DIR / DEFAULT_APP / "content" / "tiny_swords"])


def command_map(_: argparse.Namespace) -> None:
    run([sys.executable, ROOT / "tools" / "generate_island_map.py", "--package", SAMPLES_DIR / DEFAULT_APP])


def command_bench(args: argparse.Namespace) -> None:
    """Builds a benchmark in Release and runs it on this machine: sprites on the GPU, path finding, crowds, spatial queries and ray casts on the CPU, procedural generation, geometry and destruction on the CPU, or the Lua bunnymark on the CPU."""
    if args.suite in CPU_BENCHMARKS:
        build = build_options(host_name(), "Release", CPU_BENCHMARKS[args.suite], args.jobs)
        command_build(build)
        run([build_dir(build.platform, build.config) / "bin" / executable_name(build.target)])
        return

    if args.suite == "lua":
        build = build_options(host_name(), "Release", "haylen-lua-benchmark", args.jobs)
        command_build(build)
        run([build_dir(build.platform, build.config) / "bin" / executable_name(build.target), ENGINE_DIR / "bench" / "lua-benchmark"])
        return

    build = build_options(host_name(), "Release", "haylen-sprite-benchmark", args.jobs)
    command_build(build)
    run([cmake_app_executable(build_dir(build.platform, build.config), build.target)])


def sdk_dir(platform_name: str, config: str) -> Path:
    return BUILD_ROOT / "sdk" / f"haylen-{platform_name}-{config.lower()}"


def command_sdk(args: argparse.Namespace) -> None:
    """Builds the engine SDK for a platform and installs it where `find_package(haylen)` finds it."""
    options = build_options(args.platform, args.config, jobs=args.jobs)
    directory = BUILD_ROOT / f"sdk-build-{args.platform}-{args.config.lower()}"
    command, env = configure_command(options)
    command[command.index("-S") + 1] = str(ENGINE_DIR)
    command[command.index("-B") + 1] = str(directory)
    command += ["-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_PLAYER=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF"]
    run(command, env=env)
    run(["cmake", "--build", directory, "--config", args.config, "--target", "haylen_sdk", "--parallel", str(args.jobs)])
    output = Path(args.output).resolve() if args.output else sdk_dir(args.platform, args.config)
    run(["cmake", "--install", directory, "--config", args.config, "--component", "haylen_sdk", "--prefix", output])


def command_embedding(args: argparse.Namespace) -> None:
    """Builds the C++ embedding sample, which adds the engine the way another repository would."""
    directory = BUILD_ROOT / f"embedding-{args.mode}-{args.config.lower()}"
    command = ["cmake", "-S", SAMPLES_DIR / "cpp" / "embedding", "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}", f"-DCPP_EMBEDDING_MODE={args.mode}"]
    if host_name() != "windows" or shutil.which("ninja"):
        command += ["-G", "Ninja"]
    if args.mode == "package":
        command_sdk(argparse.Namespace(platform=host_name(), config=args.config, jobs=args.jobs, output=None))
        command.append(f"-DCMAKE_PREFIX_PATH={sdk_dir(host_name(), args.config)}")
    run(command)
    run(["cmake", "--build", directory, "--config", args.config, "--parallel", str(args.jobs)])


def cmake_app_executable(directory: Path, target: str) -> Path:
    folder = directory / "bin" / target
    if host_name() == "macos":
        return folder / f"{target}.app" / "Contents" / "MacOS" / target
    return folder / executable_name(target)


# Engine artifacts: the prebuilt engine that apps made from the templates use, so an app never compiles the engine.


def engine_sources_hash() -> str:
    """Hashes every engine file that reaches an artifact, so `make.py` knows when the artifacts are stale."""
    digest = hashlib.sha256()
    for folder, subfolders, files in os.walk(ENGINE_DIR):
        subfolders[:] = sorted(name for name in subfolders if name not in ENGINE_HASH_SKIPPED)
        for name in sorted(files):
            if name in ENGINE_HASH_SKIPPED:
                continue
            path = Path(folder) / name
            digest.update(path.relative_to(ENGINE_DIR).as_posix().encode())
            digest.update(b"\0")
            digest.update(path.read_bytes())
    return digest.hexdigest()


def read_manifest() -> dict:
    path = ARTIFACTS_DIR / "manifest.json"
    return json.loads(path.read_text()) if path.is_file() else {"platforms": {}}


def write_manifest(platform_name: str, config: str, sources: str) -> None:
    manifest = read_manifest()
    manifest["version"] = engine_version()
    manifest["platforms"][platform_name] = {"config": config, "sources": sources}
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    (ARTIFACTS_DIR / "manifest.json").write_text(json.dumps(manifest, indent=4) + "\n")


def apple_slice_options(slice_name: str, arch: str) -> list[str]:
    if slice_name == "ios-maccatalyst":
        return [f"-DCMAKE_TOOLCHAIN_FILE={ENGINE_DIR / 'cmake' / 'haylen-catalyst.toolchain.cmake'}", f"-DCMAKE_OSX_ARCHITECTURES={arch}"]
    systems = {"macos": ("macOS", "macosx"), "ios": ("iOS", "iphoneos"), "ios-simulator": ("iOS", "iphonesimulator"), "tvos": ("tvOS", "appletvos"), "tvos-simulator": ("tvOS", "appletvsimulator")}
    system, sdk = systems[slice_name]
    options = [f"-DCMAKE_OSX_ARCHITECTURES={arch}", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS[system]}", f"-DCMAKE_OSX_SYSROOT={sdk}"]
    if system != "macOS":
        options.append(f"-DCMAKE_SYSTEM_NAME={system}")
    return options


def build_apple_artifacts(config: str, jobs: int) -> None:
    """Builds one static library per architecture, joins the architectures of every slice with lipo and creates `Haylen.xcframework`."""
    require_host("apple")
    libraries: list[Path] = []
    headers: Path | None = None
    for slice_name, architectures in APPLE_SLICES.items():
        archives: list[Path] = []
        for arch in architectures:
            directory = ENGINE_BUILDS_DIR / f"apple-{slice_name}-{arch}-{config.lower()}"
            if not (directory / "CMakeCache.txt").exists():
                run(["cmake", "-S", ENGINE_DIR, "-B", directory, "-G", "Ninja", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", "-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_FRAMEWORK=ON", "-DHAYLEN_BUILD_PLAYER=OFF", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", *apple_slice_options(slice_name, arch)])
            run(["cmake", "--build", directory, "--target", "haylen_framework", "--parallel", str(jobs)])
            run(["cmake", "--install", directory, "--component", "haylen_framework", "--prefix", directory / "framework"])
            archives.append(directory / "framework" / "lib" / "libhaylen.a")
            if headers is None:
                run(["cmake", "--install", directory, "--component", "haylen_sdk", "--prefix", directory / "sdk"])
                headers = directory / "sdk" / "include"

        library = archives[0]
        if len(archives) > 1:
            library = ENGINE_BUILDS_DIR / f"apple-{slice_name}-{config.lower()}" / "libhaylen.a"
            library.parent.mkdir(parents=True, exist_ok=True)
            run(["lipo", "-create", *archives, "-output", library])
        libraries.append(library)

    output = ARTIFACTS_DIR / "apple" / "Haylen.xcframework"
    shutil.rmtree(output, ignore_errors=True)
    command: list = ["xcodebuild", "-create-xcframework"]
    for library in libraries:
        command += ["-library", library, "-headers", headers]
    run(command + ["-output", output])


def build_android_players(config: str, jobs: int) -> Path:
    """Builds `libhaylen.so`, the Lua player, for one ABI after the other with every job, so the configures never write into the shared CPM sources together and the compilers stay within the jobs, and gathers the libraries in the `jniLibs` layout."""
    libraries = ENGINE_BUILDS_DIR / f"android-{config.lower()}" / "jniLibs"
    shutil.rmtree(libraries, ignore_errors=True)
    for abi in ANDROID_ABIS:
        directory = ENGINE_BUILDS_DIR / f"android-{abi}-{config.lower()}"
        if not (directory / "CMakeCache.txt").exists():
            run(["cmake", "-S", ENGINE_DIR, "-B", directory, "-G", "Ninja", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", "-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", *android_options(abi)])
        run(["cmake", "--build", directory, "--target", "haylen", "--parallel", str(jobs)])
        copy_into(directory / "lib" / "libhaylen.so", libraries / abi)
    return libraries


def build_android_artifacts(config: str, jobs: int) -> None:
    """Packages the players of every ABI with the Java side into the `haylen` Android library, together with the Kotlin transport of Varn from where CPM placed it for the native build, and publishes it to the local Maven repository of the artifacts."""
    libraries = build_android_players(config, jobs)
    varn = cmake_cache_value(ENGINE_BUILDS_DIR / f"android-{ANDROID_ABIS[0]}-{config.lower()}", "varn_SOURCE_DIR")
    maven = ARTIFACTS_DIR / "android" / "maven"
    shutil.rmtree(maven, ignore_errors=True)
    task = ":haylen:publishReleasePublicationToArtifactsRepository"
    run([ensure_gradle(), "-p", ANDROID_LIBRARY_PROJECT, task, f"--max-workers={jobs}", f"-PhaylenNativeLibraries={libraries}", f"-PhaylenVarnSourceDir={varn}", f"-PhaylenMavenDir={maven}"])


def build_web_artifacts(config: str, jobs: int) -> None:
    """Builds the player for WebGPU and for WebGL2, each next to the processor of its audio output. It loads the app package at runtime, so one build serves every app."""
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        command_build(build_options(platform_name, config, "haylen", jobs))
        built = build_dir(platform_name, config) / "bin" / "haylen"
        destination = ARTIFACTS_DIR / "web" / backend
        shutil.rmtree(destination, ignore_errors=True)
        destination.mkdir(parents=True)
        for name in ("haylen.js", "haylen.wasm", WEB_AUDIO_WORKLET):
            shutil.copy2(built / name, destination)


def desktop_artifact() -> Path:
    return ARTIFACTS_DIR / "desktop" / f"{host_name()}-{host_arch()}" / executable_name("haylen")


def build_desktop_artifacts(config: str, jobs: int) -> None:
    command_build(build_options(host_name(), config, "haylen", jobs))
    destination = desktop_artifact()
    shutil.rmtree(destination.parent, ignore_errors=True)
    destination.parent.mkdir(parents=True)
    shutil.copy2(build_dir(host_name(), config) / "bin" / "haylen" / executable_name("haylen"), destination)


ARTIFACT_BUILDERS = {"apple": build_apple_artifacts, "android": build_android_artifacts, "web": build_web_artifacts, "desktop": build_desktop_artifacts}


def build_artifacts(platform_name: str, config: str, jobs: int) -> None:
    sources = engine_sources_hash()
    ARTIFACT_BUILDERS[platform_name](config, jobs)
    write_manifest(platform_name, config, sources)
    print(f'The "{platform_name}" artifacts of Haylen {engine_version()} are in "{ARTIFACTS_DIR}".')


def ensure_artifacts(platform_name: str, config: str, jobs: int) -> None:
    """Builds the artifacts of a platform when they are missing, were built with another configuration or are older than the engine sources."""
    entry = read_manifest()["platforms"].get(platform_name)
    if entry == {"config": config, "sources": engine_sources_hash()}:
        return
    print(f'The "{platform_name}" artifacts are missing or stale, so "make.py" builds them first.', flush=True)
    build_artifacts(platform_name, config, jobs)


def command_engine(args: argparse.Namespace) -> None:
    for platform_name in ARTIFACT_PLATFORMS if args.platform == "all" else [args.platform]:
        build_artifacts(platform_name, args.config, args.jobs)


# Apps: a package folder assembled with a platform template into a project under `build/apps`, then built and launched.


def resolve_app(value: str) -> Path:
    """Accepts an app folder or the name of a sample."""
    candidate = Path(value).expanduser()
    if not candidate.exists() and (SAMPLES_DIR / value).is_dir():
        candidate = SAMPLES_DIR / value
    folder = candidate.resolve()
    if not (folder / "app.json").is_file():
        raise BuildError(f'The path "{value}" is neither an app folder with an "app.json" nor a sample path from "samples/", such as "games/tiny-island". List them with "python3 make.py samples".')
    return folder


def parse_color(text: str) -> tuple[int, int, int, int]:
    """Reads `#RRGGBB` or `#AARRGGBB` like the engine and returns red, green, blue and alpha."""
    digits = text.removeprefix("#")
    if len(digits) not in (6, 8) or not re.fullmatch(r"[0-9A-Fa-f]+", digits):
        raise BuildError(f'The value "{text}" is not a color. Colors are "#RRGGBB" or "#AARRGGBB".')
    value = int(digits, 16)
    if len(digits) == 6:
        return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 0xFF
    return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, (value >> 24) & 0xFF


class App:
    """What the platform projects need from an app folder, with the defaults the engine applies to `app.json`, and its plugins with their values for the platform it is built for."""

    def __init__(self, folder: Path, platform: str) -> None:
        self.folder = folder
        document = json.loads((folder / "app.json").read_text())
        for key in ("name", "identifier", "version"):
            if not isinstance(document.get(key), str) or not document[key]:
                raise BuildError(f'The "app.json" of "{folder}" needs a "{key}" to build an app.')
        self.name: str = document["name"]
        self.identifier: str = document["identifier"]
        self.version: str = document["version"]
        self.orientation: str = document.get("orientation", "landscape")
        if self.orientation not in ANDROID_ORIENTATIONS:
            raise BuildError(f'The orientation of "{folder}/app.json" must be "landscape", "portrait" or "any".')
        if not re.fullmatch(r"\d+(\.\d+){0,2}", self.version):
            raise BuildError(f'The version of "{folder}/app.json" must be one to three numbers separated by dots, such as "1.2.0".')

        # A transparent window clears to transparent, and an app without a taskbar button also leaves out its Dock icon on macOS.
        window = document.get("window", {})
        self.transparent: bool = window.get("transparent", False)
        self.show_in_taskbar: bool = window.get("showInTaskbar", True)
        splash = document.get("splash", {})
        self.background = parse_color(splash.get("background", document.get("clearColor", "#00000000" if self.transparent else "#FF000000")))
        self.splash_logo: Path | None = None
        if splash.get("logo"):
            self.splash_logo = folder / "content" / splash["logo"]
            if not self.splash_logo.is_file():
                raise BuildError(f'The splash logo "{self.splash_logo}" of "{folder}/app.json" does not exist.')

        native = document.get("native", {})
        if not isinstance(native, dict):
            raise BuildError(f'The "native" section of "{folder}/app.json" maps library names to their files or CMake projects.')
        self.native = [NativeLibrary.parse(folder, name, entry, folder / "app.json") for name, entry in native.items()]

        # The native library of a plugin joins the ones of `app.json`, so every platform builds and places it the same way.
        self.plugins, self.plugin_values = load_app_plugins(folder, document.get("plugins", {}), platform)
        for plugin in self.plugins:
            if plugin.native is None:
                continue
            if any(library.name == plugin.native.name for library in self.native):
                raise BuildError(f'The plugin "{plugin.id}" adds the native library "{plugin.native.name}", which "{folder}/app.json" or another plugin already names.')
            self.native.append(plugin.native)

    @property
    def slug(self) -> str:
        return self.folder.name

    @property
    def build_folder(self) -> Path:
        """The folder under `build/apps` that holds everything `make.py` builds for the app, which no other app shares."""
        return APPS_DIR / build_folder_name(self.folder)

    @property
    def version_code(self) -> int:
        """Android needs an integer that grows with every version, so `1.2.3` becomes `1002003`."""
        parts = [int(part) for part in self.version.split(".")] + [0, 0]
        return parts[0] * 1_000_000 + parts[1] * 1_000 + parts[2]


def package_files(folder: Path) -> list[Path]:
    """Lists the files of the package of an app folder: `app.json`, `source`, `content` and, for every plugin that `app.json` lists, its `plugin.json` and its `source` folder."""
    plugins = json.loads((folder / "app.json").read_text()).get("plugins", {})
    if not isinstance(plugins, dict):
        raise BuildError(f'The "plugins" section of "{folder}/app.json" maps plugin ids to objects of parameter values.')

    roots = [folder / name for name in PACKAGE_FOLDERS]
    manifests = []
    for identifier in plugins:
        manifest = folder / "plugins" / identifier / "plugin.json"
        if not manifest.is_file():
            raise BuildError(f'The file "{folder}/app.json" lists the plugin "{identifier}", whose "plugins/{identifier}/plugin.json" does not exist. Add it with "python3 make.py plugin add {identifier} --app {folder}".')
        manifests.append(manifest)
        roots.append(manifest.parent / "source")
    return [folder / "app.json", *sorted([*manifests, *(path for root in roots for path in root.rglob("*") if path.is_file() and path.name != ".DS_Store")])]


def copy_package(app: App, destination: Path) -> list[str]:
    """Copies the package of an app into a folder and returns the copied paths, relative to it."""
    copied: list[str] = []
    for path in package_files(app.folder):
        relative = path.relative_to(app.folder).as_posix()
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        copied.append(relative)
    return copied


def package_folder(folder: Path, output: Path) -> None:
    if not (folder / "app.json").is_file():
        raise BuildError(f'The folder "{folder}" is not an app package because it has no "app.json".')
    files = package_files(folder)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.relative_to(folder).as_posix())
    print(f'Packaged "{folder}" into "{output}".')


# Native libraries: what the `native` section of `app.json` lists, prebuilt or built from a CMake project, placed where each platform package loads it.

NATIVE_PLATFORMS = ("macos", "ios", "tvos", "android", "windows", "linux")
# The slice of `APPLE_SLICES` that each Apple run platform builds, and the target and platform whose embed phase ships its libraries in `App.xcodeproj`.
APPLE_NATIVE_SLICES = {"macos": "macos", "catalyst": "ios-maccatalyst", "ios": "ios", "ios-simulator": "ios-simulator", "tvos": "tvos", "tvos-simulator": "tvos-simulator"}
APPLE_NATIVE_KEYS = {"macos": "macOS-macosx", "ios-maccatalyst": "iOS-macosx", "ios": "iOS-iphoneos", "ios-simulator": "iOS-iphonesimulator", "tvos": "tvOS-appletvos", "tvos-simulator": "tvOS-appletvsimulator"}
XCFRAMEWORK_SLICES = {"macos": ("macos", None), "ios-maccatalyst": ("ios", "maccatalyst"), "ios": ("ios", None), "ios-simulator": ("ios", "simulator"), "tvos": ("tvos", None), "tvos-simulator": ("tvos", "simulator")}
FRAMEWORK_PLATFORMS = {"ios": "iPhoneOS", "ios-simulator": "iPhoneSimulator", "tvos": "AppleTVOS", "tvos-simulator": "AppleTVSimulator"}


@dataclasses.dataclass(frozen=True)
class NativeLibrary:
    """A native library of an app: prebuilt files per platform, or a target of a CMake project that `make.py` builds for each listed platform. On iOS and tvOS, apps may link it statically, and then the symbols that Lua reaches are listed."""

    name: str
    files: dict[str, Path]
    cmake: Path | None
    platforms: tuple[str, ...]
    static: bool
    symbols: tuple[str, ...]

    @staticmethod
    def parse(folder: Path, name: str, entry: object, source: Path) -> "NativeLibrary":
        """Reads an entry of the `native` section of `app.json`, or the `native` section of a plugin, whose paths are relative to the folder of the file."""
        where = f'The native library "{name}" in "{source}"'
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) or not isinstance(entry, dict):
            raise BuildError(f"{where} needs a name of letters, digits and underscores and an object with its files or its CMake project.")
        unknown = set(entry) - {"files", "cmake", "platforms", "link", "symbols"}
        if unknown:
            raise BuildError(f"{where} has unknown keys: {', '.join(f'"{key}"' for key in sorted(unknown))}.")
        if ("files" in entry) == ("cmake" in entry) or ("cmake" in entry) != ("platforms" in entry):
            raise BuildError(f"{where} lists either its prebuilt files by platform or a CMake project with the platforms it builds for.")

        files = {platform: folder / path for platform, path in entry.get("files", {}).items()}
        platforms = tuple(entry.get("platforms", files.keys()))
        if "android" in files and not files["android"].is_dir():
            raise BuildError(f'{where} gives Android a folder with a subfolder of libraries for each ABI, like "jniLibs".')
        for platform in platforms:
            if platform not in NATIVE_PLATFORMS:
                raise BuildError(f"{where} names the unknown platform \"{platform}\". Native libraries ship to {', '.join(f'"{name}"' for name in NATIVE_PLATFORMS)}.")
        for path in files.values():
            if not path.exists():
                raise BuildError(f'{where} lists "{path}", which does not exist.')
        cmake = folder / entry["cmake"] if "cmake" in entry else None
        if cmake is not None and not (cmake / "CMakeLists.txt").is_file():
            raise BuildError(f'{where} names the CMake project "{cmake}", which has no "CMakeLists.txt".')

        link = entry.get("link", "dynamic")
        symbols = tuple(entry.get("symbols", ()))
        if link not in ("dynamic", "static"):
            raise BuildError(f'{where} links "dynamic" or "static", not "{link}".')
        if link == "static" and (not set(platforms) <= {"ios", "tvos"} or not symbols):
            raise BuildError(f"{where} links statically, which iOS and tvOS apps do, and lists the symbols that Lua calls.")
        return NativeLibrary(name, files, cmake, platforms, link == "static", symbols)

    def ships_to(self, platform: str) -> bool:
        return platform in self.platforms


def build_native_target(library: NativeLibrary, directory: Path, options: list[str], jobs: int) -> Path:
    """Configures and builds the CMake target of a library, which shares the name of the library, and returns the folder with its output. The variable `BUILD_SHARED_LIBS` picks the kind of library, and `HAYLEN_INCLUDE_DIR` leads to `haylen/platform/native/HaylenNative.h`."""
    output = directory / "out"
    command = ["cmake", "-S", library.cmake, "-B", directory / "build", "-DCMAKE_BUILD_TYPE=Release", f"-DBUILD_SHARED_LIBS={'OFF' if library.static else 'ON'}", f"-DHAYLEN_INCLUDE_DIR={ENGINE_DIR / 'include'}"]
    command += [f"-DCMAKE_{kind}_OUTPUT_DIRECTORY{suffix}={output}" for kind in ("LIBRARY", "ARCHIVE", "RUNTIME") for suffix in ("", "_RELEASE")]
    if host_name() != "windows" or shutil.which("ninja"):
        command += ["-G", "Ninja"]
    run(command + options)
    run(["cmake", "--build", directory / "build", "--config", "Release", "--target", library.name, "--parallel", str(jobs)])
    return output


def build_apple_native(app: App, library: NativeLibrary, slice_name: str, jobs: int) -> Path:
    """Builds a library for every architecture of an Apple slice and joins them. A dynamic library for iOS or tvOS becomes the framework bundle those systems load."""
    folder = app.build_folder / "native" / library.name / slice_name
    extension = "a" if library.static else "dylib"
    built = [build_native_target(library, folder / arch, apple_slice_options(slice_name, arch), jobs) / f"lib{library.name}.{extension}" for arch in APPLE_SLICES[slice_name]]
    joined = folder / f"lib{library.name}.{extension}"
    run(["lipo", "-create", *built, "-output", joined])
    if library.static or slice_name not in FRAMEWORK_PLATFORMS:
        return joined

    framework = folder / f"{library.name}.framework"
    shutil.rmtree(framework, ignore_errors=True)
    framework.mkdir()
    shutil.copy2(joined, framework / library.name)
    run(["install_name_tool", "-id", f"@rpath/{library.name}.framework/{library.name}", framework / library.name])
    system = "iOS" if slice_name.startswith("ios") else "tvOS"
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleExecutable": library.name,
        "CFBundleIdentifier": f"{app.identifier}.native.{library.name.replace('_', '-')}",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": library.name,
        "CFBundlePackageType": "FMWK",
        "CFBundleShortVersionString": app.version,
        "CFBundleSupportedPlatforms": [FRAMEWORK_PLATFORMS[slice_name]],
        "CFBundleVersion": app.version,
        "MinimumOSVersion": APPLE_MINIMUM_VERSIONS[system],
    }
    (framework / "Info.plist").write_bytes(plistlib.dumps(info, sort_keys=True))
    return framework


def prebuilt_apple_native(path: Path, slice_name: str) -> Path:
    """Returns the library of a prebuilt file for an Apple slice, which an xcframework holds among its slices."""
    if path.suffix != ".xcframework":
        return path
    platform, variant = XCFRAMEWORK_SLICES[slice_name]
    for entry in plistlib.loads((path / "Info.plist").read_bytes())["AvailableLibraries"]:
        if entry["SupportedPlatform"] == platform and entry.get("SupportedPlatformVariant") == variant:
            return path / entry["LibraryIdentifier"] / entry["LibraryPath"]
    raise BuildError(f'The xcframework "{path}" has no slice for "{slice_name}".')


def copy_into(source: Path, folder: Path) -> Path:
    """Copies a file or a bundle, such as a library, a framework or a resource bundle, into a folder, keeping the links inside bundles."""
    folder.mkdir(parents=True, exist_ok=True)
    destination = folder / source.name
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=True, dirs_exist_ok=True)
    else:
        shutil.copy2(source, destination)
    return destination


def prepare_apple_native(app: App, project: Path, run_platform: str, jobs: int) -> list[str]:
    """Places the libraries of an Apple run in `native/` with the file lists of the embed phase of every target and platform, writes the table of linked symbols into `source/` and returns the settings that link static libraries."""
    slice_name = APPLE_NATIVE_SLICES[run_platform]
    key = APPLE_NATIVE_KEYS[slice_name]
    platform = "macos" if slice_name == "macos" else slice_name.split("-")[0]
    embedded: list[Path] = []
    linked: list[tuple[NativeLibrary, Path]] = []
    for library in app.native:
        if not library.ships_to(platform):
            continue
        if library.static and slice_name == "ios-maccatalyst":
            raise BuildError(f'Mac Catalyst loads dynamic libraries only, so the static library "{library.name}" does not ship there.')
        source = build_apple_native(app, library, slice_name, jobs) if library.cmake else prebuilt_apple_native(library.files[platform], slice_name)
        placed = copy_into(source, project / "native" / key)
        if library.static:
            linked.append((library, placed))
        else:
            embedded.append(placed)

    for list_key in APPLE_NATIVE_KEYS.values():
        shipped = embedded if list_key == key else []
        write_if_changed(project / "native" / f"{list_key}.xcfilelist", "".join(f"$(PROJECT_DIR)/native/{list_key}/{path.name}\n" for path in shipped))
        write_if_changed(project / "native" / f"{list_key}-output.xcfilelist", "".join(f"$(TARGET_BUILD_DIR)/$(FRAMEWORKS_FOLDER_PATH)/{path.name}\n" for path in shipped))
    if not linked:
        return []

    # Dead code stripping keeps what the table refers to, and the table lets `haylen.native` find the symbols without the app exporting them.
    target_name, platform_name = key.split("-")
    condition = "TARGET_OS_IOS && !TARGET_OS_MACCATALYST" if platform == "ios" else "TARGET_OS_TV"
    declarations = [f'extern "C" void {symbol}(void);' for library, _ in linked for symbol in library.symbols]
    registrations = []
    for library, _ in linked:
        entries = ", ".join(f'{{"{symbol}", reinterpret_cast<void*>(&{symbol})}}' for symbol in library.symbols)
        registrations.append(f'    haylen::platform::NativeLibraries::registerLinked("{library.name}", {{{entries}}});')
    table = [
        "// Written by `make.py` from the `native` section of `app.json`. It links the symbols of the static native libraries into the app and registers them for `haylen.native`.",
        "#import <Foundation/Foundation.h>",
        "#include <TargetConditionals.h>",
        "",
        '#include "haylen/platform/NativeLibraries.hpp"',
        "",
        f"#if {condition}",
        *declarations,
        "#endif",
        "",
        "@interface HaylenNativeSymbols : NSObject",
        "@end",
        "",
        "@implementation HaylenNativeSymbols",
        "",
        "+ (void)load {",
        f"#if {condition}",
        *registrations,
        "#endif",
        "}",
        "",
        "@end",
        "",
    ]
    write_if_changed(project / "source" / "HaylenNativeSymbols.mm", "\n".join(table))
    flags = " ".join(f'"$(PROJECT_DIR)/native/{key}/{path.name}"' for _, path in linked)
    return [f"HAYLEN_NATIVE_LDFLAGS_{target_name}_{platform_name} = {flags}", "OTHER_LDFLAGS = $(inherited) $(HAYLEN_NATIVE_LDFLAGS_$(TARGET_NAME)_$(PLATFORM_NAME))"]


def prepare_android_native(app: App, project: Path, jobs: int) -> None:
    """Places the libraries of an app in the `jniLibs` folders of the Android project, building each ABI one after the other."""
    libraries = project / "app" / "src" / "main" / "jniLibs"
    for library in app.native:
        if not library.ships_to("android"):
            continue
        if library.cmake is None:
            shutil.copytree(library.files["android"], libraries, dirs_exist_ok=True)
            continue
        for abi in ANDROID_ABIS:
            output = build_native_target(library, app.build_folder / "native" / library.name / f"android-{abi}", android_options(abi), jobs)
            copy_into(output / f"lib{library.name}.so", libraries / abi)


def prepare_host_native(app: App, folder: Path, jobs: int) -> list[Path]:
    """Places the libraries of an app for this desktop in a folder and returns them."""
    platform = host_name()
    placed = []
    for library in app.native:
        if not library.ships_to(platform):
            continue
        if library.cmake is None:
            placed.append(copy_into(prebuilt_apple_native(library.files[platform], "macos") if platform == "macos" else library.files[platform], folder))
            continue
        options = [f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}"] if platform == "macos" else []
        output = build_native_target(library, app.build_folder / "native" / library.name / platform, options, jobs)
        built = sorted(output.glob({"macos": f"lib{library.name}.dylib", "windows": f"*{library.name}.dll", "linux": f"lib{library.name}.so"}[platform]))
        if not built:
            raise BuildError(f'The CMake target "{library.name}" of "{library.cmake}" built no shared library into "{output}".')
        placed.append(copy_into(built[0], folder))
    return placed


# Plugins: folders under `plugins/` of an app that give Lua a capability implemented natively on each platform, which `make.py` validates, packages and assembles into the platform projects.

PLUGIN_PLATFORMS = ("ios", "catalyst", "tvos", "macos", "android", "web", "windows", "linux")
PLUGIN_KEYS = {"id", "name", "version", "description", "platforms", "requires", "parameters", "apple", "android", "web", "native"}
PLUGIN_ID = re.compile(r"[a-z][a-z0-9]*(-[a-z0-9]+)*")
PARAMETER_NAME = re.compile(r"[a-z][A-Za-z0-9]*")
# A `${name}` in the `apple` and `android` sections names a parameter, while shell expansions such as `${BUILD_DIR%/Build/*}` are no references and stay as they are.
PARAMETER_REFERENCE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
# Each parameter type with the words that describe its values and the check of a value.
PARAMETER_TYPES: dict[str, tuple[str, Callable[[object], bool]]] = {
    "string": ("a text", lambda value: isinstance(value, str)),
    "number": ("a number", lambda value: isinstance(value, (int, float)) and not isinstance(value, bool)),
    "integer": ("an integer", lambda value: isinstance(value, int) and not isinstance(value, bool)),
    "boolean": ('"true" or "false"', lambda value: isinstance(value, bool)),
    "array": ("an array", lambda value: isinstance(value, list)),
    "object": ("an object", lambda value: isinstance(value, dict)),
    "file": ("the path of a file relative to the app folder", lambda value: isinstance(value, str) and value != ""),
}
PACKAGE_URL = re.compile(r"(https://|ssh://|git@)\S+")
EXACT_VERSION = re.compile(r"\d+\.\d+\.\d+([-+][0-9A-Za-z.-]+)?")
SYSTEM_FRAMEWORK = re.compile(r"[A-Za-z0-9_+.-]+\.(framework|tbd)")
OBJC_CLASS = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
GRADLE_PLUGIN_ID = re.compile(r"[A-Za-z][A-Za-z0-9_-]*(\.[A-Za-z][A-Za-z0-9_-]*)+")
GRADLE_PLUGIN_VERSION = re.compile(r"[0-9A-Za-z._+-]+")
PLACEHOLDER_NAME = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
# The targets of the Apple project and the plugin platforms each one builds for, since the iOS target also builds the Mac Catalyst app. The folder of the `Info.plist` of a target is its name in lowercase.
APPLE_PLUGIN_TARGETS = {"iOS": ("ios", "catalyst"), "tvOS": ("tvos",), "macOS": ("macos",)}
# The entitlements file of each Apple plugin platform, `<platform>/App.entitlements`, signs these targets and SDK platforms.
APPLE_ENTITLEMENTS = {"ios": ("iOS_iphoneos", "iOS_iphonesimulator"), "catalyst": ("iOS_macosx",), "tvos": ("tvOS_appletvos", "tvOS_appletvsimulator"), "macos": ("macOS_macosx",)}


@dataclasses.dataclass(frozen=True, eq=False)
class Plugin:
    """A plugin folder with its manifest, which `make.py` validated, and the native library it adds to the app."""

    folder: Path
    manifest: dict
    native: NativeLibrary | None

    @property
    def id(self) -> str:
        return self.manifest["id"]

    @property
    def version(self) -> str:
        return self.manifest["version"]

    @property
    def requires(self) -> list[str]:
        return self.manifest.get("requires", [])

    @property
    def parameters(self) -> dict[str, dict]:
        return self.manifest.get("parameters", {})

    def supports(self, *platforms: str) -> bool:
        return any(platform in self.manifest["platforms"] for platform in platforms)

    @staticmethod
    def load(folder: Path) -> "Plugin":
        """Reads the `plugin.json` of a plugin folder and fails with every problem it finds, each with the path and the key."""
        path = folder / "plugin.json"
        if not path.is_file():
            raise BuildError(f'The folder "{folder}" is no plugin, because it has no "plugin.json".')
        try:
            manifest = json.loads(path.read_text())
        except json.JSONDecodeError as error:
            raise BuildError(f'The file "{path}" is not valid JSON: {error}.') from error
        problems = PluginManifestCheck.problems_of(folder, manifest)
        if problems:
            raise BuildError("\n".join(f"{path}: {problem}" for problem in problems))
        native = NativeLibrary.parse(folder, manifest["id"].replace("-", "_"), manifest["native"], path) if "native" in manifest else None
        return Plugin(folder, manifest, native)


class PluginManifestCheck:
    """Checks a `plugin.json` against the plugin format and collects every problem as its key and a sentence."""

    def __init__(self, folder: Path, manifest: dict) -> None:
        self.folder = folder
        self.manifest = manifest
        self.problems: list[str] = []
        self.identifier = manifest.get("id")
        self.platforms: list[str] = []
        self.parameters: dict = {}

    @staticmethod
    def problems_of(folder: Path, manifest: object) -> list[str]:
        if not isinstance(manifest, dict):
            return ['The file "plugin.json" must hold an object.']
        check = PluginManifestCheck(folder, manifest)
        check.check_identity()
        check.check_parameters()
        check.check_apple()
        check.check_android()
        check.check_web()
        check.check_native()
        return check.problems

    def report(self, key: str, message: str) -> None:
        self.problems.append(f'The key "{key}" {message}')

    def check_keys(self, key: str, section: dict, allowed: set[str], required: set[str]) -> None:
        for name in sorted(set(section) - allowed):
            self.report(f"{key}.{name}" if key else name, 'is not a key of "plugin.json".')
        for name in sorted(required - set(section)):
            self.report(f"{key}.{name}" if key else name, "is missing.")

    def is_object(self, key: str, value: object, message: str) -> bool:
        if not isinstance(value, dict):
            self.report(key, message)
        return isinstance(value, dict)

    def check_texts(self, key: str, value: object, pattern: re.Pattern | None, message: str) -> None:
        if not isinstance(value, list) or not all(isinstance(item, str) and item and (pattern is None or pattern.fullmatch(item)) for item in value):
            self.report(key, message)

    def check_path(self, key: str, value: object, kind: str) -> bool:
        """Checks a path of the plugin, which stays inside its folder and names an existing file, folder, or file or folder."""
        if not isinstance(value, str) or not value or Path(value).is_absolute() or ".." in Path(value).parts:
            self.report(key, "must be a path inside the plugin folder.")
            return False
        target = self.folder / value
        if not target.exists() or (kind == "file" and not target.is_file()) or (kind == "folder" and not target.is_dir()):
            self.report(key, f'names "{value}", which is no {kind} of the plugin.')
            return False
        return True

    def check_source(self, key: str, value: object) -> None:
        """Checks a file that `make.py` copies into a project, which is a path inside the plugin or a reference to a file parameter, and so a file of the app."""
        reference = PARAMETER_REFERENCE.fullmatch(value) if isinstance(value, str) else None
        if reference is None:
            self.check_path(key, value, "file or folder")
        elif self.parameters.get(reference[1], {}).get("type") not in (None, "file"):
            self.report(key, f'references "${{{reference[1]}}}", which is no file parameter.')

    def check_references(self, key: str, value: object) -> None:
        if isinstance(value, str):
            for name in PARAMETER_REFERENCE.findall(value):
                if name not in self.manifest.get("parameters", {}):
                    self.report(key, f'references "${{{name}}}", which is no parameter of the plugin.')
        elif isinstance(value, list):
            for index, item in enumerate(value):
                self.check_references(f"{key}[{index}]", item)
        elif isinstance(value, dict):
            for name, item in value.items():
                self.check_references(f"{key}.{name}", item)

    def check_identity(self) -> None:
        self.check_keys("", self.manifest, PLUGIN_KEYS, {"id", "name", "version", "description", "platforms"})
        if "id" in self.manifest and (not isinstance(self.identifier, str) or not PLUGIN_ID.fullmatch(self.identifier)):
            self.report("id", 'must be in "dash-case", such as "firebase-analytics".')
        elif "id" in self.manifest and self.identifier != self.folder.name:
            self.report("id", f'must match the name of the plugin folder, "{self.folder.name}".')
        for key in ("name", "description"):
            if key in self.manifest and (not isinstance(self.manifest[key], str) or not self.manifest[key]):
                self.report(key, "must be a text.")
        version = self.manifest.get("version")
        if "version" in self.manifest and not (isinstance(version, str) and re.fullmatch(r"\d+(\.\d+){0,2}", version)):
            self.report("version", 'must be one to three numbers separated by dots, such as "1.2.0".')

        platforms = self.manifest.get("platforms", [])
        self.platforms = [platform for platform in platforms if platform in PLUGIN_PLATFORMS] if isinstance(platforms, list) else []
        if not platforms or self.platforms != platforms or len(set(self.platforms)) != len(self.platforms):
            self.report("platforms", f"must list each platform of the plugin once, among {', '.join(f'"{name}"' for name in PLUGIN_PLATFORMS)}.")
        requires = self.manifest.get("requires", [])
        if not isinstance(requires, list) or not all(isinstance(required, str) and PLUGIN_ID.fullmatch(required) and required != self.identifier for required in requires):
            self.report("requires", "must list the ids of other plugins that the app needs as well.")

    def check_parameters(self) -> None:
        declared = self.manifest.get("parameters", {})
        if not self.is_object("parameters", declared, "must map parameter names to their descriptions."):
            return
        self.parameters = {name: parameter for name, parameter in declared.items() if isinstance(parameter, dict)}
        for name, parameter in declared.items():
            key = f"parameters.{name}"
            if not PARAMETER_NAME.fullmatch(name):
                self.report(key, 'must be named in "camelCase", such as "iosAppId".')
            if not self.is_object(key, parameter, "must be an object with the type and the description of the parameter."):
                continue
            self.check_keys(key, parameter, {"type", "platforms", "required", "default", "description"}, {"type", "description"})
            kind = parameter.get("type")
            if "type" in parameter and kind not in PARAMETER_TYPES:
                self.report(f"{key}.type", f"must be one of {', '.join(f'"{name}"' for name in PARAMETER_TYPES)}.")
            scope = parameter.get("platforms")
            if "platforms" in parameter and (not isinstance(scope, list) or not scope or not all(platform in self.platforms for platform in scope)):
                self.report(f"{key}.platforms", "must list platforms of the plugin.")
            if "required" in parameter and not isinstance(parameter["required"], bool):
                self.report(f"{key}.required", 'must be "true" or "false".')
            if "description" in parameter and (not isinstance(parameter["description"], str) or not parameter["description"]):
                self.report(f"{key}.description", "must be a text.")
            if "default" in parameter and parameter.get("required") is True:
                self.report(f"{key}.default", 'belongs to optional parameters only, because "app.json" always gives a required parameter its value.')
            elif "default" in parameter and kind in PARAMETER_TYPES and not PARAMETER_TYPES[kind][1](parameter["default"]):
                self.report(f"{key}.default", f"must be {PARAMETER_TYPES[kind][0]}.")

    def check_apple(self) -> None:
        apple = self.manifest.get("apple")
        if "apple" not in self.manifest or not self.is_object("apple", apple, "must be an object with the Apple part of the plugin."):
            return
        if not any(platform in self.platforms for platform in ("ios", "catalyst", "tvos", "macos")):
            self.report("apple", 'needs "ios", "catalyst", "tvos" or "macos" in "platforms".')
        self.check_keys("apple", apple, {"class", "sources", "packages", "frameworks", "infoPlist", "entitlements", "resources", "buildScripts"}, set())
        if "class" in apple and not (isinstance(apple["class"], str) and OBJC_CLASS.fullmatch(apple["class"])):
            self.report("apple.class", 'must be the Objective-C name of the plugin class, such as "HaylenAdMobPlugin".')
        if "sources" in apple:
            self.check_path("apple.sources", apple["sources"], "folder")

        packages = apple.get("packages", {})
        if self.is_object("apple.packages", packages, 'must map Swift package names to their "url", "exactVersion" and "products".'):
            for name, package in packages.items():
                key = f"apple.packages.{name}"
                if not self.is_object(key, package, 'must be an object with the "url", "exactVersion" and "products" of the package.'):
                    continue
                self.check_keys(key, package, {"url", "exactVersion", "products"}, {"url", "exactVersion", "products"})
                if "url" in package and not (isinstance(package["url"], str) and PACKAGE_URL.fullmatch(package["url"])):
                    self.report(f"{key}.url", "must be the https or ssh URL of the package repository.")
                if "exactVersion" in package and not (isinstance(package["exactVersion"], str) and EXACT_VERSION.fullmatch(package["exactVersion"])):
                    self.report(f"{key}.exactVersion", 'must be one exact version, such as "13.10.0".')
                if "products" in package and not package["products"]:
                    self.report(f"{key}.products", "must list the products of the package that the app links.")
                elif "products" in package:
                    self.check_texts(f"{key}.products", package["products"], None, "must list the products of the package that the app links.")

        if "frameworks" in apple:
            self.check_texts("apple.frameworks", apple["frameworks"], SYSTEM_FRAMEWORK, 'must list system frameworks and libraries, such as "StoreKit.framework" or "libz.tbd".')
        for key in ("infoPlist", "entitlements"):
            if key in apple:
                self.is_object(f"apple.{key}", apple[key], "must be an object of keys and values.")
        resources = apple.get("resources", [])
        if not isinstance(resources, list):
            self.report("apple.resources", "must list the files that the app bundle holds at its root.")
        for index, resource in enumerate(resources if isinstance(resources, list) else []):
            self.check_source(f"apple.resources[{index}]", resource)
        self.check_build_scripts(apple.get("buildScripts", []))
        self.check_references("apple", apple)

    def check_build_scripts(self, scripts: object) -> None:
        if not isinstance(scripts, list):
            self.report("apple.buildScripts", "must list build phases.")
            return
        for index, script in enumerate(scripts):
            key = f"apple.buildScripts[{index}]"
            if not self.is_object(key, script, "must be an object with the name and the script of a build phase."):
                continue
            self.check_keys(key, script, {"name", "script", "inputFiles", "outputFiles"}, {"name", "script"})
            for field in ("name", "script"):
                if field in script and (not isinstance(script[field], str) or not script[field]):
                    self.report(f"{key}.{field}", "must be a text.")
            for field in ("inputFiles", "outputFiles"):
                if field in script:
                    self.check_texts(f"{key}.{field}", script[field], None, "must list paths.")

    def check_android(self) -> None:
        android = self.manifest.get("android")
        if "android" not in self.manifest or not self.is_object("android", android, "must be an object with the Android part of the plugin."):
            return
        if "android" not in self.platforms:
            self.report("android", 'needs "android" in "platforms".')
        self.check_keys("android", android, {"module", "gradlePlugins", "placeholders", "files"}, {"module"})
        if "module" in android and self.check_path("android.module", android["module"], "folder"):
            module = self.folder / android["module"]
            if not any((module / name).is_file() for name in ("build.gradle.kts", "build.gradle")) or not (module / "src" / "main" / "AndroidManifest.xml").is_file():
                self.report("android.module", 'must be an Android library module with a "build.gradle.kts" and "src/main/AndroidManifest.xml".')

        gradle_plugins = android.get("gradlePlugins", [])
        if not isinstance(gradle_plugins, list) or not all(isinstance(entry, dict) and set(entry) == {"id", "version"} and isinstance(entry["id"], str) and GRADLE_PLUGIN_ID.fullmatch(entry["id"]) and isinstance(entry["version"], str) and GRADLE_PLUGIN_VERSION.fullmatch(entry["version"]) for entry in gradle_plugins):
            self.report("android.gradlePlugins", 'must list the id and the version of each Gradle plugin, such as {"id": "com.google.gms.google-services", "version": "4.5.0"}.')
        placeholders = android.get("placeholders", {})
        if self.is_object("android.placeholders", placeholders, "must map manifest placeholder names to their values."):
            for name, value in placeholders.items():
                if not PLACEHOLDER_NAME.fullmatch(name) or not isinstance(value, str):
                    self.report(f"android.placeholders.{name}", "must be named with letters, digits and underscores and have a text value.")

        files = android.get("files", [])
        if not isinstance(files, list):
            self.report("android.files", "must list the files that the plugin places in the Android project.")
        for index, entry in enumerate(files if isinstance(files, list) else []):
            key = f"android.files[{index}]"
            if not self.is_object(key, entry, 'must be an object with "from" and "to".'):
                continue
            self.check_keys(key, entry, {"from", "to"}, {"from", "to"})
            if "from" in entry:
                self.check_source(f"{key}.from", entry["from"])
            if "to" in entry and (not isinstance(entry["to"], str) or not entry["to"] or Path(entry["to"]).is_absolute() or ".." in Path(entry["to"]).parts):
                self.report(f"{key}.to", 'must be a path inside the Android project, such as "app/google-services.json".')
        self.check_references("android", android)

    def check_web(self) -> None:
        web = self.manifest.get("web")
        if "web" not in self.manifest or not self.is_object("web", web, "must be an object with the module of the plugin."):
            return
        if "web" not in self.platforms:
            self.report("web", 'needs "web" in "platforms".')
        self.check_keys("web", web, {"module"}, {"module"})
        module = web.get("module")
        if "module" in web and (not isinstance(module, str) or not module.startswith("web/") or Path(module).suffix not in (".js", ".mjs")):
            self.report("web.module", 'must be an ES module in the "web" folder of the plugin, such as "web/admob.js".')
        elif "module" in web:
            self.check_path("web.module", module, "file")

    def check_native(self) -> None:
        if "native" not in self.manifest or not isinstance(self.identifier, str) or not PLUGIN_ID.fullmatch(self.identifier):
            return
        try:
            library = NativeLibrary.parse(self.folder, self.identifier.replace("-", "_"), self.manifest["native"], self.folder / "plugin.json")
        except BuildError as error:
            self.problems.append(str(error))
            return
        if not all(platform in self.platforms for platform in library.platforms):
            self.report("native.platforms", "must list platforms of the plugin.")


def parameter_values(folder: Path, plugin: Plugin, given: object, platform: str) -> tuple[dict, list[str]]:
    """Checks the values that `app.json` gives a plugin to build for a platform and returns them with the defaults applied, together with every problem and its key. A parameter applies to the platforms it lists, or else to every platform of the plugin."""
    key = f"plugins.{plugin.id}"
    if not isinstance(given, dict):
        return {}, [f'The key "{key}" must be an object of parameter values.']

    problems = [f'The key "{key}.{name}" is no parameter of the plugin "{plugin.id}".' for name in given if name not in plugin.parameters]
    values = {}
    for name, parameter in plugin.parameters.items():
        description, valid = PARAMETER_TYPES[parameter["type"]]
        applies = platform in parameter.get("platforms", plugin.manifest["platforms"])
        value = given.get(name, parameter.get("default"))
        # The command `make.py plugin add` writes an empty text for every required parameter, which is the value the developer still has to fill in.
        if value is None or (value == "" and parameter.get("required")):
            if applies and parameter.get("required"):
                problems.append(f"The key \"{key}.{name}\" needs a value to build for \"{platform}\". {parameter['description']}")
            continue
        if not valid(value):
            problems.append(f'The key "{key}.{name}" must be {description}.')
        elif parameter["type"] == "file" and applies and not (folder / value).is_file():
            problems.append(f'The key "{key}.{name}" names "{value}", which is no file of the app folder.')
        else:
            values[name] = value
    return values, problems


def plugin_order(plugins: dict[str, Plugin]) -> list[Plugin]:
    """Orders plugins so that every plugin follows the plugins it requires, and otherwise keeps the order of `app.json`."""
    ordered: list[Plugin] = []
    visiting: list[str] = []

    def visit(plugin: Plugin) -> None:
        if plugin in ordered:
            return
        if plugin.id in visiting:
            raise BuildError(f"The plugins {', '.join(f'"{name}"' for name in visiting[visiting.index(plugin.id) :])} require each other in a cycle.")
        visiting.append(plugin.id)
        for required in plugin.requires:
            visit(plugins[required])
        visiting.pop()
        ordered.append(plugin)

    for plugin in plugins.values():
        visit(plugin)
    return ordered


def load_app_plugins(folder: Path, section: object, platform: str) -> tuple[list[Plugin], dict[str, dict]]:
    """Loads the plugins that `app.json` lists from `plugins/` of the app and checks the values it gives them to build for a platform. Returns the plugins in load order and their values with the defaults applied, or fails with every problem."""
    where = folder / "app.json"
    if not isinstance(section, dict):
        raise BuildError(f'{where}: The key "plugins" must map plugin ids to objects of parameter values.')

    problems: list[str] = []
    plugins: dict[str, Plugin] = {}
    for identifier in section:
        if not (folder / "plugins" / identifier / "plugin.json").is_file():
            problems.append(f'{where}: The key "plugins.{identifier}" names no plugin of the app, because "plugins/{identifier}/plugin.json" does not exist. Add it with "python3 make.py plugin add {identifier} --app {folder}".')
            continue
        try:
            plugins[identifier] = Plugin.load(folder / "plugins" / identifier)
        except BuildError as error:
            problems.append(str(error))

    values: dict[str, dict] = {}
    for plugin in plugins.values():
        problems += [f'{where}: The plugin "{plugin.id}" requires the plugin "{required}", which "app.json" does not list.' for required in plugin.requires if required not in section]
        values[plugin.id], found = parameter_values(folder, plugin, section[plugin.id], platform)
        problems += [f"{where}: {problem}" for problem in found]
    if problems:
        raise BuildError("\n".join(problems))
    return plugin_order(plugins), values


def parameter_text(value: object) -> str:
    return value if isinstance(value, str) else json.dumps(value)


def substitute_parameters(value: object, values: dict) -> object:
    """Replaces every `${name}` in the strings of a section of `plugin.json` with the value of the parameter. A string that is only a reference takes the value with its type, and a string whose references lack a value gives `None`, which leaves its key or item out."""
    if isinstance(value, str):
        if not all(name in values for name in PARAMETER_REFERENCE.findall(value)):
            return None
        whole = PARAMETER_REFERENCE.fullmatch(value)
        return values[whole[1]] if whole else PARAMETER_REFERENCE.sub(lambda match: parameter_text(values[match[1]]), value)
    if isinstance(value, list):
        return [item for item in (substitute_parameters(item, values) for item in value) if item is not None]
    if isinstance(value, dict):
        return {key: item for key, item in ((key, substitute_parameters(item, values)) for key, item in value.items()) if item is not None}
    return value


def plugin_section(app: App, plugin: Plugin, name: str) -> dict | None:
    """Returns the `apple` or `android` section of a plugin with the values that `app.json` gives its parameters, or `None` when the plugin has none."""
    return substitute_parameters(plugin.manifest[name], app.plugin_values[plugin.id]) if name in plugin.manifest else None


def plugin_file(app: App, plugin: Plugin, value: str) -> Path | None:
    """Resolves a file that a plugin copies into a project: a reference to a file parameter names a file of the app, or nothing while the parameter has no value, and any other path names a file of the plugin."""
    reference = PARAMETER_REFERENCE.fullmatch(value)
    if reference is None:
        return plugin.folder / value
    given = app.plugin_values[plugin.id].get(reference[1])
    if given is None:
        return None
    if not (app.folder / given).exists():
        raise BuildError(f'The parameter "{reference[1]}" of the plugin "{plugin.id}" names "{given}", which is no file of "{app.folder}".')
    return app.folder / given


def merge_plugin_keys(merged: dict, values: dict, owners: dict[str, str], owner: str, label: str, top: str | None = None) -> None:
    """Merges the `Info.plist` or entitlements keys of a plugin into the keys that `make.py` and earlier plugins set: objects merge key by key, arrays gain the items they lack, and any other value that differs fails the build."""
    for key, value in values.items():
        first = top or key
        if key not in merged:
            merged[key] = value
            owners.setdefault(first, owner)
        elif isinstance(merged[key], dict) and isinstance(value, dict):
            merge_plugin_keys(merged[key], value, owners, owner, label, first)
        elif isinstance(merged[key], list) and isinstance(value, list):
            merged[key] += [item for item in value if item not in merged[key]]
        elif merged[key] != value:
            raise BuildError(f'The {label} key "{key}" is {json.dumps(merged[key])} for {owners[first]} and {json.dumps(value)} for {owner}.')


# Shaders: annotated GLSL under `content/shaders` compiled ahead of time into one `.shader` file per source, because no platform, the web editor included, compiles shaders at runtime.


def parse_shdc_yaml(text: str) -> dict:
    """Reads the reflection YAML that sokol-shdc writes with `--format bare_yaml`, whose maps and lists nest by two spaces and whose lists put every item under a lone dash."""
    lines = [(len(line) - len(line.lstrip(" ")), line.strip()) for line in text.splitlines() if line.strip()]

    def scalar(value: str):
        if value in ("true", "false"):
            return value == "true"
        return int(value) if re.fullmatch(r"-?\d+", value) else value

    def block(index: int, indent: int) -> tuple[object, int]:
        if lines[index][1] == "-":
            items = []
            while index < len(lines) and lines[index] == (indent, "-"):
                item, index = block(index + 1, lines[index + 1][0])
                items.append(item)
            return items, index
        mapping = {}
        while index < len(lines) and lines[index][0] == indent:
            key, _, rest = lines[index][1].partition(":")
            if rest.strip():
                mapping[key] = scalar(rest.strip())
                index += 1
            else:
                mapping[key], index = block(index + 1, lines[index + 1][0])
        return mapping, index

    return block(0, 0)[0]


def reflect_uniform_members(header: str, program: str) -> dict[str, list[dict]]:
    """Reads the names, types, element counts and offsets of the uniform block members from the reflection functions of a sokol-shdc C header."""
    types = {"FLOAT": "float", "FLOAT2": "vec2", "FLOAT3": "vec3", "FLOAT4": "vec4", "INT": "int", "INT2": "ivec2", "INT3": "ivec3", "INT4": "ivec4", "MAT4": "mat4"}
    members: dict[str, dict[str, dict]] = {}
    for function in ("uniform_offset", "uniform_desc"):
        start = header.index(f"{program}_{function}(")
        block = member = None
        for line in header[start : header.index("\n}\n", start)].splitlines():
            if found := re.search(r'strcmp\(ub_name, "(\w+)"\)', line):
                block = found[1]
            elif found := re.search(r'strcmp\(u_name, "(\w+)"\)', line):
                member = found[1]
                members.setdefault(block, {}).setdefault(member, {"name": member})
            elif found := re.search(r"return (\d+);", line):
                members[block][member]["offset"] = int(found[1])
            elif found := re.search(r"res\.type = SG_UNIFORMTYPE_(\w+);", line):
                members[block][member]["type"] = types[found[1]]
            elif found := re.search(r"res\.array_count = (\d+);", line):
                members[block][member]["count"] = max(1, int(found[1]))
    return {block: list(entries.values()) for block, entries in members.items()}


def describe_shader_program(program: dict, sources: list[str], known: dict[str, int]) -> dict:
    """Turns the reflection of one program for one shader language into the description the engine builds its GPU program from, with the sources in a shared list."""

    def stage(function: dict) -> dict:
        text = Path(function["path"]).read_text()
        if text not in known:
            known[text] = len(sources)
            sources.append(text)
        described = {"source": known[text], "entry": function["entry_point"]}
        if "d3d11_target" in function:
            described["d3d11_target"] = function["d3d11_target"]
        return described

    def pick(entry: dict, keys: tuple[str, ...]) -> dict:
        return {key: entry[key] for key in keys if key in entry}

    bindings = ("hlsl_register_b_n", "msl_buffer_n", "wgsl_group0_binding_n", "hlsl_register_t_n", "msl_texture_n", "wgsl_group1_binding_n", "hlsl_register_s_n", "msl_sampler_n")
    return {
        "vertex": stage(program["vertex_func"]),
        "fragment": stage(program["fragment_func"]),
        "attrs": [{**pick(attr, ("slot", "glsl_name", "hlsl_sem_name", "hlsl_sem_index")), "base_type": {"Float": "float", "Int": "sint", "UInt": "uint"}[attr["base_type"]]} for attr in program.get("attrs", [])],
        "uniform_blocks": [{**pick(block, ("slot", "stage", "size", *bindings)), "glsl_uniforms": [pick(uniform, ("type", "array_count", "glsl_name")) for uniform in block.get("glsl_uniforms", [])]} for block in program.get("uniform_blocks", [])],
        "views": [{**pick(view["texture"], ("slot", "stage", "sample_type", "multisampled", *bindings)), "image_type": view["texture"]["type"]} for view in program.get("views", [])],
        "samplers": [pick(sampler, ("slot", "stage", "sampler_type", *bindings)) for sampler in program.get("samplers", [])],
        "texture_sampler_pairs": [pick(pair, ("slot", "stage", "view_slot", "sampler_slot", "glsl_name")) for pair in program.get("texture_sampler_pairs", [])],
    }


def compile_shader(source: Path, output: Path) -> None:
    """Compiles one annotated GLSL source into a `.shader` file with every program the engine draws with, for every shader language it supports, and the reflection of the uniforms and textures of the source."""
    text = source.read_text()
    declared = re.findall(r"^\s*@program\s+(\w+)\s+(\w+)\s+(\w+)", text, re.MULTILINE)
    if len(declared) != 1 or declared[0][1] != "haylen_vs":
        raise BuildError(f'The shader "{source}" must declare exactly one "@program" whose vertex shader is "haylen_vs" from "haylen/material.glsl".')
    name = declared[0][0]

    def shdc(arguments: list, program: str) -> None:
        compiled = subprocess.run([str(part) for part in [ensure_shdc(), "--input", source.resolve(), *arguments]], cwd=SHADER_LIBRARY_DIR, capture_output=True, text=True)
        if compiled.returncode != 0:
            raise BuildError(f'The sokol-shdc tool could not compile the "{program}" program of "{source}":\n{compiled.stdout}{compiled.stderr}'.rstrip())

    sources: list[str] = []
    known: dict[str, int] = {}
    programs: dict[str, dict] = {}
    with tempfile.TemporaryDirectory() as temporary:
        folder = Path(temporary)
        for program, defines in SHADER_PROGRAMS.items():
            shdc(["--output", folder / program, "--slang", SHADER_SLANGS, "--format", "bare_yaml", *([f"--defines={':'.join(defines)}"] if defines else [])], program)
            reflection = parse_shdc_yaml((folder / f"{program}_reflection.yaml").read_text())
            programs[program] = {shader["slang"]: describe_shader_program(shader["programs"][0], sources, known) for shader in reflection["shaders"]}

        # The C header of one language holds the members of every uniform block, which the reflection YAML leaves out.
        shdc(["--output", folder / "reflection.h", "--slang", "glsl430", "--reflection"], "sprite")
        members = reflect_uniform_members((folder / "reflection.h").read_text(), name)
        reflection = parse_shdc_yaml((folder / "sprite_reflection.yaml").read_text())["shaders"][0]["programs"][0]

    blocks = [{"name": block["struct_name"], "slot": block["slot"], "size": block["size"], "uniforms": members.get(block["struct_name"], [])} for block in reflection.get("uniform_blocks", []) if block["stage"] == "fragment" and block["struct_name"] not in SHADER_ENGINE_BLOCKS]
    textures = [{"name": view["texture"]["name"], "slot": view["texture"]["slot"]} for view in reflection.get("views", []) if view["texture"]["name"] not in SHADER_ENGINE_TEXTURES]
    document = {"format": "haylen-shader", "version": 1, "name": name, "blocks": blocks, "textures": textures, "sources": sources, "programs": programs}
    output.write_text(json.dumps(document, separators=(",", ":")))
    print(f'Compiled "{source}" into "{output}".', flush=True)


def shader_sources(folder: Path) -> list[Path]:
    """Lists the sources of an app that declare a program, which are the ones `make.py` compiles, while the others are files they include."""
    shaders = folder / "content" / "shaders"
    return sorted(path for path in shaders.rglob("*.glsl") if re.search(r"^\s*@program\b", path.read_text(), re.MULTILINE)) if shaders.is_dir() else []


def newest_shader_source(folder: Path) -> float:
    """Returns the time of the newest shader source of an app or the shader library, which every compiled shader of the app must be newer than."""
    return max(path.stat().st_mtime for path in [*(folder / "content" / "shaders").rglob("*.glsl"), *SHADER_LIBRARY_DIR.rglob("*.glsl")])


def compile_app_shaders(folder: Path) -> None:
    """Compiles the shaders of an app whose `.shader` file is older than its source, the other sources of the app that it may include or the shader library."""
    if not (folder / "content" / "shaders").is_dir():
        return
    newest = newest_shader_source(folder)
    for source in shader_sources(folder):
        output = source.with_suffix(".shader")
        if not output.is_file() or output.stat().st_mtime < newest:
            compile_shader(source, output)


def watch_app_shaders(folder: Path, stop: threading.Event) -> None:
    """Compiles the shaders of an app in development again whenever a source changes, which the player then reloads, and reports errors without stopping, once per change."""
    shaders = folder / "content" / "shaders"
    seen = newest_shader_source(folder) if shaders.is_dir() else 0.0
    while not stop.wait(SHADER_WATCH_SECONDS):
        stamp = newest_shader_source(folder) if shaders.is_dir() else 0.0
        if stamp == seen:
            continue
        seen = stamp
        try:
            compile_app_shaders(folder)
        except BuildError as error:
            print(f"Error: {error}", file=sys.stderr, flush=True)


def command_shaders(args: argparse.Namespace) -> None:
    folder = resolve_app(args.app)
    if not shader_sources(folder):
        print(f'The app "{folder}" has no shaders under "content/shaders".')
        return
    if args.force:
        for source in shader_sources(folder):
            compile_shader(source, source.with_suffix(".shader"))
        return
    compile_app_shaders(folder)


def platform_templates() -> list[str]:
    return sorted(path.name for path in PLATFORM_TEMPLATES_DIR.iterdir() if path.is_dir())


def assemble(app: App, template: str | None, run_platform: str) -> Path:
    """Recreates the `<platform>` folder of the build folder of an app from the platform template and lays the `platform/<template>` folder of the app over it. Platforms without a template start empty and take the `platform/<platform>` folder of the app, such as the libraries a Windows app keeps next to its executable."""
    folder = app.build_folder / run_platform
    shutil.rmtree(folder, ignore_errors=True)
    if template is None:
        folder.mkdir(parents=True)
    else:
        shutil.copytree(PLATFORM_TEMPLATES_DIR / template, folder, symlinks=True)
    overrides = app.folder / "platform" / (template or run_platform)
    if overrides.is_dir():
        shutil.copytree(overrides, folder, dirs_exist_ok=True, ignore=COPY_IGNORED)
    return folder


def write_if_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file() or path.read_text() != text:
        path.write_text(text)


def apple_colorset(color: tuple[int, int, int, int]) -> dict:
    red, green, blue, alpha = color
    components = {"red": f"{red / 255:.3f}", "green": f"{green / 255:.3f}", "blue": f"{blue / 255:.3f}", "alpha": f"{alpha / 255:.3f}"}
    return {"colors": [{"color": {"color-space": "srgb", "components": components}, "idiom": "universal"}], "info": {"author": "xcode", "version": 1}}


def apple_plugin_keys(app: App, platforms: tuple[str, ...], section: str, keys: dict, label: str) -> dict:
    """Merges the `infoPlist` or `entitlements` keys of the plugins that build for any of the platforms into the keys that `make.py` writes from `app.json`."""
    merged = copy.deepcopy(keys)
    owners = {key: '"app.json"' for key in merged}
    for plugin in app.plugins:
        if plugin.supports(*platforms) and "apple" in plugin.manifest:
            merge_plugin_keys(merged, plugin_section(app, plugin, "apple").get(section, {}), owners, f'the plugin "{plugin.id}"', label)
    return merged


def write_apple_entitlements(app: App, project: Path) -> list[str]:
    """Writes the entitlements that the plugins of an app ask for into `<platform>/App.entitlements` of the Apple project and returns the `App.xcconfig` settings that sign each target and SDK platform with its file, since the iOS target signs Mac Catalyst apps too."""
    settings = []
    for platform, signed in APPLE_ENTITLEMENTS.items():
        entitlements = apple_plugin_keys(app, (platform,), "entitlements", {}, "entitlements")
        if entitlements:
            write_if_changed(project / platform / "App.entitlements", plistlib.dumps(entitlements, sort_keys=True).decode())
            settings += [f"HAYLEN_ENTITLEMENTS_{key} = {platform}/App.entitlements" for key in signed]
    if settings:
        settings.append("CODE_SIGN_ENTITLEMENTS = $(HAYLEN_ENTITLEMENTS_$(TARGET_NAME)_$(PLATFORM_NAME))")
    return settings


def apple_targets(plugin: Plugin) -> dict[str, dict]:
    """Returns the targets of the Apple project that the Apple part of a plugin joins, each with the XcodeGen keys that keep the plugin to iOS or to Mac Catalyst when it lists only one of them."""
    targets = {}
    for target, platforms in APPLE_PLUGIN_TARGETS.items():
        supported = [platform for platform in platforms if plugin.supports(platform)]
        if supported:
            targets[target] = {} if len(supported) == len(platforms) else {"destinationFilters": ["iOS" if supported == ["ios"] else "macCatalyst"]}
    return targets


def write_apple_plugins(app: App, project: Path) -> bool:
    """Copies the Apple sources and resources of the plugins of an app into `plugins/` of the Apple project and writes `plugins.json`, the XcodeGen include of `project.yml` that adds them with their Swift packages, system frameworks and build scripts to the targets of their platforms. Returns whether the include adds anything, because the project then needs generating again."""
    packages: dict[str, dict] = {}
    package_owners: dict[str, str] = {}
    bundled: dict[str, str] = {}
    targets = {target: {"sources": [], "dependencies": [], "postBuildScripts": []} for target in APPLE_PLUGIN_TARGETS}
    linked: dict[str, dict[tuple[str, ...], list[str] | None]] = {target: {} for target in APPLE_PLUGIN_TARGETS}
    for plugin in app.plugins:
        apple = plugin_section(app, plugin, "apple")
        if apple is None:
            continue

        folder = project / "plugins" / plugin.id
        sources = []
        if "sources" in apple:
            shutil.copytree(plugin.folder / apple["sources"], folder / "sources", ignore=COPY_IGNORED)
            sources.append({"path": f"plugins/{plugin.id}/sources", "name": plugin.id, "group": "plugins"})
        for resource in plugin.manifest["apple"].get("resources", []):
            source = plugin_file(app, plugin, resource)
            if source is None:
                continue
            if source.name in bundled:
                raise BuildError(f'The plugins "{bundled[source.name]}" and "{plugin.id}" both place "{source.name}" at the root of the app bundle.')
            bundled[source.name] = plugin.id
            copy_into(source, folder / "resources")
        if (folder / "resources").is_dir():
            sources.append({"path": f"plugins/{plugin.id}/resources", "name": f"{plugin.id} resources", "group": "plugins", "buildPhase": "resources"})

        for name, package in apple.get("packages", {}).items():
            declared = {"url": package["url"], "exactVersion": package["exactVersion"]}
            if packages.setdefault(name, declared) != declared:
                raise BuildError(f'The plugins "{package_owners[name]}" and "{plugin.id}" ask for the Swift package "{name}" from different URLs or versions.')
            package_owners.setdefault(name, plugin.id)

        # A product or framework that two plugins link joins the target once, for every destination either plugin builds for.
        for target, filters in apple_targets(plugin).items():
            targets[target]["sources"] += [{**source, **filters} for source in sources]
            targets[target]["postBuildScripts"] += apple.get("buildScripts", [])
            dependencies = [("package", name, product) for name, package in apple.get("packages", {}).items() for product in package["products"]]
            for dependency in [*dependencies, *(("sdk", framework) for framework in apple.get("frameworks", []))]:
                destinations = filters.get("destinationFilters")
                if dependency in linked[target] and (linked[target][dependency] is None or destinations is None or linked[target][dependency] != destinations):
                    destinations = None
                linked[target][dependency] = destinations

    for target, dependencies in linked.items():
        for dependency, destinations in dependencies.items():
            entry = {"package": dependency[1], "product": dependency[2]} if dependency[0] == "package" else {"sdk": dependency[1]}
            targets[target]["dependencies"].append({**entry, **({"destinationFilters": destinations} if destinations else {})})

    included = {target: {key: value for key, value in parts.items() if value} for target, parts in targets.items()}
    spec = {"packages": packages, "targets": {target: parts for target, parts in included.items() if parts}}
    spec = {key: value for key, value in spec.items() if value}
    write_if_changed(project / "plugins.json", json.dumps(spec, indent=4) + "\n")
    return bool(spec)


def write_apple_settings(app: App, project: Path, native: list[str]) -> None:
    """Writes `App.xcconfig` with the settings that link the static native libraries and sign with the entitlements of the plugins, the `Info.plist` of every platform with the keys and the classes of the plugins, and the splash assets of an app into the Apple project."""
    xcconfig = "\n".join([
        "// Written by `make.py` from `app.json` and the plugins of the app.",
        f"HAYLEN_PRODUCT_NAME = {app.name}",
        f"HAYLEN_BUNDLE_IDENTIFIER = {app.identifier}",
        f"MARKETING_VERSION = {app.version}",
        f"CURRENT_PROJECT_VERSION = {app.version}",
        *native,
        *write_apple_entitlements(app, project),
        "",
    ])
    write_if_changed(project / "App.xcconfig", xcconfig)

    common = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": app.name,
        "CFBundleExecutable": "$(EXECUTABLE_NAME)",
        "CFBundleIdentifier": "$(PRODUCT_BUNDLE_IDENTIFIER)",
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "$(PRODUCT_NAME)",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "$(MARKETING_VERSION)",
        "CFBundleVersion": "$(CURRENT_PROJECT_VERSION)",
        "GCSupportsControllerUserInteraction": True,
        "ITSAppUsesNonExemptEncryption": False,
    }
    # The `sokol_app` module creates its window from the scene that UIKit connects, so iOS and tvOS apps declare the scene life cycle with one scene.
    scenes = {"UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": False}}
    phone, pad = IOS_ORIENTATIONS[app.orientation]
    plists = {
        "ios": {**common, **scenes, "LSRequiresIPhoneOS": True, "UILaunchStoryboardName": "LaunchScreen", "UIStatusBarHidden": True, "UIViewControllerBasedStatusBarAppearance": False, "UISupportedInterfaceOrientations": phone, "UISupportedInterfaceOrientations~ipad": pad},
        "tvos": {**common, **scenes, "UILaunchStoryboardName": "LaunchScreen"},
        "macos": {**common, "LSMinimumSystemVersion": "$(MACOSX_DEPLOYMENT_TARGET)", "NSHighResolutionCapable": True, "NSPrincipalClass": "NSApplication", **({} if app.show_in_taskbar else {"LSUIElement": True})},
    }
    # The runtime loads the plugin classes that `HaylenPlugins` lists, in order, and skips a class that a destination leaves out, as Mac Catalyst does with iOS-only plugins.
    for target, platforms in APPLE_PLUGIN_TARGETS.items():
        values = apple_plugin_keys(app, platforms, "infoPlist", plists[target.lower()], '"Info.plist"')
        classes = [plugin.manifest["apple"]["class"] for plugin in app.plugins if plugin.supports(*platforms) and "class" in plugin.manifest.get("apple", {})]
        if classes:
            values["HaylenPlugins"] = classes
        write_if_changed(project / target.lower() / "Info.plist", plistlib.dumps(values, sort_keys=True).decode())

    # The launch screens of iOS and tvOS show the splash logo, or the vector engine logo, over the splash background.
    for platform_name in ("ios", "tvos"):
        catalog = project / platform_name / "Assets.xcassets"
        logo = catalog / "splash_logo.imageset"
        shutil.rmtree(logo, ignore_errors=True)
        logo.mkdir(parents=True)
        source = app.splash_logo or ENGINE_LOGO
        filename = f"splash_logo{source.suffix.lower()}"
        shutil.copy2(source, logo / filename)
        image = {"images": [{"filename": filename, "idiom": "universal"}], "info": {"author": "xcode", "version": 1}}
        if source.suffix.lower() == ".svg":
            image["properties"] = {"preserves-vector-representation": True}
        write_if_changed(logo / "Contents.json", json.dumps(image, indent=2) + "\n")
        write_if_changed(catalog / "splash_background.colorset" / "Contents.json", json.dumps(apple_colorset(app.background), indent=2) + "\n")


def apple_simulator(family: str, requested: str | None) -> dict:
    """Finds the simulator a run targets: the requested name or id, else a booted one of the family, else the first available one."""
    devices = json.loads(capture(["xcrun", "simctl", "list", "devices", "available", "--json"]))["devices"]
    candidates = [device for runtime, entries in devices.items() if f"SimRuntime.{family}-" in runtime for device in entries]
    if requested:
        matches = [device for device in candidates if requested in (device["udid"], device["name"])]
        if not matches:
            raise BuildError(f'No available {family} simulator is named "{requested}".')
        return matches[0]
    if not candidates:
        raise BuildError(f'No {family} simulator is installed. Install the runtime with "xcodebuild -downloadPlatform {family}".')
    booted = [device for device in candidates if device["state"] == "Booted"]
    preferred = [device for device in candidates if device["name"].startswith("iPhone")] if family == "iOS" else candidates
    return (booted or preferred or candidates)[0]


def apple_team() -> str:
    team = os.environ.get("HAYLEN_APPLE_TEAM")
    if not team:
        raise BuildError('Apps for Apple devices are signed with a development team. Set "HAYLEN_APPLE_TEAM" to its id.')
    return team


def relay_unified_log(stream: subprocess.Popen) -> None:
    """Prints the message of every event of a log stream in the `ndjson` style, with warnings and errors on `stderr` like the engine on a desktop."""
    for line in stream.stdout:
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            print(line.rstrip(), flush=True)
            continue
        target = sys.stderr if event.get("messageType") in ("Error", "Fault") else sys.stdout
        print(event.get("eventMessage", ""), file=target, flush=True)


@contextlib.contextmanager
def unified_log(executable: str, prefix: list):
    """Relays what the engine of an app process writes to the unified log, where it logs on iOS, tvOS and Mac Catalyst, while the block runs. The prefix runs `log` inside a simulator."""
    predicate = f'process == "{executable}" AND subsystem == "{APPLE_LOG_SUBSYSTEM}"'
    stream = subprocess.Popen([str(part) for part in [*prefix, "log", "stream", "--style", "ndjson", "--level", "debug", "--predicate", predicate]], stdout=subprocess.PIPE, text=True)
    # The stream names its filter on the first line and delivers events from then on, so the app starts after that line.
    stream.stdout.readline()
    threading.Thread(target=relay_unified_log, args=(stream,), daemon=True).start()
    try:
        yield
    finally:
        # Events reach the stream a moment after the app writes them, so the last lines of an app that ended still arrive.
        time.sleep(LOG_STREAM_GRACE_SECONDS)
        stream.terminate()
        stream.wait()


def launch_apple(bundle: Path, args: argparse.Namespace, simulator: dict | None) -> None:
    """Launches an app bundle on this Mac, a simulator or a device and streams its output until it exits. The engine writes to the standard output on macOS and to the unified log elsewhere, which simulators and Mac Catalyst stream next to the output of the process."""
    desktop = args.platform in {"macos", "catalyst"}
    info = plistlib.loads((bundle / "Contents" / "Info.plist" if desktop else bundle / "Info.plist").read_bytes())
    executable, identifier = info["CFBundleExecutable"], info["CFBundleIdentifier"]
    if args.platform == "macos":
        run([bundle / "Contents" / "MacOS" / executable])
    elif args.platform == "catalyst":
        with unified_log(executable, []):
            run([bundle / "Contents" / "MacOS" / executable])
    elif simulator:
        udid = simulator["udid"]
        if simulator["state"] != "Booted":
            run(["xcrun", "simctl", "boot", udid])
        run(["xcrun", "simctl", "bootstatus", udid, "-b"])
        run(["xcrun", "simctl", "install", udid, bundle])
        with unified_log(executable, ["xcrun", "simctl", "spawn", udid]):
            run(["xcrun", "simctl", "launch", "--console-pty", "--terminate-running-process", udid, identifier])
    else:
        if not args.device:
            raise BuildError('Name the device with "--device". Use the command "xcrun devicectl list devices" to list them.')
        run(["xcrun", "devicectl", "device", "install", "app", "--device", args.device, bundle])
        run(["xcrun", "devicectl", "device", "process", "launch", "--console", "--device", args.device, identifier])


def run_apple(app: App, project: Path, args: argparse.Namespace) -> None:
    require_host("apple")
    (project / "Haylen.xcframework").symlink_to(ARTIFACTS_DIR / "apple" / "Haylen.xcframework")
    copy_package(app, project / "app")
    write_apple_settings(app, project, prepare_apple_native(app, project, args.platform, args.jobs))
    # The committed `App.xcodeproj` matches the template, whose `plugins.json` adds nothing, so only an app whose plugins add to the project generates it again.
    if write_apple_plugins(app, project):
        run([ensure_xcodegen(), "generate", "--spec", project / "project.yml"], cwd=project)

    settings = APPLE_RUNS[args.platform]
    simulator = apple_simulator(settings["simulator"], args.device) if "simulator" in settings else None
    destination = f"id={simulator['udid']}" if simulator else settings["destination"]
    if args.platform in {"macos", "catalyst"}:
        destination += f",arch={'arm64' if host_arch() == 'arm64' else 'x86_64'}"
    derived = project / "build"
    command = ["xcodebuild", "-project", project / "App.xcodeproj", "-scheme", settings["scheme"], "-configuration", args.config, "-destination", destination, "-derivedDataPath", derived, "-jobs", str(args.jobs), "build"]
    if args.platform in {"ios", "tvos"}:
        command += ["-allowProvisioningUpdates", f"DEVELOPMENT_TEAM={apple_team()}"]
    run(command)
    launch_apple(next((derived / "Build" / "Products").glob("*/*.app")), args, simulator)


def gradle_property(value: str) -> str:
    """Escapes a value of `gradle.properties`, which Gradle reads as ISO 8859-1 text with backslash escapes, so any text survives."""
    units = value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").encode("utf-16-be")
    codes = (int.from_bytes(units[index : index + 2], "big") for index in range(0, len(units), 2))
    return "".join(chr(code) if code < 0x80 else f"\\u{code:04x}" for code in codes)


def prepare_android_plugins(app: App, project: Path) -> dict[str, str]:
    """Copies the library module of every plugin into `plugins/<id>` of the Android project together with the files the plugin places in the project, and returns the `gradle.properties` keys that include the modules, apply their Gradle plugins and set their manifest placeholders."""
    modules: list[str] = []
    gradle_plugins: dict[str, str] = {}
    placeholders: dict[str, str] = {}
    owners: dict[str, dict[str, str]] = {"gradle": {}, "placeholder": {}, "file": {}}
    for plugin in app.plugins:
        android = plugin_section(app, plugin, "android")
        if android is None:
            continue
        shutil.copytree(plugin.folder / android["module"], project / "plugins" / plugin.id, ignore=COPY_IGNORED)
        modules.append(f"{plugin.id}=plugins/{plugin.id}")

        for entry in android.get("gradlePlugins", []):
            if gradle_plugins.setdefault(entry["id"], entry["version"]) != entry["version"]:
                raise BuildError(f"The plugins \"{owners['gradle'][entry['id']]}\" and \"{plugin.id}\" apply the Gradle plugin \"{entry['id']}\" in different versions.")
            owners["gradle"].setdefault(entry["id"], plugin.id)
        for name, value in android.get("placeholders", {}).items():
            if placeholders.setdefault(name, parameter_text(value)) != parameter_text(value):
                raise BuildError(f"The plugins \"{owners['placeholder'][name]}\" and \"{plugin.id}\" give the manifest placeholder \"{name}\" different values.")
            owners["placeholder"].setdefault(name, plugin.id)

        for entry in plugin.manifest["android"].get("files", []):
            source = plugin_file(app, plugin, entry["from"])
            destination = substitute_parameters(entry["to"], app.plugin_values[plugin.id])
            if source is None or destination is None:
                continue
            if destination in owners["file"]:
                raise BuildError(f"The plugins \"{owners['file'][destination]}\" and \"{plugin.id}\" both place \"{destination}\" in the Android project.")
            owners["file"][destination] = plugin.id
            (project / destination).parent.mkdir(parents=True, exist_ok=True)
            if source.is_dir():
                shutil.copytree(source, project / destination, dirs_exist_ok=True)
            else:
                shutil.copy2(source, project / destination)

    return {
        "haylen.plugins": ",".join(modules),
        "haylen.gradlePlugins": ",".join(f"{identifier}={version}" for identifier, version in gradle_plugins.items()),
        **{f"haylen.placeholder.{name}": value for name, value in placeholders.items()},
    }


def write_android_settings(app: App, project: Path, library: str, plugins: dict[str, str]) -> None:
    """Points the Gradle project at the engine repository and writes the identity, version, orientation and splash of an app, the native library its activity loads and the keys of its plugins."""
    properties = project / "gradle.properties"
    values = {
        "haylen.repository": (ARTIFACTS_DIR / "android" / "maven").as_posix(),
        "haylen.engineVersion": engine_version(),
        "haylen.name": app.name,
        "haylen.identifier": app.identifier,
        "haylen.versionName": app.version,
        "haylen.versionCode": str(app.version_code),
        "haylen.orientation": ANDROID_ORIENTATIONS[app.orientation],
        "haylen.library": library,
        **plugins,
    }
    lines = [line for line in properties.read_text().splitlines() if not line.startswith("haylen.")]
    lines += [f"{key}={gradle_property(value)}" for key, value in values.items()]
    properties.write_text("\n".join(lines) + "\n")

    # The splash resources of the app replace the defaults of the `haylen` library, which show the engine logo.
    resources = project / "app" / "src" / "main" / "res"
    red, green, blue, alpha = app.background
    colors = f'<?xml version="1.0" encoding="utf-8"?>\n<!-- Written by `make.py` from the splash of `app.json`. -->\n<resources>\n    <color name="haylen_splash_background">#{alpha:02X}{red:02X}{green:02X}{blue:02X}</color>\n</resources>\n'
    write_if_changed(resources / "values" / "haylen_splash.xml", colors)
    if app.splash_logo:
        if app.splash_logo.suffix.lower() not in {".png", ".webp", ".jpg", ".jpeg"}:
            raise BuildError(f'Android splash logos are PNG, WebP or JPEG images, not "{app.splash_logo.name}".')
        shutil.copy2(app.splash_logo, resources / "drawable" / f"haylen_splash_logo{app.splash_logo.suffix.lower()}")


def android_device(requested: str | None) -> str:
    devices = [line.split()[0] for line in capture([adb(), "devices"]).splitlines()[1:] if line.strip().endswith("device")]
    if requested:
        if requested not in devices:
            raise BuildError(f'The device "{requested}" is not a connected Android device. Use the command "adb devices" to list them.')
        return requested
    if len(devices) != 1:
        raise BuildError('Name the Android device with "--device", because ' + ("none is connected." if not devices else f"{len(devices)} are connected: {', '.join(f'"{device}"' for device in devices)}."))
    return devices[0]


def prepare_android(app: App, project: Path, library: str, jobs: int) -> None:
    """Copies the package of an app into the assets of its Android project, adds the modules of its plugins, writes its settings with the native library the activity loads and places its native libraries."""
    files = copy_package(app, project / "app" / "src" / "main" / "assets" / "app")
    # Android cannot list asset folders recursively, so the runtime reads the files of the package from this index.
    (project / "app" / "src" / "main" / "assets" / "app" / "haylen-package-index.json").write_text(json.dumps(sorted(files)))
    write_android_settings(app, project, library, prepare_android_plugins(app, project))
    prepare_android_native(app, project, jobs)


def launch_android(app: App, project: Path, args: argparse.Namespace, device: str) -> None:
    """Builds the APK of an Android project with Gradle, installs it on a device, starts it and streams the log of its process until it ends."""
    variant = "Release" if args.config == "Release" else "Debug"
    run([ensure_gradle(), "-p", project, f":app:assemble{variant}", f"--max-workers={args.jobs}"])
    apk = project / "app" / "build" / "outputs" / "apk" / variant.lower() / f"app-{variant.lower()}.apk"
    run([adb(), "-s", device, "install", "-r", apk])
    # The launcher intent starts the task of the app, so the launcher icon brings that task back as it is later.
    run([adb(), "-s", device, "shell", "am", "start", "-W", "-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER", "-n", f"{app.identifier}/dev.haylen.HaylenActivity"])
    process = capture([adb(), "-s", device, "shell", "pidof", app.identifier]).strip()
    if process:
        run([adb(), "-s", device, "logcat", "--pid", process])


def run_android(app: App, project: Path, args: argparse.Namespace) -> None:
    prepare_android(app, project, "haylen", args.jobs)
    launch_android(app, project, args, android_device(args.device))


def write_web_plugins(app: App, site: Path) -> list[dict]:
    """Copies the `web` folder of every plugin with a web part to `plugins/<id>/` of the site and returns their entries of `config.json`, whose modules the loader imports and loads before the runtime starts, with the values of their parameters."""
    entries = []
    for plugin in app.plugins:
        if "web" not in plugin.manifest:
            continue
        shutil.copytree(plugin.folder / "web", site / "plugins" / plugin.id, ignore=COPY_IGNORED)
        module = Path(plugin.manifest["web"]["module"]).relative_to("web").as_posix()
        entries.append({"id": plugin.id, "version": plugin.version, "module": f"plugins/{plugin.id}/{module}", "config": app.plugin_values[plugin.id]})
    return entries


def write_web_settings(app: App, site: Path) -> None:
    """Writes `app.zip`, the splash logo, the web modules of the plugins and `config.json`, whose sizes let the loader show progress when the server sends no length."""
    package_folder(app.folder, site / "app.zip")
    logo = app.splash_logo or ENGINE_LOGO
    logo_name = f"splash{logo.suffix.lower()}" if app.splash_logo else ENGINE_LOGO.name
    if app.splash_logo:
        shutil.copy2(logo, site / logo_name)
    red, green, blue, alpha = app.background
    sizes = {path: (site / path).stat().st_size for path in ("app.zip", "webgpu/haylen.wasm", "webgl2/haylen.wasm")}
    config = {"name": app.name, "transparent": app.transparent, "splash": {"logo": logo_name, "background": f"rgba({red}, {green}, {blue}, {alpha / 255:.3f})"}, "sizes": sizes, "plugins": write_web_plugins(app, site)}
    (site / "config.json").write_text(json.dumps(config, indent=4) + "\n")


def run_web(app: App, site: Path, args: argparse.Namespace) -> None:
    for backend in ("webgpu", "webgl2"):
        shutil.copytree(ARTIFACTS_DIR / "web" / backend, site / backend)
    write_web_settings(app, site)
    serve(site, args.host, args.port, args.coep, args.coop, args.open)


def run_desktop_app(app: App, folder: Path, args: argparse.Namespace) -> None:
    """Runs the shipped layout of a Windows or Linux app: the player named after the app with the package in an app folder next to it, and the native libraries next to it on Windows and in `lib` on Linux, which the `RUNPATH` of the player covers."""
    require_host(args.platform)
    executable = folder / executable_name(app.slug)
    shutil.copy2(desktop_artifact(), executable)
    copy_package(app, folder / "app")
    prepare_host_native(app, folder if args.platform == "windows" else folder / "lib", args.jobs)
    run([executable], cwd=folder)


@dataclasses.dataclass(frozen=True)
class RunTarget:
    """A platform that `make.py run` builds for: the platform template it assembles, if any, the engine artifacts it needs, the plugin platform whose parameters it checks and the function that builds and launches it."""

    template: str | None
    artifacts: str
    plugins: str
    run: Callable[[App, Path, argparse.Namespace], None]


# A new platform is a folder under `templates/platform` and an entry here.
RUN_TARGETS = {
    "macos": RunTarget("apple", "apple", "macos", run_apple),
    "catalyst": RunTarget("apple", "apple", "catalyst", run_apple),
    "ios": RunTarget("apple", "apple", "ios", run_apple),
    "ios-simulator": RunTarget("apple", "apple", "ios", run_apple),
    "tvos": RunTarget("apple", "apple", "tvos", run_apple),
    "tvos-simulator": RunTarget("apple", "apple", "tvos", run_apple),
    "android": RunTarget("android", "android", "android", run_android),
    "web": RunTarget("web", "web", "web", run_web),
    "windows": RunTarget(None, "desktop", "windows", run_desktop_app),
    "linux": RunTarget(None, "desktop", "linux", run_desktop_app),
}


def command_run(args: argparse.Namespace) -> None:
    app = resolve_app(args.app)
    compile_app_shaders(app)
    if args.platform is None:
        # The player of this machine runs the package folder in development mode, which reloads edited files, while changed shaders compile again in the background.
        info = App(app, host_name())
        build = build_options(host_name(), args.config, "haylen", args.jobs)
        command_build(build)
        # The native libraries of the app wait in a folder of their own, which the player searches first.
        native = info.build_folder / "native" / "development"
        shutil.rmtree(native, ignore_errors=True)
        native_options = ["--native", native] if prepare_host_native(info, native, args.jobs) else []
        stop = threading.Event()
        threading.Thread(target=watch_app_shaders, args=(app, stop), daemon=True).start()
        try:
            run([build_dir(build.platform, build.config) / "bin" / "haylen" / executable_name("haylen"), "--dev", *native_options, app])
        finally:
            stop.set()
        return

    target = RUN_TARGETS[args.platform]
    info = App(app, target.plugins)
    ensure_artifacts(target.artifacts, args.engine_config, args.jobs)
    target.run(info, assemble(info, target.template, args.platform), args)


def command_new(args: argparse.Namespace) -> None:
    """Creates an app from the starter app and a copy of every platform template, which the developer owns from then on."""
    folder = Path(args.folder).expanduser().resolve()
    if folder.exists() and any(folder.iterdir()):
        raise BuildError(f'The folder "{folder}" already exists and is not empty.')

    slug = re.sub(r"[^a-z0-9]+", "-", folder.name.lower()).strip("-")
    if not slug:
        raise BuildError(f'The folder name "{folder.name}" gives no app name. Use a folder name with letters or digits.')
    name = args.name or " ".join(word.capitalize() for word in slug.split("-"))
    identifier = args.identifier or f"com.example.{slug.replace('-', '')}"
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+", identifier):
        raise BuildError(f'The identifier "{identifier}" is not a reverse domain identifier such as "com.example.game".')

    shutil.copytree(APP_TEMPLATE, folder, dirs_exist_ok=True)
    document = json.loads((folder / "app.json").read_text())
    document.update({"name": name, "identifier": identifier, "orientation": args.orientation})
    document["window"]["title"] = name
    # The starter app is laid out for landscape, so a portrait app turns its window and design resolution upright.
    if args.orientation == "portrait":
        for section in (document["window"], document["design"]):
            section["width"], section["height"] = section["height"], section["width"]
    (folder / "app.json").write_text(json.dumps(document, indent=4) + "\n")
    for template in platform_templates():
        shutil.copytree(PLATFORM_TEMPLATES_DIR / template, folder / "platform" / template, ignore=shutil.ignore_patterns(".DS_Store"))
    print(f'Created "{name}" ("{identifier}") in "{folder}". Run it with "python3 make.py run {folder}".')


def write_app_json(folder: Path, document: dict) -> None:
    (folder / "app.json").write_text(json.dumps(document, indent=4, ensure_ascii=False) + "\n")


def is_git_repository(value: str) -> bool:
    """Tells a git repository address, such as `https://github.com/haylen-org/<plugin>.git` or `git@github.com:haylen-org/<plugin>.git`, from a folder path."""
    return "://" in value or value.startswith("git@") or value.endswith(".git")


def command_plugin_add(args: argparse.Namespace) -> None:
    """Copies a plugin folder, or the root of a git repository at a branch, tag or commit, into `plugins/` of an app, replacing an earlier copy, and lists it in `app.json` with the defaults of its parameters and an empty text for every required one."""
    app = resolve_app(args.app)
    with tempfile.TemporaryDirectory() as scratch:
        if is_git_repository(args.plugin):
            # Fetching one ref by name works for branches, tags and commits alike, and the checkout takes the name of the plugin id like any plugin folder.
            checkout = Path(scratch) / "checkout"
            run(["git", "init", "--quiet", checkout])
            run(["git", "-C", checkout, "fetch", "--quiet", "--depth", "1", args.plugin, args.ref or "HEAD"])
            run(["git", "-C", checkout, "checkout", "--quiet", "FETCH_HEAD"])
            if not (checkout / "plugin.json").is_file():
                raise BuildError(f'The address "{args.plugin}" is no plugin repository, because it has no "plugin.json" at its root.')
            identifier = json.loads((checkout / "plugin.json").read_text()).get("id")
            source = checkout.rename(Path(scratch) / identifier) if isinstance(identifier, str) and PLUGIN_ID.fullmatch(identifier) else checkout
        elif args.ref:
            raise BuildError('The option "--ref" picks a branch, tag or commit of a git repository, and a plugin folder has none.')
        else:
            source = Path(args.plugin).expanduser().resolve()
        if not (source / "plugin.json").is_file():
            raise BuildError(f'The folder "{args.plugin}" is no plugin, because it has no "plugin.json" at its root.')
        plugin = Plugin.load(source)
        target = app / "plugins" / plugin.id
        if source != target:
            shutil.rmtree(target, ignore_errors=True)
            shutil.copytree(source, target, ignore=COPY_IGNORED)

    document = json.loads((app / "app.json").read_text())
    listed = document.setdefault("plugins", {})
    values = listed.setdefault(plugin.id, {})
    for name, parameter in plugin.parameters.items():
        if name not in values and ("default" in parameter or parameter.get("required")):
            values[name] = parameter.get("default", "")
    write_app_json(app, document)

    print(f'Added "{plugin.id}" {plugin.version} to "{app}".')
    missing = [name for name, value in values.items() if value == "" and plugin.parameters.get(name, {}).get("required")]
    if missing:
        print(f"Fill in {', '.join(f'"{name}"' for name in missing)} under \"plugins.{plugin.id}\" of \"{app / 'app.json'}\".")
    for required in plugin.requires:
        if required not in listed:
            print(f'The plugin "{plugin.id}" requires "{required}", which the app does not list yet. Add it with "python3 make.py plugin add" and its folder or repository.')


def command_plugin_remove(args: argparse.Namespace) -> None:
    """Deletes a plugin from `plugins/` of an app and from its `app.json`."""
    app = resolve_app(args.app)
    folder = app / "plugins" / args.id
    document = json.loads((app / "app.json").read_text())
    listed = document.get("plugins", {})
    if args.id not in listed and not folder.exists():
        raise BuildError(f'The app "{app}" has no plugin "{args.id}".')

    shutil.rmtree(folder, ignore_errors=True)
    if folder.parent.is_dir() and not any(folder.parent.iterdir()):
        folder.parent.rmdir()
    if args.id in listed:
        del listed[args.id]
        if not listed:
            del document["plugins"]
        write_app_json(app, document)
    print(f'Removed "{args.id}" from "{app}".')


def plugin_status(app: Path, identifier: str, listed: dict) -> list[str]:
    """Describes a plugin of an app in one line, followed by the problems that keep it from building for any of its platforms."""
    folder = app / "plugins" / identifier
    if not (folder / "plugin.json").is_file():
        return [f'{identifier:<24} Missing, because "app.json" lists it and "plugins/{identifier}/plugin.json" does not exist.']
    try:
        plugin = Plugin.load(folder)
    except BuildError as error:
        return [f"{identifier:<24} invalid", *(f"    {line}" for line in str(error).splitlines())]

    problems: list[str] = []
    status = 'Not in "app.json", so no build carries it.'
    if identifier in listed:
        problems += [f'The plugin "{identifier}" requires the plugin "{required}", which "app.json" does not list.' for required in plugin.requires if required not in listed]
        for platform in plugin.manifest["platforms"]:
            problems += [problem for problem in parameter_values(app, plugin, listed[identifier], platform)[1] if problem not in problems]
        status = f"{len(problems)} problems" if problems else "ok"

    return [f"{identifier:<24} {plugin.version:<10} {', '.join(plugin.manifest['platforms']):<48} {status}", *(f"    {problem}" for problem in problems)]


def command_plugin_list(args: argparse.Namespace) -> None:
    """Lists the plugins of an app with the problems that keep each one from building."""
    app = resolve_app(args.app)
    listed = json.loads((app / "app.json").read_text()).get("plugins", {})
    folders = sorted(path.name for path in (app / "plugins").iterdir() if path.is_dir()) if (app / "plugins").is_dir() else []
    identifiers = [*listed, *(name for name in folders if name not in listed)]
    if not identifiers:
        print(f'The app "{app}" has no plugins.')
    for identifier in identifiers:
        print("\n".join(plugin_status(app, identifier, listed)))


def command_plugin_new(args: argparse.Namespace) -> None:
    """Creates a plugin from `templates/plugin`, with its id in the names of its files, classes and modules."""
    folder = Path(args.folder).expanduser().resolve()
    identifier = args.id or folder.name
    if not PLUGIN_ID.fullmatch(identifier):
        raise BuildError(f'The name "{identifier}" is no plugin id. Plugin ids are "dash-case", such as "firebase-analytics".')
    if identifier != folder.name:
        raise BuildError(f'A plugin folder is named after the id of the plugin, so the folder of "{identifier}" is named "{identifier}", not "{folder.name}".')
    if folder.exists() and any(folder.iterdir()):
        raise BuildError(f'The folder "{folder}" already exists and is not empty.')

    words = identifier.split("-")
    tokens = {"{{ID}}": identifier, "{{NAME}}": "".join(word.capitalize() for word in words), "{{TITLE}}": " ".join(word.capitalize() for word in words), "{{PACKAGE}}": "".join(words)}

    def substitute(text: str) -> str:
        for token, value in tokens.items():
            text = text.replace(token, value)
        return text

    for source in sorted(path for path in PLUGIN_TEMPLATE.rglob("*") if path.is_file() and path.name != ".DS_Store"):
        target = folder / substitute(source.relative_to(PLUGIN_TEMPLATE).as_posix())
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(substitute(source.read_text()))
    print(f'Created the plugin "{identifier}" in "{folder}". Add it to an app with "python3 make.py plugin add {folder} --app <app>".')


# The platforms a C++ app project runs on: this machine, the browser, Mac Catalyst, iOS, tvOS, their simulators and Android.
CPP_RUN_PLATFORMS = [host_name(), "web", *(name for name in APPLE_NATIVE_SLICES if name != "macos"), "android"]


def cpp_target(project: Path, requested: str | None) -> str:
    return requested or project.name


def command_run_cpp(args: argparse.Namespace) -> None:
    """Builds a C++ app project, which compiles the engine through its own CMake, and runs it on this machine, in the browser, on an Apple simulator or device, on Mac Catalyst or on Android."""
    candidate = Path(args.project).expanduser()
    project = (candidate if candidate.exists() else SAMPLES_DIR / args.project).resolve()
    if not (project / "CMakeLists.txt").is_file():
        raise BuildError(f'The path "{args.project}" is neither a CMake project folder nor the name of a C++ sample.')
    target = cpp_target(project, args.target)
    folder = CPP_BUILDS_DIR / build_folder_name(project)

    if args.platform == "web":
        bundle_web(project, target, args.config, folder, args.jobs)
        serve(folder / "web", args.host, args.port, args.coep, args.coop, args.open)
    elif args.platform == "android":
        run_cpp_android(project, target, folder, args)
    elif args.platform in APPLE_NATIVE_SLICES and args.platform != "macos":
        run_cpp_apple(project, target, folder, args)
    else:
        directory = folder / f"{host_name()}-{args.config.lower()}"
        command = ["cmake", "-S", project, "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}"]
        if host_name() != "windows" or shutil.which("ninja"):
            command += ["-G", "Ninja"]
        run(command)
        run(["cmake", "--build", directory, "--config", args.config, "--target", target, "--parallel", str(args.jobs)])
        run([cmake_app_executable(directory, target)])


def run_cpp_apple(project: Path, target: str, folder: Path, args: argparse.Namespace) -> None:
    """Builds a C++ app for iOS, tvOS or their simulators with the Xcode generator, which compiles the launch screen and signs the bundle, or for Mac Catalyst with Ninja and the Mac Catalyst toolchain, for the architecture of this Mac, then launches it like `make.py run`."""
    require_host("apple")
    device = args.platform in {"ios", "tvos"}
    arch = "arm64" if device or host_arch() == "arm64" else "x86_64"
    directory = folder / f"{args.platform}-{args.config.lower()}"
    generator = "Ninja" if args.platform == "catalyst" else "Xcode"
    command = ["cmake", "-S", project, "-B", directory, "-G", generator, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}", *apple_slice_options(APPLE_NATIVE_SLICES[args.platform], arch)]
    if device:
        command.append(f"-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM={apple_team()}")
    run(command)
    run(["cmake", "--build", directory, "--config", args.config, "--target", target, "--parallel", str(args.jobs), *(["--", "-allowProvisioningUpdates"] if device else [])])

    # Ninja places the bundle in `bin/<target>`, while the Xcode generator places it in `bin`.
    settings = APPLE_RUNS[args.platform]
    simulator = apple_simulator(settings["simulator"], args.device) if "simulator" in settings else None
    launch_apple(next((directory / "bin").glob(f"**/{target}.app")), args, simulator)


def run_cpp_android(project: Path, target: str, folder: Path, args: argparse.Namespace) -> None:
    """Builds the library of a C++ app for the ABI of the Android device and packages it with the package of the app into the Android template, whose activity loads it instead of the Lua player, then installs and launches it like `make.py run`."""
    device = android_device(args.device)
    abi = capture([adb(), "-s", device, "shell", "getprop", "ro.product.cpu.abi"]).strip()
    if abi not in ANDROID_ABIS:
        raise BuildError(f"The device \"{device}\" runs \"{abi}\", while the engine builds for {', '.join(f'"{name}"' for name in ANDROID_ABIS)}.")
    directory = folder / f"android-{abi}-{args.config.lower()}"
    run(["cmake", "-S", project, "-B", directory, "-G", "Ninja", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}", *android_options(abi)])
    run(["cmake", "--build", directory, "--target", target, "--parallel", str(args.jobs)])

    # The activity, the bridge and the other Java classes come from the `haylen` library of the artifacts, while the app brings the engine in its own library.
    built = directory / "bin" / target
    app = App(Path((built / "package.txt").read_text().strip()), "android")
    ensure_artifacts("android", args.engine_config, args.jobs)
    gradle = assemble(app, "android", "android")
    prepare_android(app, gradle, target, args.jobs)
    copy_into(built / f"lib{target}.so", gradle / "app" / "src" / "main" / "jniLibs" / abi)
    launch_android(app, gradle, args, device)


def bundle_web(source: Path, target: str, config: str, folder: Path, jobs: int) -> None:
    """Builds a CMake web target for WebGPU and WebGL2 in a build folder and bundles both into its web folder, whose page runs the backend the browser supports."""
    output = folder / "web"
    shutil.rmtree(output, ignore_errors=True)
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        directory = folder / f"{platform_name}-{config.lower()}"
        if not (directory / "CMakeCache.txt").exists():
            renderer = "WGPU" if platform_name == "web" else "GLES3"
            run([ensure_emsdk(), "cmake", "-S", source, "-B", directory, "-G", "Ninja", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", f"-DHAYLEN_RENDER_BACKEND={renderer}"])
        run(["cmake", "--build", directory, "--target", target, "--parallel", str(jobs)])
        built = directory / "bin" / target
        destination = output / backend
        destination.mkdir(parents=True)
        for suffix in (".js", ".wasm", ".data"):
            if (built / f"{target}{suffix}").is_file():
                shutil.copy2(built / f"{target}{suffix}", destination)
        shutil.copy2(built / WEB_AUDIO_WORKLET, destination)

    # The page is the shell of the target, with the script that picks the backend where Emscripten would put its own script, next to the engine logo that the default shell shows as its icon.
    shell = (built / f"{target}.shell.html").read_text()
    picker = (ENGINE_DIR / "platform" / "web" / "backend-picker.html").read_text().replace("{{TARGET}}", target)
    if "{{{ SCRIPT }}}" not in shell:
        raise BuildError(f'The web shell of "{target}" has no "{{{{{{ SCRIPT }}}}}}" placeholder.')
    (output / "index.html").write_text(shell.replace("{{{ SCRIPT }}}", picker.strip()))
    shutil.copy2(built / ENGINE_LOGO.name, output)
    print(f'Bundled "{target}" for WebGPU and WebGL2 into "{output}".')


def list_samples() -> list[tuple[str, str]]:
    """Returns the path from `samples/` and the kind of every sample: a C++ project has a `CMakeLists.txt`, a Lua app only an `app.json`."""
    found = []
    for folder in sorted(path for path in SAMPLES_DIR.glob("*/*") if path.is_dir()):
        if (folder / "CMakeLists.txt").is_file():
            found.append((folder.relative_to(SAMPLES_DIR).as_posix(), "cpp"))
        elif (folder / "app.json").is_file():
            found.append((folder.relative_to(SAMPLES_DIR).as_posix(), "lua"))
    return found


def command_samples(_: argparse.Namespace) -> None:
    """Lists the samples by category with the command that runs each one."""
    category = ""
    for path, kind in list_samples():
        current = path.split("/")[0]
        if current != category:
            category = current
            print(f"{category}/")
        runner = "run-cpp" if kind == "cpp" else "run"
        print(f"  {path:<32} python3 make.py {runner} {path}")


def command_package(args: argparse.Namespace) -> None:
    app = resolve_app(args.app)
    compile_app_shaders(app)
    package_folder(app, Path(args.output).resolve())


class WebHandler(http.server.SimpleHTTPRequestHandler):
    """Serves a folder with the headers WebAssembly pages need: explicit MIME types, no caching and precompressed files. The web runtime is single-threaded, so the page stays in one browsing context group with the popups it opens, such as sign-in and payment pages, unless a page asks for cross-origin isolation with the opener policy `same-origin`."""

    coep = "require-corp"
    coop = "off"

    def end_headers(self) -> None:
        if self.coop != "off":
            self.send_header("Cross-Origin-Opener-Policy", self.coop)
        if self.coep != "off":
            self.send_header("Cross-Origin-Embedder-Policy", self.coep)
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def guess_type(self, path: str) -> str:
        return MIME_TYPES.get(Path(path).suffix.lower()) or super().guess_type(path)

    def send_head(self):
        # A request for a file that also exists as `.br` or `.gz` is answered with the compressed copy the browser accepts.
        path = Path(self.translate_path(self.path))
        accepted = self.headers.get("Accept-Encoding", "")
        for suffix, encoding in ((".br", "br"), (".gz", "gzip")):
            compressed = path.with_name(path.name + suffix)
            if encoding in accepted and path.suffix and compressed.is_file():
                data = compressed.open("rb")
                self.send_response(200)
                self.send_header("Content-Type", self.guess_type(str(path)))
                self.send_header("Content-Encoding", encoding)
                self.send_header("Content-Length", str(compressed.stat().st_size))
                self.send_header("Vary", "Accept-Encoding")
                self.end_headers()
                return data
        return super().send_head()


def serve(directory: Path, host: str, port: int, coep: str = "require-corp", coop: str = "off", open_page: bool = False) -> None:
    handler = functools.partial(type("Handler", (WebHandler,), {"coep": coep, "coop": coop}), directory=str(directory))
    with http.server.ThreadingHTTPServer((host, port), handler) as server:
        url = f"http://{host}:{port}/"
        print(f'Serving "{directory}" at "{url}".', flush=True)
        if open_page:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


def command_serve(args: argparse.Namespace) -> None:
    serve(Path(args.directory).resolve(), args.host, args.port, args.coep, args.coop, args.open)


def command_clean(_: argparse.Namespace) -> None:
    shutil.rmtree(BUILD_ROOT, ignore_errors=True)
    print(f'Removed "{BUILD_ROOT}".')


def add_build_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--platform", default=host_name(), choices=PLATFORMS)
    parser.add_argument("--config", default="Debug", choices=CONFIGS)
    parser.add_argument("--backend", choices=["METAL", "D3D11", "GLCORE", "GLES3", "WGPU"])
    parser.add_argument("--xcode", action="store_true", help="Use the Xcode generator on macOS.")
    add_sanitizer_option(parser)
    parser.add_argument("--target")
    parser.add_argument("--jobs", type=int, default=default_jobs())


def add_sanitizer_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--sanitizers", choices=["address", "thread"], help="Build in a tree of its own with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer.")


def add_web_server_options(parser: argparse.ArgumentParser, port: int) -> None:
    parser.add_argument("--host", default="127.0.0.1", help='Address the local web server listens on. The LAN address of this machine lets phones and other computers open the page, which then runs without sound, because browsers offer AudioWorklet only to pages served over https or from "localhost".')
    parser.add_argument("--port", type=int, default=port, help="Port of the local web server.")
    parser.add_argument("--coep", default="require-corp", choices=["require-corp", "credentialless", "off"], help='The "Cross-Origin-Embedder-Policy" header. Pages that load third-party scripts, such as Google sign-in, need "credentialless" or "off".')
    parser.add_argument("--coop", default="off", choices=["off", "same-origin-allow-popups", "same-origin"], help='The "Cross-Origin-Opener-Policy" header, none by default, since the single-threaded runtime needs no cross-origin isolation and "same-origin" cuts the page off from the sign-in and payment popups of plugins. The value "same-origin" with a "--coep" policy isolates the page for "SharedArrayBuffer" and threads.')
    parser.add_argument("--open", action="store_true", help="Open the page in the default browser.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Haylen build entry point.")
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    tools = commands.add_parser("tools", help="Download the pinned build tools.")
    tools.add_argument("--emsdk", action="store_true", help=f'Install Emscripten {EMSDK_VERSION} into ".tools".')
    tools.add_argument("--gradle", action="store_true", help=f'Install Gradle {GRADLE_VERSION} into ".tools".')
    tools.set_defaults(handler=command_tools)

    for name, handler, help_text in (("configure", command_configure, "Generate a build tree of the engine."), ("build", command_build, "Configure the engine when needed and build it."), ("test", command_test, "Build and run the engine tests on this machine.")):
        sub = commands.add_parser(name, help=help_text)
        add_build_options(sub)
        sub.set_defaults(handler=handler)

    engine = commands.add_parser("engine", help='Build the prebuilt engine artifacts that apps use, into "build/artifacts".')
    engine.add_argument("--platform", default="all", choices=[*ARTIFACT_PLATFORMS, "all"])
    engine.add_argument("--config", default="Release", choices=CONFIGS)
    engine.add_argument("--jobs", type=int, default=default_jobs())
    engine.set_defaults(handler=command_engine)

    new = commands.add_parser("new", help="Create an app with the starter code and a copy of every platform template.")
    new.add_argument("folder", help="Folder of the new app, which must not exist or be empty.")
    new.add_argument("--name", help="Display name, from the folder name by default.")
    new.add_argument("--identifier", help='Reverse domain identifier, "com.example.<folder>" by default.')
    new.add_argument("--orientation", default="landscape", choices=["landscape", "portrait", "any"])
    new.set_defaults(handler=command_new)

    plugin = commands.add_parser("plugin", help="Add plugins to an app, remove them, list them or create a plugin.")
    actions = plugin.add_subparsers(dest="action", required=True, metavar="action")
    add_plugin = actions.add_parser("add", help='Copy a plugin folder or a plugin repository into "plugins/" of an app and list it in "app.json".')
    add_plugin.add_argument("plugin", help='A plugin folder, or the git repository of a plugin, such as "https://github.com/haylen-org/<plugin>.git".')
    add_plugin.add_argument("--ref", help="Branch, tag or commit of the repository, its default branch otherwise.")
    add_plugin.add_argument("--app", default=".", help='App folder or sample path from "samples/", the current folder by default.')
    add_plugin.set_defaults(handler=command_plugin_add)
    remove_plugin = actions.add_parser("remove", help='Delete a plugin from "plugins/" of an app and from its "app.json".')
    remove_plugin.add_argument("id", help="Id of the plugin.")
    remove_plugin.add_argument("--app", default=".", help='App folder or sample path from "samples/", the current folder by default.')
    remove_plugin.set_defaults(handler=command_plugin_remove)
    list_plugins = actions.add_parser("list", help="List the plugins of an app with their status.")
    list_plugins.add_argument("--app", default=".", help='App folder or sample path from "samples/", the current folder by default.')
    list_plugins.set_defaults(handler=command_plugin_list)
    new_plugin = actions.add_parser("new", help='Create a plugin from "templates/plugin".')
    new_plugin.add_argument("folder", help="Folder of the new plugin, named after its id, which must not exist or be empty.")
    new_plugin.add_argument("--id", help='Id of the plugin in "dash-case", the folder name by default.')
    new_plugin.set_defaults(handler=command_plugin_new)

    commands.add_parser("samples", help="List the samples by category, with the command that runs each one.").set_defaults(handler=command_samples)

    run_app = commands.add_parser("run", help="Run an app: in the player of this machine with hot reload, or built from the templates for a platform.")
    run_app.add_argument("app", nargs="?", default=DEFAULT_APP, help=f'App folder or sample path from "samples/", "{DEFAULT_APP}" by default.')
    run_app.add_argument("--platform", choices=list(RUN_TARGETS), help="Build the app for a platform from its template. Without it the desktop player runs the app folder in development mode.")
    run_app.add_argument("--device", help="Simulator name or id, Apple device id or Android serial.")
    run_app.add_argument("--config", default="Debug", choices=["Debug", "Release"], help="Configuration of the player or of the platform project.")
    run_app.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the platform project uses.")
    run_app.add_argument("--jobs", type=int, default=default_jobs())
    add_web_server_options(run_app, 8000)
    run_app.set_defaults(handler=command_run)

    run_cpp = commands.add_parser("run-cpp", help="Build and run a C++ app project, which compiles the engine through CMake.")
    run_cpp.add_argument("project", help='CMake project folder or C++ sample path from "samples/", such as "cpp/embedding".')
    run_cpp.add_argument("--platform", default=host_name(), choices=CPP_RUN_PLATFORMS)
    run_cpp.add_argument("--target", help='The "haylen_add_app" target, named like the project folder by default.')
    run_cpp.add_argument("--device", help="Simulator name or id, Apple device id or Android serial.")
    run_cpp.add_argument("--config", default="Debug", choices=CONFIGS)
    run_cpp.add_argument("--engine-config", default="Release", choices=CONFIGS, help='Configuration of the "haylen" Android library whose Java classes Android apps use.')
    run_cpp.add_argument("--jobs", type=int, default=default_jobs())
    add_web_server_options(run_cpp, 8000)
    run_cpp.set_defaults(handler=command_run_cpp)

    package = commands.add_parser("package", help='Zip the "app.json", "source" and "content" of an app.')
    package.add_argument("app", help='App folder or sample path from "samples/".')
    package.add_argument("-o", "--output", default="app.zip", help='Zip file to write, "app.zip" by default.')
    package.set_defaults(handler=command_package)

    shaders = commands.add_parser("shaders", help='Compile the shaders under "content/shaders" of an app into ".shader" files for every backend.')
    shaders.add_argument("app", help='App folder or sample path from "samples/".')
    shaders.add_argument("--force", action="store_true", help="Compile every shader, including the ones that are up to date.")
    shaders.set_defaults(handler=command_shaders)

    serve_folder = commands.add_parser("serve", help="Serve a folder with the headers WebAssembly pages need.")
    serve_folder.add_argument("directory")
    add_web_server_options(serve_folder, 8000)
    serve_folder.set_defaults(handler=command_serve)

    coverage = commands.add_parser("coverage", help="Measure engine code coverage with LLVM source-based coverage.")
    coverage.add_argument("--jobs", type=int, default=default_jobs())
    add_sanitizer_option(coverage)
    coverage.set_defaults(handler=command_coverage)

    formatter = commands.add_parser("format", help="Format the C, C++ and Objective-C sources with clang-format.")
    formatter.add_argument("--check", action="store_true", help="Fail instead of rewriting files.")
    formatter.set_defaults(handler=command_format)

    bench = commands.add_parser("bench", help="Build and run a benchmark in Release on this machine.")
    bench.add_argument("--suite", default="sprites", choices=["sprites", "algorithms", "procedural", "lua"], help="The sprite benchmark on the GPU, the algorithm or procedural benchmark on the CPU or the Lua bunnymark on the CPU.")
    bench.add_argument("--jobs", type=int, default=default_jobs())
    bench.set_defaults(handler=command_bench)

    sdk = commands.add_parser("sdk", help='Build the engine SDK and install it for "find_package(haylen)".')
    sdk.add_argument("--platform", default=host_name(), choices=sorted(DESKTOP_PLATFORMS | WEB_PLATFORMS))
    sdk.add_argument("--config", default="Release", choices=CONFIGS)
    sdk.add_argument("--output", help='Install prefix, "build/sdk/haylen-<platform>-<config>" by default.')
    sdk.add_argument("--jobs", type=int, default=default_jobs())
    sdk.set_defaults(handler=command_sdk)

    embedding = commands.add_parser("embedding", help='Build the C++ embedding sample through "add_subdirectory", CPM or an installed SDK.')
    embedding.add_argument("--mode", default="subdirectory", choices=["subdirectory", "cpm", "package"])
    embedding.add_argument("--config", default="Debug", choices=CONFIGS)
    embedding.add_argument("--jobs", type=int, default=default_jobs())
    embedding.set_defaults(handler=command_embedding)

    assets = commands.add_parser("assets", help="Import the Tiny Swords pack into the Tiny Island sample.")
    assets.add_argument("archive", help='Path to "Tiny Swords (Free Pack).zip".')
    assets.set_defaults(handler=command_assets)

    commands.add_parser("map", help="Generate the Tiny Island Tiled map.").set_defaults(handler=command_map)
    commands.add_parser("clean", help="Remove all build trees.").set_defaults(handler=command_clean)

    args = parser.parse_args()
    try:
        args.handler(args)
    except (BuildError, subprocess.CalledProcessError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == "__main__":
    main()
