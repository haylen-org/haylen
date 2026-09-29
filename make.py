#!/usr/bin/env python3
"""Single entry point to build, run, test and package Haylen and its apps on every platform."""

from __future__ import annotations

import argparse
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
# Every folder here is the project template of one platform, which make.py new copies into platform/<name> of an app.
PLATFORM_TEMPLATES_DIR = TEMPLATES_DIR / "platform"
ARTIFACTS_DIR = BUILD_ROOT / "artifacts"
ENGINE_BUILDS_DIR = BUILD_ROOT / "engine"
APPS_DIR = BUILD_ROOT / "apps"
CPP_BUILDS_DIR = BUILD_ROOT / "cpp"
ANDROID_LIBRARY_PROJECT = ENGINE_DIR / "platform" / "android"
ENGINE_LOGO = PLATFORM_TEMPLATES_DIR / "web" / "haylen-logo.svg"
DEFAULT_APP = "games/tiny-island"

SHDC_COMMIT = "11d0cf678105d614d675e6d9bd2aaf3eeff12f8c"
# App shaders include the shader library as haylen/material.glsl, which sokol-shdc finds from its working directory.
SHADER_LIBRARY_DIR = ENGINE_DIR / "shaders" / "include"
SHADER_SLANGS = "glsl430:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl"
# Every material compiles once per kind of draw and once more for lit canvases, and the engine picks the program of a draw by these names.
SHADER_PROGRAMS = {"sprite": [], "sprite_lit": ["HAYLEN_LIT"], "text": ["HAYLEN_TEXT"], "text_lit": ["HAYLEN_TEXT", "HAYLEN_LIT"], "mesh": ["HAYLEN_MESH"], "mesh_lit": ["HAYLEN_MESH", "HAYLEN_LIT"]}
# The uniform blocks and textures of the shader library, which the engine fills itself.
SHADER_ENGINE_BLOCKS = {"haylen_vs_params", "haylen_lit_params"}
SHADER_ENGINE_TEXTURES = {"sprite_texture"}
SHADER_WATCH_SECONDS = 0.5
GRADLE_VERSION = "9.8.0"
ANDROID_NDK_VERSION = "30.0.16248370"
EMSDK_VERSION = "6.0.10"
# miniaudio plays through AAudio from Android 8.1 on, and the engine builds it without OpenSL ES, like the minSdk of the Android library and template.
ANDROID_MIN_SDK = 27
# The oldest Apple systems the engine runs on: std::format with floating point, which the engine formats text and logs with, reaches their C++ library in iOS and tvOS 16.3 and macOS 13.3.
# Mac Catalyst takes its minimum, the iOS version, from engine/cmake/haylen-catalyst.toolchain.cmake.
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
# A package is app.json with the Lua modules under source and the assets under content, and nothing else in its folder ships.
PACKAGE_FOLDERS = ("source", "content")
# Engine files that never reach an artifact, so editing them keeps the artifacts fresh.
ENGINE_HASH_SKIPPED = {"tests", "bench", "build", ".cxx", ".gradle", ".kotlin", ".DS_Store"}
# The benchmarks of make.py bench that are plain executables on the CPU, by suite.
CPU_BENCHMARKS = {"algorithms": "haylen-algorithm-benchmark", "procedural": "haylen-procedural-benchmark"}

# The slices of Haylen.xcframework and the architectures each one joins with lipo.
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
        raise BuildError(f"Unsupported host system: {system}")
    return names[system]


def host_arch() -> str:
    machine = host_platform.machine().lower()
    if machine in {"arm64", "aarch64"}:
        return "arm64"
    if machine in {"x86_64", "amd64"}:
        return "x64"
    raise BuildError(f"Unsupported host architecture: {machine}")


def executable_name(name: str) -> str:
    return f"{name}.exe" if host_name() == "windows" else name


def engine_version() -> str:
    return (ENGINE_DIR / "VERSION").read_text().strip()


def download(url: str, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading {url}", flush=True)
    urllib.request.urlretrieve(url, target)


def ensure_shdc() -> Path:
    executable = executable_name("sokol-shdc")
    target = TOOLS_DIR / executable
    if target.exists():
        return target

    folders = {("macos", "arm64"): "osx_arm64", ("macos", "x64"): "osx", ("linux", "arm64"): "linux_arm64", ("linux", "x64"): "linux", ("windows", "x64"): "win32"}
    folder = folders.get((host_name(), host_arch()))
    if folder is None:
        raise BuildError("sokol-shdc has no prebuilt binary for this host.")

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
    raise BuildError("The Android SDK was not found. Set ANDROID_HOME to its folder.")


def android_ndk() -> Path:
    for variable in ("ANDROID_NDK_HOME", "ANDROID_NDK_ROOT"):
        value = os.environ.get(variable)
        if value and Path(value).exists():
            return Path(value)

    candidate = android_sdk() / "ndk" / ANDROID_NDK_VERSION
    if candidate.exists():
        return candidate
    raise BuildError(f"Android NDK {ANDROID_NDK_VERSION} was not found. Install it with the SDK manager or set ANDROID_NDK_HOME.")


def adb() -> Path:
    return android_sdk() / "platform-tools" / executable_name("adb")


def require_host(platform_name: str) -> None:
    apple = {"macos", "ios", "tvos", "apple", "ios-simulator", "tvos-simulator", "catalyst"}
    required = "macos" if platform_name in apple else {"linux": "linux", "windows": "windows"}.get(platform_name)
    if required is not None and host_name() != required:
        raise BuildError(f"{platform_name} builds require a {required} host.")


def build_dir(platform_name: str, config: str) -> Path:
    return BUILD_ROOT / f"{platform_name}-{config.lower()}"


def requested_backend(args: argparse.Namespace) -> str:
    """Returns the HAYLEN_RENDER_BACKEND a build asks for. Each web platform always uses its own backend."""
    if args.platform in WEB_PLATFORMS:
        return "WGPU" if args.platform == "web" else "GLES3"
    return args.backend or "AUTO"


def requested_sanitizers(args: argparse.Namespace) -> str:
    return "ON" if getattr(args, "sanitize", False) else "OFF"


def build_options(platform_name: str, config: str, target: str | None = None, jobs: int | None = None) -> argparse.Namespace:
    return argparse.Namespace(platform=platform_name, config=config, backend=None, xcode=False, sanitize=False, coverage=False, target=target, jobs=jobs or default_jobs())


def configure_command(args: argparse.Namespace) -> tuple[list, dict[str, str]]:
    require_host(args.platform)
    directory = build_dir(args.platform, args.config)
    command = ["cmake", "-S", ROOT, "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}"]
    command += [f"-DHAYLEN_RENDER_BACKEND={requested_backend(args)}", f"-DHAYLEN_ENABLE_SANITIZERS={requested_sanitizers(args)}"]
    env = os.environ.copy()

    if args.platform == "macos":
        command += ["-G", "Xcode"] if args.xcode else ["-G", "Ninja"]
        command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}")
    elif args.platform in {"ios", "tvos"}:
        system = "iOS" if args.platform == "ios" else "tvOS"
        command += ["-G", "Xcode", f"-DCMAKE_SYSTEM_NAME={system}", "-DCMAKE_OSX_ARCHITECTURES=arm64", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS[system]}", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_PLAYER=OFF"]
    elif args.platform == "android":
        toolchain = android_ndk() / "build" / "cmake" / "android.toolchain.cmake"
        command += ["-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", "-DANDROID_ABI=arm64-v8a", f"-DANDROID_PLATFORM=android-{ANDROID_MIN_SDK}", "-DANDROID_STL=c++_static", "-DHAYLEN_BUILD_TESTS=OFF"]
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
    raise BuildError(f"{name} is missing from the CMake cache in {directory}.")


def ensure_configured(args: argparse.Namespace) -> Path:
    """Configures the build tree when it does not exist yet or when the requested options differ from the ones it was configured with."""
    directory = build_dir(args.platform, args.config)
    if not (directory / "CMakeCache.txt").exists():
        command_configure(args)
        return directory

    if cmake_cache_value(directory, "HAYLEN_RENDER_BACKEND") != requested_backend(args) or cmake_cache_value(directory, "HAYLEN_ENABLE_SANITIZERS") != requested_sanitizers(args):
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
    run(["ctest", "--test-dir", build_dir(args.platform, args.config), "-C", args.config, "--output-on-failure", "--parallel", str(args.jobs)])


def llvm_tool(name: str) -> str:
    if host_name() == "macos":
        return capture(["xcrun", "--find", name]).strip()
    tool = shutil.which(name)
    if tool is None:
        raise BuildError(f"{name} was not found. Install LLVM to produce coverage reports.")
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
    """Lists multi-line lambdas outside clang-format off regions, since clang-format cannot lay them out acceptably."""
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
                findings.append(f"{path.relative_to(ROOT)}:{number}: multi-line lambda outside clang-format off")
    return findings


def command_format(args: argparse.Namespace) -> None:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        raise BuildError("clang-format was not found on PATH.")

    files = format_sources()
    command = [clang_format, "--style=file"] + (["--dry-run", "--Werror"] if args.check else ["-i"])
    for start in range(0, len(files), 200):
        run(command + files[start : start + 200])

    findings = unguarded_lambdas(files)
    for finding in findings:
        print(finding)
    if findings and args.check:
        raise BuildError(f"{len(findings)} multi-line lambdas need clang-format off and on markers.")


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
    """Builds the engine SDK for a platform and installs it where find_package(haylen) finds it."""
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
    """Hashes every engine file that reaches an artifact, so make.py knows when the artifacts are stale."""
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
    """Builds one static library per architecture, joins the architectures of every slice with lipo and creates Haylen.xcframework."""
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


def android_varn_sources(config: str) -> str:
    """Configures the native Android tree, whose CMake cache tells where CPM placed Varn, because Gradle compiles the Kotlin transport of Varn."""
    return cmake_cache_value(ensure_configured(build_options("android", config)), "varn_SOURCE_DIR")


def build_android_artifacts(config: str, jobs: int) -> None:
    """Builds the haylen Android library with the Lua player for every ABI and publishes it to the local Maven repository of the artifacts."""
    maven = ARTIFACTS_DIR / "android" / "maven"
    shutil.rmtree(maven, ignore_errors=True)
    task = ":haylen:publishReleasePublicationToArtifactsRepository"
    run([ensure_gradle(), "-p", ANDROID_LIBRARY_PROJECT, task, f"--max-workers={jobs}", f"-PhaylenSokolShdc={ensure_shdc()}", f"-PhaylenVarnSourceDir={android_varn_sources(config)}", f"-PhaylenMavenDir={maven}"])


def build_web_artifacts(config: str, jobs: int) -> None:
    """Builds the player for WebGPU and for WebGL2. It loads the app package at runtime, so one build serves every app."""
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        command_build(build_options(platform_name, config, "haylen", jobs))
        built = build_dir(platform_name, config) / "bin" / "haylen"
        destination = ARTIFACTS_DIR / "web" / backend
        shutil.rmtree(destination, ignore_errors=True)
        destination.mkdir(parents=True)
        for suffix in (".js", ".wasm"):
            shutil.copy2(built / f"haylen{suffix}", destination)


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
    print(f"The {platform_name} artifacts of Haylen {engine_version()} are in {ARTIFACTS_DIR}")


def ensure_artifacts(platform_name: str, config: str, jobs: int) -> None:
    """Builds the artifacts of a platform when they are missing, were built with another configuration or are older than the engine sources."""
    entry = read_manifest()["platforms"].get(platform_name)
    if entry == {"config": config, "sources": engine_sources_hash()}:
        return
    print(f"The {platform_name} artifacts are missing or stale, so make.py builds them first.", flush=True)
    build_artifacts(platform_name, config, jobs)


def command_engine(args: argparse.Namespace) -> None:
    for platform_name in ARTIFACT_PLATFORMS if args.platform == "all" else [args.platform]:
        build_artifacts(platform_name, args.config, args.jobs)


# Apps: a package folder assembled with a platform template into a project under build/apps, then built and launched.


def resolve_app(value: str) -> Path:
    """Accepts an app folder or the name of a sample."""
    candidate = Path(value).expanduser()
    if not candidate.exists() and (SAMPLES_DIR / value).is_dir():
        candidate = SAMPLES_DIR / value
    folder = candidate.resolve()
    if not (folder / "app.json").is_file():
        raise BuildError(f"{value} is neither an app folder with an app.json nor a sample path from samples/, such as games/tiny-island. List them with: python3 make.py samples")
    return folder


def parse_color(text: str) -> tuple[int, int, int, int]:
    """Reads #RRGGBB or #AARRGGBB like the engine and returns red, green, blue and alpha."""
    digits = text.removeprefix("#")
    if len(digits) not in (6, 8) or not re.fullmatch(r"[0-9A-Fa-f]+", digits):
        raise BuildError(f"{text} is not a color. Colors are #RRGGBB or #AARRGGBB.")
    value = int(digits, 16)
    if len(digits) == 6:
        return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 0xFF
    return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, (value >> 24) & 0xFF


class App:
    """What the platform projects need from an app folder, with the defaults the engine applies to app.json."""

    def __init__(self, folder: Path) -> None:
        self.folder = folder
        document = json.loads((folder / "app.json").read_text())
        for key in ("name", "identifier", "version"):
            if not isinstance(document.get(key), str) or not document[key]:
                raise BuildError(f"The app.json of {folder} needs a {key} to build an app.")
        self.name: str = document["name"]
        self.identifier: str = document["identifier"]
        self.version: str = document["version"]
        self.orientation: str = document.get("orientation", "landscape")
        if self.orientation not in ANDROID_ORIENTATIONS:
            raise BuildError(f"The orientation of {folder}/app.json must be landscape, portrait or any.")
        if not re.fullmatch(r"\d+(\.\d+){0,2}", self.version):
            raise BuildError(f"The version of {folder}/app.json must be one to three numbers separated by dots, such as 1.2.0.")

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
                raise BuildError(f"The splash logo {self.splash_logo} of {folder}/app.json does not exist.")

        native = document.get("native", {})
        if not isinstance(native, dict):
            raise BuildError(f"The native section of {folder}/app.json maps library names to their files or CMake projects.")
        self.native = [NativeLibrary.parse(folder, name, entry) for name, entry in native.items()]

    @property
    def slug(self) -> str:
        return self.folder.name

    @property
    def version_code(self) -> int:
        """Android needs an integer that grows with every version, so 1.2.3 becomes 1002003."""
        parts = [int(part) for part in self.version.split(".")] + [0, 0]
        return parts[0] * 1_000_000 + parts[1] * 1_000 + parts[2]


def copy_package(app: App, destination: Path) -> list[str]:
    """Copies app.json, source and content into a folder and returns the copied paths, relative to it."""
    copied: list[str] = []
    for path in [app.folder / "app.json", *sorted(path for name in PACKAGE_FOLDERS for path in (app.folder / name).rglob("*"))]:
        if path.is_file() and path.name != ".DS_Store":
            relative = path.relative_to(app.folder).as_posix()
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            copied.append(relative)
    return copied


def package_folder(folder: Path, output: Path) -> None:
    if not (folder / "app.json").is_file():
        raise BuildError(f"{folder} is not an app package because it has no app.json.")
    files = [folder / "app.json", *sorted(path for name in PACKAGE_FOLDERS for path in (folder / name).rglob("*"))]
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            if path.is_file() and path.name != ".DS_Store":
                archive.write(path, path.relative_to(folder).as_posix())
    print(f"Packaged {folder} into {output}")


# Native libraries: what the native section of app.json lists, prebuilt or built from a CMake project, placed where each platform package loads it.

NATIVE_PLATFORMS = ("macos", "ios", "tvos", "android", "windows", "linux")
# The ABIs of the haylen Android library, which the native libraries of an app match.
ANDROID_ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64")
# The slice of APPLE_SLICES that each Apple run platform builds, and the target and platform whose embed phase ships its libraries in App.xcodeproj.
APPLE_NATIVE_SLICES = {"macos": "macos", "catalyst": "ios-maccatalyst", "ios": "ios", "ios-simulator": "ios-simulator", "tvos": "tvos", "tvos-simulator": "tvos-simulator"}
APPLE_NATIVE_KEYS = {"macos": "macOS-macosx", "ios-maccatalyst": "iOS-macosx", "ios": "iOS-iphoneos", "ios-simulator": "iOS-iphonesimulator", "tvos": "tvOS-appletvos", "tvos-simulator": "tvOS-appletvsimulator"}
XCFRAMEWORK_SLICES = {"macos": ("macos", None), "ios-maccatalyst": ("ios", "maccatalyst"), "ios": ("ios", None), "ios-simulator": ("ios", "simulator"), "tvos": ("tvos", None), "tvos-simulator": ("tvos", "simulator")}
FRAMEWORK_PLATFORMS = {"ios": "iPhoneOS", "ios-simulator": "iPhoneSimulator", "tvos": "AppleTVOS", "tvos-simulator": "AppleTVSimulator"}


@dataclasses.dataclass(frozen=True)
class NativeLibrary:
    """A native library of an app: prebuilt files per platform, or a target of a CMake project that make.py builds for each listed platform. iOS and tvOS may link it statically, and then the symbols that Lua reaches are listed."""

    name: str
    files: dict[str, Path]
    cmake: Path | None
    platforms: tuple[str, ...]
    static: bool
    symbols: tuple[str, ...]

    @staticmethod
    def parse(folder: Path, name: str, entry: object) -> "NativeLibrary":
        where = f"The native library {name} in {folder}/app.json"
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", name) or not isinstance(entry, dict):
            raise BuildError(f"{where} needs a name of letters, digits and underscores and an object with its files or its CMake project.")
        unknown = set(entry) - {"files", "cmake", "platforms", "link", "symbols"}
        if unknown:
            raise BuildError(f"{where} has unknown keys: {', '.join(sorted(unknown))}.")
        if ("files" in entry) == ("cmake" in entry) or ("cmake" in entry) != ("platforms" in entry):
            raise BuildError(f"{where} lists either its prebuilt files by platform or a CMake project with the platforms it builds for.")

        files = {platform: folder / path for platform, path in entry.get("files", {}).items()}
        platforms = tuple(entry.get("platforms", files.keys()))
        if "android" in files and not files["android"].is_dir():
            raise BuildError(f"{where} gives Android a folder with a subfolder of libraries for each ABI, like jniLibs.")
        for platform in platforms:
            if platform not in NATIVE_PLATFORMS:
                raise BuildError(f"{where} names the unknown platform {platform}. Native libraries ship to {', '.join(NATIVE_PLATFORMS)}.")
        for path in files.values():
            if not path.exists():
                raise BuildError(f"{where} lists {path}, which does not exist.")
        cmake = folder / entry["cmake"] if "cmake" in entry else None
        if cmake is not None and not (cmake / "CMakeLists.txt").is_file():
            raise BuildError(f"{where} names the CMake project {cmake}, which has no CMakeLists.txt.")

        link = entry.get("link", "dynamic")
        symbols = tuple(entry.get("symbols", ()))
        if link not in ("dynamic", "static"):
            raise BuildError(f"{where} links dynamic or static, not {link}.")
        if link == "static" and (not set(platforms) <= {"ios", "tvos"} or not symbols):
            raise BuildError(f"{where} links statically, which iOS and tvOS apps do, and lists the symbols that Lua calls.")
        return NativeLibrary(name, files, cmake, platforms, link == "static", symbols)

    def ships_to(self, platform: str) -> bool:
        return platform in self.platforms


def build_native_target(library: NativeLibrary, directory: Path, options: list[str], jobs: int) -> Path:
    """Configures and builds the CMake target of a library, which shares the name of the library, and returns the folder with its output. BUILD_SHARED_LIBS picks the kind of library, and HAYLEN_INCLUDE_DIR leads to haylen/platform/native/HaylenNative.h."""
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
    folder = APPS_DIR / app.slug / "native" / library.name / slice_name
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
    raise BuildError(f"{path} has no slice for {slice_name}.")


def copy_native(source: Path, folder: Path) -> Path:
    """Copies a library file or bundle into a folder, keeping the links inside frameworks."""
    folder.mkdir(parents=True, exist_ok=True)
    destination = folder / source.name
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=True, dirs_exist_ok=True)
    else:
        shutil.copy2(source, destination)
    return destination


def prepare_apple_native(app: App, project: Path, run_platform: str, jobs: int) -> list[str]:
    """Places the libraries of an Apple run in native/ with the file lists of the embed phase of every target and platform, writes the table of linked symbols into source/ and returns the settings that link static libraries."""
    slice_name = APPLE_NATIVE_SLICES[run_platform]
    key = APPLE_NATIVE_KEYS[slice_name]
    platform = "macos" if slice_name == "macos" else slice_name.split("-")[0]
    embedded: list[Path] = []
    linked: list[tuple[NativeLibrary, Path]] = []
    for library in app.native:
        if not library.ships_to(platform):
            continue
        if library.static and slice_name == "ios-maccatalyst":
            raise BuildError(f"Mac Catalyst loads dynamic libraries only, so the static library {library.name} does not ship there.")
        source = build_apple_native(app, library, slice_name, jobs) if library.cmake else prebuilt_apple_native(library.files[platform], slice_name)
        placed = copy_native(source, project / "native" / key)
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

    # Dead code stripping keeps what the table refers to, and the table lets haylen.native find the symbols without the app exporting them.
    target_name, platform_name = key.split("-")
    condition = "TARGET_OS_IOS && !TARGET_OS_MACCATALYST" if platform == "ios" else "TARGET_OS_TV"
    declarations = [f'extern "C" void {symbol}(void);' for library, _ in linked for symbol in library.symbols]
    registrations = []
    for library, _ in linked:
        entries = ", ".join(f'{{"{symbol}", reinterpret_cast<void*>(&{symbol})}}' for symbol in library.symbols)
        registrations.append(f'    haylen::platform::NativeLibraries::registerLinked("{library.name}", {{{entries}}});')
    table = [
        "// Written by make.py from the native section of app.json. It links the symbols of the static native libraries into the app and registers them for haylen.native.",
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
    """Places the libraries of an app in the jniLibs folders of the Android project, building each ABI one after the other."""
    libraries = project / "app" / "src" / "main" / "jniLibs"
    for library in app.native:
        if not library.ships_to("android"):
            continue
        if library.cmake is None:
            shutil.copytree(library.files["android"], libraries, dirs_exist_ok=True)
            continue
        toolchain = android_ndk() / "build" / "cmake" / "android.toolchain.cmake"
        for abi in ANDROID_ABIS:
            options = [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", f"-DANDROID_ABI={abi}", f"-DANDROID_PLATFORM=android-{ANDROID_MIN_SDK}", "-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON"]
            output = build_native_target(library, APPS_DIR / app.slug / "native" / library.name / f"android-{abi}", options, jobs)
            copy_native(output / f"lib{library.name}.so", libraries / abi)


def prepare_host_native(app: App, folder: Path, jobs: int) -> list[Path]:
    """Places the libraries of an app for this desktop in a folder and returns them."""
    platform = host_name()
    placed = []
    for library in app.native:
        if not library.ships_to(platform):
            continue
        if library.cmake is None:
            placed.append(copy_native(prebuilt_apple_native(library.files[platform], "macos") if platform == "macos" else library.files[platform], folder))
            continue
        options = [f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}"] if platform == "macos" else []
        output = build_native_target(library, APPS_DIR / app.slug / "native" / library.name / platform, options, jobs)
        built = sorted(output.glob({"macos": f"lib{library.name}.dylib", "windows": f"*{library.name}.dll", "linux": f"lib{library.name}.so"}[platform]))
        if not built:
            raise BuildError(f"The CMake target {library.name} of {library.cmake} built no shared library into {output}.")
        placed.append(copy_native(built[0], folder))
    return placed


# Shaders: annotated GLSL under content/shaders compiled ahead of time into one .shader file per source, because no platform, the web editor included, compiles shaders at runtime.


def parse_shdc_yaml(text: str) -> dict:
    """Reads the reflection YAML that sokol-shdc writes with --format bare_yaml, whose maps and lists nest by two spaces and whose lists put every item under a lone dash."""
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
    """Compiles one annotated GLSL source into a .shader file with every program the engine draws with, for every shader language it supports, and the reflection of the uniforms and textures of the source."""
    text = source.read_text()
    declared = re.findall(r"^\s*@program\s+(\w+)\s+(\w+)\s+(\w+)", text, re.MULTILINE)
    if len(declared) != 1 or declared[0][1] != "haylen_vs":
        raise BuildError(f"{source} must declare exactly one @program whose vertex shader is haylen_vs from haylen/material.glsl.")
    name = declared[0][0]

    def shdc(arguments: list, program: str) -> None:
        compiled = subprocess.run([str(part) for part in [ensure_shdc(), "--input", source.resolve(), *arguments]], cwd=SHADER_LIBRARY_DIR, capture_output=True, text=True)
        if compiled.returncode != 0:
            raise BuildError(f"sokol-shdc could not compile the {program} program of {source}:\n{compiled.stdout}{compiled.stderr}".rstrip())

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
    print(f"Compiled {source} into {output}", flush=True)


def shader_sources(folder: Path) -> list[Path]:
    """Lists the sources of an app that declare a program, which are the ones make.py compiles, while the others are files they include."""
    shaders = folder / "content" / "shaders"
    return sorted(path for path in shaders.rglob("*.glsl") if re.search(r"^\s*@program\b", path.read_text(), re.MULTILINE)) if shaders.is_dir() else []


def newest_shader_source(folder: Path) -> float:
    """Returns the time of the newest shader source of an app or the shader library, which every compiled shader of the app must be newer than."""
    return max(path.stat().st_mtime for path in [*(folder / "content" / "shaders").rglob("*.glsl"), *SHADER_LIBRARY_DIR.rglob("*.glsl")])


def compile_app_shaders(folder: Path) -> None:
    """Compiles the shaders of an app whose .shader file is older than its source, the other sources of the app that it may include or the shader library."""
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
            print(f"error: {error}", file=sys.stderr, flush=True)


def command_shaders(args: argparse.Namespace) -> None:
    folder = resolve_app(args.app)
    if not shader_sources(folder):
        print(f"{folder} has no shaders under content/shaders.")
        return
    if args.force:
        for source in shader_sources(folder):
            compile_shader(source, source.with_suffix(".shader"))
        return
    compile_app_shaders(folder)


def platform_templates() -> list[str]:
    return sorted(path.name for path in PLATFORM_TEMPLATES_DIR.iterdir() if path.is_dir())


def assemble(app: App, template: str | None, run_platform: str) -> Path:
    """Recreates build/apps/<app>/<platform> from the platform template and lays the platform/<template> folder of the app over it. Platforms without a template start empty and take the platform/<platform> folder of the app, such as the libraries a Windows app keeps next to its executable."""
    folder = APPS_DIR / app.slug / run_platform
    shutil.rmtree(folder, ignore_errors=True)
    if template is None:
        folder.mkdir(parents=True)
    else:
        shutil.copytree(PLATFORM_TEMPLATES_DIR / template, folder, symlinks=True)
    overrides = app.folder / "platform" / (template or run_platform)
    if overrides.is_dir():
        shutil.copytree(overrides, folder, dirs_exist_ok=True, ignore=shutil.ignore_patterns(".DS_Store", "build", ".gradle", ".cxx", ".kotlin"))
    return folder


def write_if_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file() or path.read_text() != text:
        path.write_text(text)


def apple_colorset(color: tuple[int, int, int, int]) -> dict:
    red, green, blue, alpha = color
    components = {"red": f"{red / 255:.3f}", "green": f"{green / 255:.3f}", "blue": f"{blue / 255:.3f}", "alpha": f"{alpha / 255:.3f}"}
    return {"colors": [{"color": {"color-space": "srgb", "components": components}, "idiom": "universal"}], "info": {"author": "xcode", "version": 1}}


def write_apple_settings(app: App, project: Path, native: list[str]) -> None:
    """Writes App.xcconfig with the settings that link the static native libraries, the Info.plist of every platform and the splash assets of an app into the Apple project."""
    xcconfig = "\n".join([
        "// Written by make.py from app.json, so App.xcodeproj never changes per app.",
        f"HAYLEN_PRODUCT_NAME = {app.name}",
        f"HAYLEN_BUNDLE_IDENTIFIER = {app.identifier}",
        f"MARKETING_VERSION = {app.version}",
        f"CURRENT_PROJECT_VERSION = {app.version}",
        *native,
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
    # sokol_app creates its window from the scene that UIKit connects, so iOS and tvOS apps declare the scene life cycle with one scene.
    scenes = {"UIApplicationSceneManifest": {"UIApplicationSupportsMultipleScenes": False}}
    phone, pad = IOS_ORIENTATIONS[app.orientation]
    plists = {
        "ios": {**common, **scenes, "LSRequiresIPhoneOS": True, "UILaunchStoryboardName": "LaunchScreen", "UIStatusBarHidden": True, "UIViewControllerBasedStatusBarAppearance": False, "UISupportedInterfaceOrientations": phone, "UISupportedInterfaceOrientations~ipad": pad},
        "tvos": {**common, **scenes, "UILaunchStoryboardName": "LaunchScreen"},
        "macos": {**common, "LSMinimumSystemVersion": "$(MACOSX_DEPLOYMENT_TARGET)", "NSHighResolutionCapable": True, "NSPrincipalClass": "NSApplication", **({} if app.show_in_taskbar else {"LSUIElement": True})},
    }
    for platform_name, values in plists.items():
        write_if_changed(project / platform_name / "Info.plist", plistlib.dumps(values, sort_keys=True).decode())

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
            raise BuildError(f"No available {family} simulator is named {requested}.")
        return matches[0]
    if not candidates:
        raise BuildError(f"No {family} simulator is installed. Install the runtime with xcodebuild -downloadPlatform {family}.")
    booted = [device for device in candidates if device["state"] == "Booted"]
    preferred = [device for device in candidates if device["name"].startswith("iPhone")] if family == "iOS" else candidates
    return (booted or preferred or candidates)[0]


def run_apple(app: App, project: Path, args: argparse.Namespace) -> None:
    require_host("apple")
    (project / "Haylen.xcframework").symlink_to(ARTIFACTS_DIR / "apple" / "Haylen.xcframework")
    copy_package(app, project / "app")
    write_apple_settings(app, project, prepare_apple_native(app, project, args.platform, args.jobs))

    settings = APPLE_RUNS[args.platform]
    simulator = apple_simulator(settings["simulator"], args.device) if "simulator" in settings else None
    destination = f"id={simulator['udid']}" if simulator else settings["destination"]
    if args.platform in {"macos", "catalyst"}:
        destination += f",arch={'arm64' if host_arch() == 'arm64' else 'x86_64'}"
    derived = project / "build"
    command = ["xcodebuild", "-project", project / "App.xcodeproj", "-scheme", settings["scheme"], "-configuration", args.config, "-destination", destination, "-derivedDataPath", derived, "build"]
    team = os.environ.get("HAYLEN_APPLE_TEAM")
    if args.platform in {"ios", "tvos"}:
        if not team:
            raise BuildError("Apps for Apple devices are signed with a development team. Set HAYLEN_APPLE_TEAM to its id.")
        command += ["-allowProvisioningUpdates", f"DEVELOPMENT_TEAM={team}"]
    run(command)

    bundle = next((derived / "Build" / "Products").glob("*/*.app"))
    if args.platform in {"macos", "catalyst"}:
        executable = bundle / "Contents" / "MacOS" / bundle.stem
        run([executable])
    elif simulator:
        udid = simulator["udid"]
        if simulator["state"] != "Booted":
            run(["xcrun", "simctl", "boot", udid])
        run(["xcrun", "simctl", "bootstatus", udid, "-b"])
        run(["xcrun", "simctl", "install", udid, bundle])
        run(["xcrun", "simctl", "launch", "--console-pty", "--terminate-running-process", udid, app.identifier])
    else:
        if not args.device:
            raise BuildError("Name the device with --device. xcrun devicectl list devices lists them.")
        run(["xcrun", "devicectl", "device", "install", "app", "--device", args.device, bundle])
        run(["xcrun", "devicectl", "device", "process", "launch", "--console", "--device", args.device, app.identifier])


def write_android_settings(app: App, project: Path) -> None:
    """Points the Gradle project at the engine repository and writes the identity, version, orientation and splash of an app."""
    properties = project / "gradle.properties"
    values = {
        "haylen.repository": (ARTIFACTS_DIR / "android" / "maven").as_posix(),
        "haylen.engineVersion": engine_version(),
        "haylen.name": app.name,
        "haylen.identifier": app.identifier,
        "haylen.versionName": app.version,
        "haylen.versionCode": str(app.version_code),
        "haylen.orientation": ANDROID_ORIENTATIONS[app.orientation],
    }
    lines = [line for line in properties.read_text().splitlines() if not line.startswith("haylen.")]
    lines += [f"{key}={value}" for key, value in values.items()]
    properties.write_text("\n".join(lines) + "\n")

    # The splash resources of the app replace the defaults of the haylen library, which show the engine logo.
    resources = project / "app" / "src" / "main" / "res"
    red, green, blue, alpha = app.background
    colors = f'<?xml version="1.0" encoding="utf-8"?>\n<!-- Written by make.py from the splash of app.json. -->\n<resources>\n    <color name="haylen_splash_background">#{alpha:02X}{red:02X}{green:02X}{blue:02X}</color>\n</resources>\n'
    write_if_changed(resources / "values" / "haylen_splash.xml", colors)
    if app.splash_logo:
        if app.splash_logo.suffix.lower() not in {".png", ".webp", ".jpg", ".jpeg"}:
            raise BuildError(f"Android splash logos are PNG, WebP or JPEG images, not {app.splash_logo.name}.")
        shutil.copy2(app.splash_logo, resources / "drawable" / f"haylen_splash_logo{app.splash_logo.suffix.lower()}")


def android_device(requested: str | None) -> str:
    devices = [line.split()[0] for line in capture([adb(), "devices"]).splitlines()[1:] if line.strip().endswith("device")]
    if requested:
        if requested not in devices:
            raise BuildError(f"{requested} is not a connected Android device. adb devices lists them.")
        return requested
    if len(devices) != 1:
        raise BuildError("Name the Android device with --device, because " + ("none is connected." if not devices else f"{len(devices)} are connected: {', '.join(devices)}."))
    return devices[0]


def run_android(app: App, project: Path, args: argparse.Namespace) -> None:
    files = copy_package(app, project / "app" / "src" / "main" / "assets" / "app")
    # Android cannot list asset folders recursively, so the runtime reads the files of the package from this index.
    (project / "app" / "src" / "main" / "assets" / "app" / "haylen-package-index.json").write_text(json.dumps(sorted(files)))
    write_android_settings(app, project)
    prepare_android_native(app, project, args.jobs)

    variant = "Release" if args.config == "Release" else "Debug"
    run([ensure_gradle(), "-p", project, f":app:assemble{variant}"])
    apk = project / "app" / "build" / "outputs" / "apk" / variant.lower() / f"app-{variant.lower()}.apk"
    device = android_device(args.device)
    run([adb(), "-s", device, "install", "-r", apk])
    run([adb(), "-s", device, "shell", "am", "start", "-W", "-n", f"{app.identifier}/dev.haylen.HaylenActivity"])
    process = capture([adb(), "-s", device, "shell", "pidof", app.identifier]).strip()
    if process:
        run([adb(), "-s", device, "logcat", "--pid", process])


def write_web_settings(app: App, site: Path) -> None:
    """Writes app.zip, the splash logo and config.json, whose sizes let the loader show progress when the server sends no length."""
    package_folder(app.folder, site / "app.zip")
    logo = app.splash_logo or ENGINE_LOGO
    logo_name = f"splash{logo.suffix.lower()}" if app.splash_logo else ENGINE_LOGO.name
    if app.splash_logo:
        shutil.copy2(logo, site / logo_name)
    red, green, blue, alpha = app.background
    sizes = {path: (site / path).stat().st_size for path in ("app.zip", "webgpu/haylen.wasm", "webgl2/haylen.wasm")}
    config = {"name": app.name, "transparent": app.transparent, "splash": {"logo": logo_name, "background": f"rgba({red}, {green}, {blue}, {alpha / 255:.3f})"}, "sizes": sizes}
    (site / "config.json").write_text(json.dumps(config, indent=4) + "\n")


def run_web(app: App, site: Path, args: argparse.Namespace) -> None:
    for backend in ("webgpu", "webgl2"):
        shutil.copytree(ARTIFACTS_DIR / "web" / backend, site / backend)
    write_web_settings(app, site)
    serve(site, args.port, args.coep, args.open)


def run_desktop_app(app: App, folder: Path, args: argparse.Namespace) -> None:
    """Runs the shipped layout of a Windows or Linux app: the player named after the app with the package in an app folder next to it, and the native libraries next to it on Windows and in lib on Linux, which the RUNPATH of the player covers."""
    require_host(args.platform)
    executable = folder / executable_name(app.slug)
    shutil.copy2(desktop_artifact(), executable)
    copy_package(app, folder / "app")
    prepare_host_native(app, folder if args.platform == "windows" else folder / "lib", args.jobs)
    run([executable], cwd=folder)


@dataclasses.dataclass(frozen=True)
class RunTarget:
    """A platform that make.py run builds for: the platform template it assembles, if any, the engine artifacts it needs and the function that builds and launches it."""

    template: str | None
    artifacts: str
    run: Callable[[App, Path, argparse.Namespace], None]


# A new platform is a folder under templates/platform and an entry here.
RUN_TARGETS = {
    "macos": RunTarget("apple", "apple", run_apple),
    "catalyst": RunTarget("apple", "apple", run_apple),
    "ios": RunTarget("apple", "apple", run_apple),
    "ios-simulator": RunTarget("apple", "apple", run_apple),
    "tvos": RunTarget("apple", "apple", run_apple),
    "tvos-simulator": RunTarget("apple", "apple", run_apple),
    "android": RunTarget("android", "android", run_android),
    "web": RunTarget("web", "web", run_web),
    "windows": RunTarget(None, "desktop", run_desktop_app),
    "linux": RunTarget(None, "desktop", run_desktop_app),
}


def command_run(args: argparse.Namespace) -> None:
    app = resolve_app(args.app)
    compile_app_shaders(app)
    if args.platform is None:
        # The player of this machine runs the package folder in development mode, which reloads edited files, while changed shaders compile again in the background.
        build = build_options(host_name(), args.config, "haylen", args.jobs)
        command_build(build)
        # The native libraries of the app wait in a folder of their own, which the player searches first.
        info = App(app)
        native = APPS_DIR / info.slug / "native" / "development"
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
    info = App(app)
    ensure_artifacts(target.artifacts, args.engine_config, args.jobs)
    target.run(info, assemble(info, target.template, args.platform), args)


def command_new(args: argparse.Namespace) -> None:
    """Creates an app from the starter app and a copy of every platform template, which the developer owns from then on."""
    folder = Path(args.folder).expanduser().resolve()
    if folder.exists() and any(folder.iterdir()):
        raise BuildError(f"{folder} already exists and is not empty.")

    slug = re.sub(r"[^a-z0-9]+", "-", folder.name.lower()).strip("-")
    if not slug:
        raise BuildError(f"{folder.name} gives no app name. Use a folder name with letters or digits.")
    name = args.name or " ".join(word.capitalize() for word in slug.split("-"))
    identifier = args.identifier or f"com.example.{slug.replace('-', '')}"
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+", identifier):
        raise BuildError(f"{identifier} is not a reverse domain identifier such as com.example.game.")

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
    print(f"Created {name} ({identifier}) in {folder}. Run it with: python3 make.py run {folder}")


def cpp_target(project: Path, requested: str | None) -> str:
    return requested or project.name


def command_run_cpp(args: argparse.Namespace) -> None:
    """Builds a C++ app project, which compiles the engine through its own CMake, and runs it on this machine or in the browser."""
    candidate = Path(args.project).expanduser()
    project = (candidate if candidate.exists() else SAMPLES_DIR / args.project).resolve()
    if not (project / "CMakeLists.txt").is_file():
        raise BuildError(f"{args.project} is neither a CMake project folder nor the name of a C++ sample.")
    target = cpp_target(project, args.target)

    if args.platform == "web":
        site = CPP_BUILDS_DIR / project.name / "web"
        bundle_web(project, target, args.config, site, args.jobs)
        serve(site, args.port, args.coep, args.open)
        return

    directory = CPP_BUILDS_DIR / project.name / f"{host_name()}-{args.config.lower()}"
    command = ["cmake", "-S", project, "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={args.config}"]
    if host_name() != "windows" or shutil.which("ninja"):
        command += ["-G", "Ninja"]
    run(command)
    run(["cmake", "--build", directory, "--config", args.config, "--target", target, "--parallel", str(args.jobs)])
    run([cmake_app_executable(directory, target)])


def bundle_web(source: Path, target: str, config: str, output: Path, jobs: int) -> None:
    """Builds a CMake web target for WebGPU and WebGL2 into one folder whose page runs the backend the browser supports."""
    shutil.rmtree(output, ignore_errors=True)
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        directory = CPP_BUILDS_DIR / source.name / f"{platform_name}-{config.lower()}"
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

    # The page is the shell of the target, with the script that picks the backend where Emscripten would put its own script.
    shell = (built / f"{target}.shell.html").read_text()
    picker = (ENGINE_DIR / "platform" / "web" / "backend-picker.html").read_text().replace("{{TARGET}}", target)
    if "{{{ SCRIPT }}}" not in shell:
        raise BuildError(f"The web shell of {target} has no {{{{{{ SCRIPT }}}}}} placeholder.")
    (output / "index.html").write_text(shell.replace("{{{ SCRIPT }}}", picker.strip()))
    print(f"Bundled {target} for WebGPU and WebGL2 into {output}")


def list_samples() -> list[tuple[str, str]]:
    """Returns the path from samples/ and the kind of every sample: a C++ project has a CMakeLists.txt, a Lua app only an app.json."""
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
    """Serves a folder with the headers WebAssembly pages need: cross-origin isolation for SharedArrayBuffer and threads, explicit MIME types, no caching and precompressed files."""

    coep = "require-corp"

    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        if self.coep != "off":
            self.send_header("Cross-Origin-Embedder-Policy", self.coep)
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def guess_type(self, path: str) -> str:
        return MIME_TYPES.get(Path(path).suffix.lower()) or super().guess_type(path)

    def send_head(self):
        # A request for a file that also exists as .br or .gz is answered with the compressed copy the browser accepts.
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


def serve(directory: Path, port: int, coep: str = "require-corp", open_page: bool = False) -> None:
    handler = functools.partial(type("Handler", (WebHandler,), {"coep": coep}), directory=str(directory))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as server:
        url = f"http://127.0.0.1:{port}/"
        print(f"Serving {directory} at {url}", flush=True)
        if open_page:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


def command_serve(args: argparse.Namespace) -> None:
    serve(Path(args.directory).resolve(), args.port, args.coep, args.open)


def command_clean(_: argparse.Namespace) -> None:
    shutil.rmtree(BUILD_ROOT, ignore_errors=True)
    print(f"Removed {BUILD_ROOT}")


def add_build_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--platform", default=host_name(), choices=PLATFORMS)
    parser.add_argument("--config", default="Debug", choices=CONFIGS)
    parser.add_argument("--backend", choices=["METAL", "D3D11", "GLCORE", "GLES3", "WGPU"])
    parser.add_argument("--xcode", action="store_true", help="Use the Xcode generator on macOS.")
    parser.add_argument("--sanitize", action="store_true", help="Build with AddressSanitizer and UndefinedBehaviorSanitizer.")
    parser.add_argument("--target")
    parser.add_argument("--jobs", type=int, default=default_jobs())


def add_web_server_options(parser: argparse.ArgumentParser, port: int) -> None:
    parser.add_argument("--port", type=int, default=port, help="Port of the local web server.")
    parser.add_argument("--coep", default="require-corp", choices=["require-corp", "credentialless", "off"], help="Cross-Origin-Embedder-Policy. Pages that load third-party scripts, such as Google sign-in, need credentialless or off.")
    parser.add_argument("--open", action="store_true", help="Open the page in the default browser.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Haylen build entry point.")
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    tools = commands.add_parser("tools", help="Download the pinned build tools.")
    tools.add_argument("--emsdk", action="store_true", help=f"Install Emscripten {EMSDK_VERSION} into .tools.")
    tools.add_argument("--gradle", action="store_true", help=f"Install Gradle {GRADLE_VERSION} into .tools.")
    tools.set_defaults(handler=command_tools)

    for name, handler, help_text in (("configure", command_configure, "Generate a build tree of the engine."), ("build", command_build, "Configure the engine when needed and build it."), ("test", command_test, "Build and run the engine tests on this machine.")):
        sub = commands.add_parser(name, help=help_text)
        add_build_options(sub)
        sub.set_defaults(handler=handler)

    engine = commands.add_parser("engine", help="Build the prebuilt engine artifacts that apps use, into build/artifacts.")
    engine.add_argument("--platform", default="all", choices=[*ARTIFACT_PLATFORMS, "all"])
    engine.add_argument("--config", default="Release", choices=CONFIGS)
    engine.add_argument("--jobs", type=int, default=default_jobs())
    engine.set_defaults(handler=command_engine)

    new = commands.add_parser("new", help="Create an app with the starter code and a copy of every platform template.")
    new.add_argument("folder", help="Folder of the new app, which must not exist or be empty.")
    new.add_argument("--name", help="Display name, from the folder name by default.")
    new.add_argument("--identifier", help="Reverse domain identifier, com.example.<folder> by default.")
    new.add_argument("--orientation", default="landscape", choices=["landscape", "portrait", "any"])
    new.set_defaults(handler=command_new)

    commands.add_parser("samples", help="List the samples by category, with the command that runs each one.").set_defaults(handler=command_samples)

    run_app = commands.add_parser("run", help="Run an app: in the player of this machine with hot reload, or built from the templates for a platform.")
    run_app.add_argument("app", nargs="?", default=DEFAULT_APP, help=f"App folder or sample path from samples/, {DEFAULT_APP} by default.")
    run_app.add_argument("--platform", choices=list(RUN_TARGETS), help="Build the app for a platform from its template. Without it the desktop player runs the app folder in development mode.")
    run_app.add_argument("--device", help="Simulator name or id, Apple device id or Android serial.")
    run_app.add_argument("--config", default="Debug", choices=["Debug", "Release"], help="Configuration of the player or of the platform project.")
    run_app.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the platform project uses.")
    run_app.add_argument("--jobs", type=int, default=default_jobs())
    add_web_server_options(run_app, 8000)
    run_app.set_defaults(handler=command_run)

    run_cpp = commands.add_parser("run-cpp", help="Build and run a C++ app project, which compiles the engine through CMake.")
    run_cpp.add_argument("project", help="CMake project folder or C++ sample path from samples/, such as cpp/embedding.")
    run_cpp.add_argument("--platform", default=host_name(), choices=[host_name(), "web"])
    run_cpp.add_argument("--target", help="The haylen_add_app target, named like the project folder by default.")
    run_cpp.add_argument("--config", default="Debug", choices=CONFIGS)
    run_cpp.add_argument("--jobs", type=int, default=default_jobs())
    add_web_server_options(run_cpp, 8000)
    run_cpp.set_defaults(handler=command_run_cpp)

    package = commands.add_parser("package", help="Zip the app.json, source and content of an app.")
    package.add_argument("app", help="App folder or sample path from samples/.")
    package.add_argument("-o", "--output", default="app.zip", help="Zip file to write, app.zip by default.")
    package.set_defaults(handler=command_package)

    shaders = commands.add_parser("shaders", help="Compile the shaders under content/shaders of an app into .shader files for every backend.")
    shaders.add_argument("app", help="App folder or sample path from samples/.")
    shaders.add_argument("--force", action="store_true", help="Compile every shader, including the ones that are up to date.")
    shaders.set_defaults(handler=command_shaders)

    serve_folder = commands.add_parser("serve", help="Serve a folder with the headers WebAssembly pages need.")
    serve_folder.add_argument("directory")
    add_web_server_options(serve_folder, 8000)
    serve_folder.set_defaults(handler=command_serve)

    coverage = commands.add_parser("coverage", help="Measure engine code coverage with LLVM source-based coverage.")
    coverage.add_argument("--jobs", type=int, default=default_jobs())
    coverage.add_argument("--sanitize", action="store_true")
    coverage.set_defaults(handler=command_coverage)

    formatter = commands.add_parser("format", help="Format the C, C++ and Objective-C sources with clang-format.")
    formatter.add_argument("--check", action="store_true", help="Fail instead of rewriting files.")
    formatter.set_defaults(handler=command_format)

    bench = commands.add_parser("bench", help="Build and run a benchmark in Release on this machine.")
    bench.add_argument("--suite", default="sprites", choices=["sprites", "algorithms", "procedural", "lua"], help="The sprite benchmark on the GPU, the algorithm or procedural benchmark on the CPU or the Lua bunnymark on the CPU.")
    bench.add_argument("--jobs", type=int, default=default_jobs())
    bench.set_defaults(handler=command_bench)

    sdk = commands.add_parser("sdk", help="Build the engine SDK and install it for find_package(haylen).")
    sdk.add_argument("--platform", default=host_name(), choices=sorted(DESKTOP_PLATFORMS | WEB_PLATFORMS))
    sdk.add_argument("--config", default="Release", choices=CONFIGS)
    sdk.add_argument("--output", help="Install prefix, build/sdk/haylen-<platform>-<config> by default.")
    sdk.add_argument("--jobs", type=int, default=default_jobs())
    sdk.set_defaults(handler=command_sdk)

    embedding = commands.add_parser("embedding", help="Build the C++ embedding sample through add_subdirectory, CPM or an installed SDK.")
    embedding.add_argument("--mode", default="subdirectory", choices=["subdirectory", "cpm", "package"])
    embedding.add_argument("--config", default="Debug", choices=CONFIGS)
    embedding.add_argument("--jobs", type=int, default=default_jobs())
    embedding.set_defaults(handler=command_embedding)

    assets = commands.add_parser("assets", help="Import the Tiny Swords pack into the Tiny Island sample.")
    assets.add_argument("archive", help="Path to 'Tiny Swords (Free Pack).zip'.")
    assets.set_defaults(handler=command_assets)

    commands.add_parser("map", help="Generate the Tiny Island Tiled map.").set_defaults(handler=command_map)
    commands.add_parser("clean", help="Remove all build trees.").set_defaults(handler=command_clean)

    args = parser.parse_args()
    try:
        args.handler(args)
    except (BuildError, subprocess.CalledProcessError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from error


if __name__ == "__main__":
    main()
