#!/usr/bin/env python3
"""Single entry point to build, run, test and package Haylen and its apps on every platform."""

from __future__ import annotations

import argparse
import base64
import contextlib
import copy
import dataclasses
import difflib
import functools
import hashlib
import http.server
import json
import os
import platform as host_platform
import plistlib
import re
import shlex
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import textwrap
import threading
import time
import urllib.parse
import urllib.request
import webbrowser
import xml.etree.ElementTree
import zipfile
from collections.abc import Mapping
from pathlib import Path
from typing import Callable, Iterator, TextIO

ROOT = Path(__file__).resolve().parent
BUILD_ROOT = ROOT / "build"
TOOLS_DIR = ROOT / ".tools"
ENGINE_DIR = ROOT / "engine"
TEMPLATES_DIR = ROOT / "templates"
# The templates of apps come in layers: `common` holds what every kind of app shares, and the folder of each kind what only that kind has, the package and the platform projects under `platform/<name>`. A new app or platform project takes the files of `common` first and then those of its kind, whose file replaces a common file of the same path.
COMMON_TEMPLATE = TEMPLATES_DIR / "common"
APP_KINDS = {"lua": "Lua", "cpp": "C++"}
PLUGIN_TEMPLATE = TEMPLATES_DIR / "plugin"
# The folder that haylen.py writes inside a platform project, and the only one it writes there.
GENERATED_FOLDER = "haylen"
ARTIFACTS_DIR = BUILD_ROOT / "artifacts"
ENGINE_BUILDS_DIR = BUILD_ROOT / "engine"
APPS_DIR = BUILD_ROOT / "apps"
CPP_BUILDS_DIR = BUILD_ROOT / "cpp"
ANDROID_LIBRARY_PROJECT = ENGINE_DIR / "platform" / "android"
ENGINE_LOGO = ENGINE_DIR / "platform" / "web" / "haylen-logo.svg"
# The web runtime loads the AudioWorklet processor of its audio output from next to its script, so every web build ships it beside the `.js` and `.wasm` files.
WEB_AUDIO_WORKLET = "haylen-audio-worklet.js"
# The worker of the content store of a web page, which keeps downloaded content in the private file system of the origin.
WEB_CONTENT_STORE = "haylen-content-store.js"
# The files of the runtime of a web page, which travel together next to its script.
WEB_RUNTIME_FILES = ("haylen.js", "haylen.wasm", WEB_AUDIO_WORKLET, WEB_CONTENT_STORE)
# The project of the engine tests that adds the engine like another project does, in each of the ways of `CONSUMER_MODES`, which `sdk --check-consumers` builds.
CONSUMER_PROJECT = ENGINE_DIR / "tests" / "consumer"
CONSUMER_MODES = {"subdirectory": '"add_subdirectory"', "cpm": "CPM", "package": 'the installed SDK and "find_package"'}
# The keys that `android-key` writes into `keystore/` of an Android project, where the Gradle scripts of the template find them, with the size, validity and default name of a new key.
ANDROID_KEY_FOLDER = "keystore"
ANDROID_KEY_SIZE = 2048
ANDROID_KEY_DAYS = 10000
ANDROID_KEY_NAME = "CN=Upload, OU=Upload, O=Upload, L=Upload, ST=Upload, C=BR"
# The command `keytool` refuses passwords shorter than this.
ANDROID_KEY_PASSWORD_LENGTH = 6

SHDC_COMMIT = "b975d5642093d62e76a1e088bdae945d6d8b8ff2"
# App shaders include the shader library as `haylen/material.glsl`, which sokol-shdc finds from its working directory.
SHADER_LIBRARY_DIR = ENGINE_DIR / "shaders" / "include"
SHADER_SLANGS = "glsl430:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl"
# Every material compiles once per kind of draw and once more for lit canvases, and the engine picks the program of a draw by these names.
SHADER_PROGRAMS = {"sprite": [], "sprite_lit": ["HAYLEN_LIT"], "batch": ["HAYLEN_BATCH"], "batch_lit": ["HAYLEN_BATCH", "HAYLEN_LIT"], "text": ["HAYLEN_TEXT"], "text_lit": ["HAYLEN_TEXT", "HAYLEN_LIT"], "mesh": ["HAYLEN_MESH"], "mesh_lit": ["HAYLEN_MESH", "HAYLEN_LIT"], "shape": ["HAYLEN_SHAPE"], "shape_lit": ["HAYLEN_SHAPE", "HAYLEN_LIT"]}
# The uniform blocks and textures of the shader library, which the engine fills itself.
SHADER_ENGINE_BLOCKS = {"haylen_vs_params", "haylen_lit_params"}
SHADER_ENGINE_TEXTURES = {"sprite_texture", *(f"sprite_texture_{index}" for index in range(1, 8))}
SHADER_WATCH_SECONDS = 0.5
GRADLE_VERSION = "9.8.0"
# XcodeGen generates the Apple projects from their `project.yml`. The hash is the one of the `xcodegen.zip` asset of the release.
XCODEGEN_VERSION = "2.46.0"
XCODEGEN_SHA256 = "4d9e34b62172d645eed6457cac13fc222569974098ef4ee9c3368bedf0196806"
# The AppImage generator of CPack packs a Linux app with appimagetool, starts it with the AppImage runtime and points the libraries it bundles at the image with patchelf. The hashes are the ones of the release assets of each architecture.
APPIMAGETOOL_VERSION = "1.9.1"
APPIMAGE_RUNTIME_VERSION = "20251108"
PATCHELF_VERSION = "0.19.2"
APPIMAGE_TOOLS_SHA256 = {
    "x86_64": {"appimagetool": "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0", "runtime": "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d", "patchelf": "2abdc34fbe949c995a1baa4ec310896a085588a45a99cd0d726851bdfac2a4bb"},
    "aarch64": {"appimagetool": "f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158", "runtime": "00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444", "patchelf": "ba6850c1a6f4cbdb050e1cc22fa5239159ad31c43606846e49c408d335f594ff"},
}
# The installer generator of CPack makes the Windows installer with Inno Setup. The hash is the one of the x64 installer of the release.
INNO_SETUP_VERSION = "7.1.0"
INNO_SETUP_SHA256 = "0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f"
ANDROID_NDK_VERSION = "30.0.16248370"
EMSDK_VERSION = "6.0.10"
# The miniaudio library plays through AAudio from Android 8.1 on, and the engine builds it without OpenSL ES, like the `minSdk` of the Android library and template.
ANDROID_MIN_SDK = 27
# The ABIs of the `haylen` Android library, which the native libraries of an app match: 32-bit ARM keeps the Android TV devices that still run it, and `x86_64` serves emulators.
ANDROID_ABIS = ("arm64-v8a", "armeabi-v7a", "x86_64")
# The library that the activity of a release build loads instead of the Lua player of the `haylen` library: the Lua player with the bootstrap of the app.
ANDROID_RELEASE_LIBRARY = "haylen_app"
# The file in the assets of an Android project that names the plugins whose classes the runtime loads, written by `prepare`.
ANDROID_PLUGINS_FILE = "haylen-plugins.json"
# The Android libraries that `haylen.py engine` publishes as `dev.haylen:<module>`, each a module of the Gradle project of the engine.
ANDROID_LIBRARIES = ("haylen", "haylen-plugins", "haylen-links", "haylen-coroutines")
# The host tool that builds, verifies, inspects, compares and publishes the protected releases of apps with the format library of the engine, which the desktop artifacts carry.
CONTENT_TOOL = "haylen-content"
# The variable that names the folder of the key folders of apps, as continuous integration gives it, instead of the configuration folder of the user.
KEYS_VARIABLE = "HAYLEN_KEYS_DIR"
# The content profile of each run platform, which a protected release is built for. The Apple platforms share one release, since their targets share one project.
CONTENT_PROFILES = {"macos": "apple", "catalyst": "apple", "ios": "apple", "ios-simulator": "apple", "tvos": "apple", "tvos-simulator": "apple", "android": "android", "web": "web", "windows": "windows", "linux": "linux"}
# A compaction of a publication tree keeps the current generation of every channel and the one before it.
CONTENT_KEPT_GENERATIONS = 2
# The source of the bootstrap that a release build compiles in, which the content tool writes from the key folder of the app.
BOOTSTRAP_SOURCE = "HaylenBootstrap.cpp"
# The Swift helpers of the bridge and of plugin contexts, which every Apple target with Swift compiles from the engine, since a Swift module in the artifact would have to match the Swift compiler of each app.
APPLE_SWIFT_HELPERS = "HaylenBridgeAsync.swift"
# The files of a release folder besides its shards, and the suffix of the shards.
RELEASE_MANIFESTS = ("app.hmanifest", "content.hmanifest")
RELEASE_SHARD_SUFFIX = ".hpak"
# The largest protected release that the store of a content profile accepts inside an app, with the words that name the limit: the App Store limits the uncompressed size of an iOS or tvOS app, and Google Play the compressed download of the base module, whose shards are compressed already.
STORE_RELEASE_LIMITS = {"apple": (4 * 1024**3, "the 4 GB that the App Store accepts for the uncompressed size of an iOS or tvOS app"), "android": (200 * 1024**2, "the 200 MB that Google Play accepts for the compressed download of the base module of an app")}
# Symbol files, which stay with the developer and never ship.
SYMBOL_SUFFIXES = (".pdb", ".debug", ".dwo", ".dsym", ".map", ".symbols")
# A file of the package smaller than this may match an unrelated file of a release by chance, such as an empty file, so the check of raw files leaves it out.
RELEASE_MATCH_MINIMUM = 16
# The oldest Apple systems the engine runs on: `std::format` with floating point, which the engine formats text and logs with, reaches their C++ library in iOS and tvOS 16.3, and `sokol_app` draws macOS frames through `-[NSView displayLinkWithTarget:selector:]`, which macOS 14.0 introduced.
# Mac Catalyst takes its minimum, the iOS version, from `engine/cmake/haylen-catalyst.toolchain.cmake`.
APPLE_MINIMUM_VERSIONS = {"iOS": "16.3", "tvOS": "16.3", "macOS": "14.0"}

PLATFORMS = ["macos", "linux", "windows", "ios", "tvos", "android", "web", "web-webgl2"]
DESKTOP_PLATFORMS = {"macos", "linux", "windows"}
WEB_PLATFORMS = {"web", "web-webgl2"}
CONFIGS = ["Debug", "Release", "RelWithDebInfo"]
ARTIFACT_PLATFORMS = ["apple", "android", "web", "desktop"]
FORMAT_EXTENSIONS = {".h", ".hpp", ".c", ".cpp", ".m", ".mm"}
FORMAT_ROOTS = ["engine/include", "engine/src", "engine/tests", "engine/tools", "samples", "templates"]
# Build outputs inside those roots, such as the native tree Gradle keeps in each Android project, hold generated and third-party code.
FORMAT_SKIPPED_FOLDERS = {".cxx", ".gradle", "build", "_deps"}
# A package is `app.json` with the Lua modules under `source` and the assets under `content`, and nothing else in its folder ships.
PACKAGE_FOLDERS = ("source", "content")
# The CMake file that tells haylen_add_app the Apple parts of the plugins of a C++ app, and the kinds of source files it compiles.
CPP_PLUGINS_FILE = "haylen-plugins.cmake"
CPP_APPLE_PLUGIN_SOURCES = {".swift", ".m", ".mm", ".c", ".cpp"}
# The list of the web parts of the plugins of a C++ app, which the page of the app reads next to it.
CPP_WEB_PLUGINS = "plugins.json"
# Build outputs and Finder files that never travel with a copied platform folder or plugin.
COPY_IGNORED = shutil.ignore_patterns(".DS_Store", ".git", "build", ".gradle", ".cxx", ".kotlin")
# Engine files that never reach an artifact, so editing them keeps the artifacts fresh.
ENGINE_HASH_SKIPPED = {"tests", "bench", "build", ".cxx", ".gradle", ".kotlin", ".DS_Store"}
# The benchmarks of `haylen.py bench` that are plain executables on the CPU, by suite.
CPU_BENCHMARKS = {"core": "haylen-core-benchmark", "algorithms": "haylen-algorithm-benchmark", "procedural": "haylen-procedural-benchmark", "physics": "haylen-physics-benchmark", "ui": "haylen-ui-benchmark", "render": "haylen-render-benchmark", "particles": "haylen-particle-benchmark", "content": "haylen-content-benchmark", "nodes": "haylen-node-benchmark", "animation": "haylen-animation-benchmark"}

# The slices of `Haylen.xcframework` and the architectures each one joins with lipo.
APPLE_SLICES = {
    "macos": ["arm64", "x86_64"],
    "ios": ["arm64"],
    "ios-simulator": ["arm64", "x86_64"],
    "ios-maccatalyst": ["arm64", "x86_64"],
    "tvos": ["arm64"],
    "tvos-simulator": ["arm64", "x86_64"],
}

# How each Apple run platform builds the Apple project: its scheme, the xcodebuild destination or the simulator family it boots, the suffix of the folder of its products, its target template and the platform of its engine frameworks.
APPLE_RUNS = {
    "macos": {"scheme": "macOS", "destination": "platform=macOS", "products": "", "template": "HaylenMacOS", "frameworks": "macOS"},
    "catalyst": {"scheme": "iOS", "destination": "platform=macOS,variant=Mac Catalyst", "products": "-maccatalyst", "template": "HaylenIOS", "frameworks": "macCatalyst"},
    "ios": {"scheme": "iOS", "destination": "generic/platform=iOS", "products": "-iphoneos", "template": "HaylenIOS", "frameworks": "iOS"},
    "ios-simulator": {"scheme": "iOS", "simulator": "iOS", "products": "-iphonesimulator", "template": "HaylenIOS", "frameworks": "iOS"},
    "tvos": {"scheme": "tvOS", "destination": "generic/platform=tvOS", "products": "-appletvos", "template": "HaylenTVOS", "frameworks": "tvOS"},
    "tvos-simulator": {"scheme": "tvOS", "simulator": "tvOS", "products": "-appletvsimulator", "template": "HaylenTVOS", "frameworks": "tvOS"},
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
    """A failure that haylen.py expects, which stops the command with its message, and the output of the tool that failed as its details, instead of a traceback."""

    def __init__(self, message: str, details: str = "") -> None:
        super().__init__(message)
        self.details = details


class Terminal:
    """Prints what haylen.py does: the titles of its steps, the commands it runs, successes, warnings and errors. A message marks reserved expressions with double quotes and paths and URLs with backticks. On a terminal with color, reserved expressions print in color without their quotes and paths and URLs print underlined, and without color reserved expressions keep their quotes. Paths and URLs always print bare, so terminals can open them."""

    MARKUP = re.compile(r'`([^`\n]+)`|"([^"\n]+)"')
    STEP = "1"
    COMMAND = "2"
    SUCCESS = "32"
    WARNING = "1;33"
    ERROR = "1;31"
    EMPHASIS = "1"
    EXPRESSION = "36"
    LOCATION = "4"
    # The console mode of Windows that shows escape sequences, which consoles leave off for older programs.
    VIRTUAL_TERMINAL_PROCESSING = 0x0004

    def __init__(self, stdout: TextIO, stderr: TextIO, environment: Mapping[str, str]) -> None:
        self.stdout = stdout
        self.stderr = stderr
        self.stdout_color = Terminal.shows_color(stdout, environment)
        self.stderr_color = Terminal.shows_color(stderr, environment)

    @staticmethod
    def shows_color(stream: TextIO, environment: Mapping[str, str]) -> bool:
        """Tells whether a stream prints color: never with `NO_COLOR`, always with `FORCE_COLOR`, and otherwise on a terminal that is not `dumb`."""
        if environment.get("NO_COLOR"):
            return False
        terminal = stream.isatty() and Terminal.enable_escapes(stream)
        return bool(environment.get("FORCE_COLOR")) or (terminal and environment.get("TERM") != "dumb")

    @staticmethod
    def enable_escapes(stream: TextIO) -> bool:
        """Turns on escape sequences in a Windows console and tells whether the terminal of a stream shows them, which every other terminal does."""
        if os.name != "nt":
            return True
        import ctypes
        import msvcrt

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = msvcrt.get_osfhandle(stream.fileno())
        mode = ctypes.c_uint32()
        return bool(kernel32.GetConsoleMode(handle, ctypes.byref(mode))) and bool(kernel32.SetConsoleMode(handle, mode.value | Terminal.VIRTUAL_TERMINAL_PROCESSING))

    @staticmethod
    def paint(text: str, color: bool, style: str) -> str:
        return f"\x1b[{style}m{text}\x1b[0m" if color and style else text

    @staticmethod
    def render(message: str, color: bool, style: str = "") -> str:
        """Turns the marks of a message into color within the style of the line, or without color keeps the quotes of reserved expressions and drops the backticks of paths and URLs."""
        if not color:
            return Terminal.MARKUP.sub(lambda match: match[0] if match[1] is None else match[1], message)
        base = f"\x1b[{style}m" if style else ""

        def mark(match: re.Match) -> str:
            code, text = (Terminal.EXPRESSION, match[2]) if match[1] is None else (Terminal.LOCATION, match[1])
            return f"\x1b[{code}m{text}\x1b[0m{base}"

        rendered = Terminal.MARKUP.sub(mark, message)
        return f"{base}{rendered}\x1b[0m" if base else rendered

    @staticmethod
    def command_line(parts: list[str]) -> str:
        return subprocess.list2cmdline(parts) if os.name == "nt" else shlex.join(parts)

    def step(self, title: str) -> None:
        print(Terminal.render(f"==> {title}", self.stdout_color, Terminal.STEP), file=self.stdout, flush=True)

    def command(self, parts: list[str]) -> None:
        print(Terminal.paint(f"$ {Terminal.command_line(parts)}", self.stdout_color, Terminal.COMMAND), file=self.stdout, flush=True)

    def info(self, message: str) -> None:
        print(Terminal.render(message, self.stdout_color), file=self.stdout, flush=True)

    def success(self, message: str) -> None:
        print(Terminal.render(message, self.stdout_color, Terminal.SUCCESS), file=self.stdout, flush=True)

    def warning(self, message: str, *notes: str) -> None:
        """Prints a warning with the notes that tell what to do about it, each on a line of its own."""
        lines = [f"{Terminal.paint('Warning:', self.stderr_color, Terminal.WARNING)} {Terminal.render(message, self.stderr_color)}"]
        lines += [f"  {Terminal.render(note, self.stderr_color)}" for note in notes]
        print("\n".join(lines), file=self.stderr, flush=True)

    def error(self, message: str, details: str = "") -> None:
        """Prints an error with its cause and what to do in one block, followed by the output of the tool that failed, as it is."""
        lines = [f"{Terminal.paint('Error:', self.stderr_color, Terminal.ERROR)} {Terminal.render(message, self.stderr_color, Terminal.EMPHASIS)}"]
        if details:
            lines.append(Terminal.paint(details, self.stderr_color, Terminal.COMMAND))
        print("\n".join(lines), file=self.stderr, flush=True)

    def verbatim(self, text: str, error: bool = False) -> None:
        """Prints text that must stay as it is, such as a snippet to copy or a diff."""
        print(text, file=self.stderr if error else self.stdout, flush=True)


terminal = Terminal(sys.stdout, sys.stderr, os.environ)


def shown_path(path: Path) -> str:
    """Names a path relative to the current folder when it lies inside it, so messages stay short and terminals still open it."""
    return str(path.relative_to(Path.cwd())) if path.is_relative_to(Path.cwd()) else str(path)


# The command that runs haylen.py from the current folder, which the messages that suggest a command start with.
TOOL = "haylen"
APP_FOLDER = f'An app folder holds "app.json", its Lua modules under "source" and its assets under "content", and "{TOOL} new <folder>" creates one.'


def missing_command(executable: str) -> str:
    return f'The command "{Path(executable).name}" was not found. Install it, or add its folder to "PATH".'


def run(command: list, *, cwd: Path = ROOT, env: dict[str, str] | None = None, echo: bool = True) -> None:
    """Runs a command with its output in the terminal, after printing the command unless the caller describes it in a step of its own, and stops with an error that names the command when it fails."""
    parts = [str(part) for part in command]
    if echo:
        terminal.command(parts)
    try:
        subprocess.run(parts, cwd=cwd, env=env, check=True)
    except FileNotFoundError as error:
        raise BuildError(missing_command(parts[0])) from error
    except subprocess.CalledProcessError as error:
        name = Path(parts[0]).name
        if error.returncode < 0:
            raise BuildError(f'The command "{name}" was stopped by the signal {-error.returncode}.') from error
        raise BuildError(f'The command "{name}" failed with exit code {error.returncode}, and its output above shows why.') from error


def capture(command: list, *, cwd: Path = ROOT) -> str:
    """Runs a command quietly and returns its standard output, and stops with its error output when it fails."""
    parts = [str(part) for part in command]
    try:
        return subprocess.run(parts, cwd=cwd, check=True, capture_output=True, text=True).stdout
    except FileNotFoundError as error:
        raise BuildError(missing_command(parts[0])) from error
    except subprocess.CalledProcessError as error:
        raise BuildError(f'The command "{Path(parts[0]).name}" failed with exit code {error.returncode}.', (error.stderr or error.stdout).strip()) from error


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
    return (ENGINE_DIR / "VERSION").read_text(encoding="utf-8").strip()


def download(url: str, target: Path, sha256: str | None = None) -> None:
    """Downloads a file and, when a hash is pinned, deletes the download and fails unless its SHA-256 matches."""
    target.parent.mkdir(parents=True, exist_ok=True)
    terminal.step(f"Downloading `{url}`")
    urllib.request.urlretrieve(url, target)
    if sha256 and hashlib.sha256(target.read_bytes()).hexdigest() != sha256:
        target.unlink()
        raise BuildError(f'The download of `{url}` does not match its pinned SHA-256 "{sha256}", so it was deleted. Try again, and if it still differs, the file on the server changed.')


def ensure_shdc() -> Path:
    """Returns the pinned sokol-shdc, kept in a folder named after its commit, so a new pin downloads its own binary. The binary moves into place only once it is complete and executable, since tests that compile shaders may ask for it at the same time."""
    executable = executable_name("sokol-shdc")
    target = TOOLS_DIR / f"sokol-shdc-{SHDC_COMMIT[:7]}" / executable
    if target.exists():
        return target

    folders = {("macos", "arm64"): "osx_arm64", ("macos", "x64"): "osx", ("linux", "arm64"): "linux_arm64", ("linux", "x64"): "linux", ("windows", "x64"): "win32"}
    folder = folders.get((host_name(), host_arch()))
    if folder is None:
        raise BuildError(f'The shader compiler "sokol-shdc" has no prebuilt binary for "{host_name()}" on "{host_arch()}". Use a host with a prebuilt binary, as the build guide lists.')

    partial = target.with_name(f"{executable}.{os.getpid()}.part")
    download(f"https://raw.githubusercontent.com/floooh/sokol-tools-bin/{SHDC_COMMIT}/bin/{folder}/{executable}", partial)
    partial.chmod(0o755)
    partial.replace(target)
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

    terminal.step(f"Installing Emscripten {EMSDK_VERSION} into `{shown_path(root)}`")
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
    raise BuildError(f"The Android NDK {ANDROID_NDK_VERSION} was not found in `{android_sdk() / 'ndk'}`. Install it with \"sdkmanager 'ndk;{ANDROID_NDK_VERSION}'\".")


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
        raise BuildError(f'Builds for "{platform_name}" need a "{required}" host, and this one is "{host_name()}".')


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
    tools = {"sokol-shdc": ensure_shdc}
    if host_name() == "macos":
        tools["XcodeGen"] = ensure_xcodegen
    if args.emsdk:
        tools["Emscripten"] = ensure_emsdk
    if args.gradle:
        tools["Gradle"] = ensure_gradle
    for name, ensure in tools.items():
        terminal.success(f'The tool "{name}" is ready at `{shown_path(ensure())}`.')


def command_configure(args: argparse.Namespace) -> None:
    command, env = configure_command(args)
    terminal.step(f'Configuring the "{args.platform}" build in `{shown_path(build_dir(args.platform, args.config, args.sanitizers))}`')
    run(command, env=env)


def cmake_cache_value(directory: Path, name: str) -> str:
    prefix = f"{name}:"
    for line in (directory / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            return line.split("=", 1)[1]
    raise BuildError(f'The variable "{name}" is missing from the CMake cache of `{shown_path(directory)}`. Configure the build tree again with "{TOOL} configure".')


def ensure_configured(args: argparse.Namespace) -> Path:
    """Configures the build tree when it does not exist yet, when the requested backend differs from the one it was configured with or when the pinned shader compiler changed."""
    directory = build_dir(args.platform, args.config, args.sanitizers)
    if not (directory / "CMakeCache.txt").exists() or cmake_cache_value(directory, "HAYLEN_RENDER_BACKEND") != requested_backend(args) or Path(cmake_cache_value(directory, "HAYLEN_SOKOL_SHDC")) != ensure_shdc():
        command_configure(args)
    return directory


def command_build(args: argparse.Namespace) -> None:
    directory = ensure_configured(args)
    command = ["cmake", "--build", directory, "--config", args.config, "--parallel", str(args.jobs)]
    if args.target:
        command += ["--target", args.target]
    built = f'the target "{args.target}"' if args.target else "the engine"
    terminal.step(f'Building {built} for "{args.platform}" in `{shown_path(directory)}`')
    run(command)


def command_test(args: argparse.Namespace) -> None:
    args.platform = host_name()
    args.target = "haylen_tests"
    command_build(args)
    terminal.step("Running the engine tests")
    run(["ctest", "--test-dir", build_dir(args.platform, args.config, args.sanitizers), "-C", args.config, "--output-on-failure", "--parallel", str(args.jobs)])
    terminal.success("The engine tests passed.")


def llvm_tool(name: str) -> str:
    if host_name() == "macos":
        return capture(["xcrun", "--find", name]).strip()
    tool = shutil.which(name)
    if tool is None:
        raise BuildError(f'The LLVM tool "{name}" was not found. Install LLVM and add its folder to "PATH" to measure coverage.')
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
    terminal.step(f"Building the engine tests with coverage in `{shown_path(directory)}`")
    run(command, env=env)
    run(["cmake", "--build", directory, "--target", "haylen_tests", "--parallel", str(args.jobs)])

    profiles = directory / "coverage"
    shutil.rmtree(profiles, ignore_errors=True)
    terminal.step("Running the engine tests")
    run(["ctest", "--test-dir", directory, "--output-on-failure", "--parallel", str(args.jobs)])

    raw = sorted(profiles.glob("*.profraw"))
    if not raw:
        raise BuildError(f"The test run wrote no coverage profiles into `{shown_path(profiles)}`. Measure coverage with Clang, which the guide of the build describes.")
    terminal.step("Writing the coverage report")
    merged = profiles / "haylen.profdata"
    run([llvm_tool("llvm-profdata"), "merge", "-sparse", *raw, "-o", merged])

    binary = directory / "bin" / "haylen_tests"
    ignore = r"(_deps|\.cache|engine/tests|engine/src/platform/(apple|android|web|windows|linux|sokol)|generated)"
    report = [llvm_tool("llvm-cov"), "report", binary, f"-instr-profile={merged}", f"-ignore-filename-regex={ignore}"]
    run(report)
    run([llvm_tool("llvm-cov"), "show", binary, f"-instr-profile={merged}", f"-ignore-filename-regex={ignore}", "-format=html", f"-output-dir={profiles / 'html'}"])
    terminal.success(f"The coverage report is in `{shown_path(profiles / 'html' / 'index.html')}`.")


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
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            stripped = line.strip()
            if stripped == "// clang-format off":
                guarded = True
            elif stripped == "// clang-format on":
                guarded = False
            elif not guarded and LAMBDA_START.search(stripped) and not stripped.startswith(("//", "[[", "#")):
                findings.append(f'The file `{shown_path(path)}:{number}` has a multi-line lambda outside "clang-format off".')
    return findings


def command_format(args: argparse.Namespace) -> None:
    clang_format = shutil.which("clang-format")
    if clang_format is None:
        raise BuildError(missing_command("clang-format"))

    files = format_sources()
    command = [clang_format, "--style=file"] + (["--dry-run", "--Werror"] if args.check else ["-i"])
    terminal.step(f"{'Checking the format of' if args.check else 'Formatting'} {len(files)} files with \"clang-format\"")
    failed = False
    # The files go in groups, which keeps every command line within the limits of the system, and the command lines stay unprinted, since they list every file.
    for start in range(0, len(files), 200):
        try:
            run(command + files[start : start + 200], echo=False)
        except BuildError:
            failed = True

    findings = unguarded_lambdas(files)
    for finding in findings:
        terminal.warning(finding)
    if failed and args.check:
        raise BuildError(f'Some files are not formatted, as the output of "clang-format" above shows. Format them with "{TOOL} format".')
    if failed:
        raise BuildError('The command "clang-format" could not format some files, and its output above shows why.')
    if findings and args.check:
        raise BuildError(f'The {len(findings)} multi-line lambdas above need "// clang-format off" and "// clang-format on" markers around them.')
    terminal.success(f"The {len(files)} files are formatted." if args.check else f"Formatted {len(files)} files.")


def command_bench(args: argparse.Namespace) -> None:
    """Builds a benchmark in Release and runs it on this machine: sprites on the GPU, the frame loop, events, owners, timers, tweens, jobs and asset loading on the CPU, path finding, crowds, spatial queries and ray casts on the CPU, procedural generation, geometry and destruction on the CPU, physics worlds on the CPU, long UI lists on the CPU, frames of sprites, shapes, text and interface components on the CPU, particle emitters on the CPU, protected content on the CPU and the disk, or the Lua sprite benchmark on the CPU."""
    if args.suite in CPU_BENCHMARKS:
        build = build_options(host_name(), "Release", CPU_BENCHMARKS[args.suite], args.jobs)
        command_build(build)
        terminal.step(f'Running the "{args.suite}" benchmark')
        run([build_dir(build.platform, build.config) / "bin" / executable_name(build.target)])
        return

    if args.suite == "lua":
        build = build_options(host_name(), "Release", "haylen-lua-benchmark", args.jobs)
        command_build(build)
        terminal.step('Running the "lua" benchmark')
        run([build_dir(build.platform, build.config) / "bin" / executable_name(build.target), ENGINE_DIR / "bench" / "lua-benchmark"])
        return

    build = build_options(host_name(), "Release", "haylen-sprite-benchmark", args.jobs)
    command_build(build)
    terminal.step('Running the "sprites" benchmark')
    run([cmake_app_executable(build_dir(build.platform, build.config), build.target)])


def sdk_dir(platform_name: str, config: str) -> Path:
    return BUILD_ROOT / "sdk" / f"haylen-{platform_name}-{config.lower()}"


def consumer_modes(args: argparse.Namespace) -> list[str]:
    """Returns the ways of adding the engine that `sdk --check-consumers` builds a consumer project with: the ones it lists, or every one when it lists none, and none without the option."""
    if args.check_consumers is None:
        return []
    if args.platform != host_name():
        raise BuildError(f'The option "--check-consumers" builds consumer projects for this machine, "{host_name()}", so it cannot follow "--platform {args.platform}".')
    return args.check_consumers or list(CONSUMER_MODES)


def command_sdk(args: argparse.Namespace) -> None:
    """Builds the engine SDK for a platform, installs it where `find_package(haylen)` finds it and builds a consumer project in every way that `--check-consumers` asks for."""
    modes = consumer_modes(args)
    options = build_options(args.platform, args.config, jobs=args.jobs)
    directory = BUILD_ROOT / f"sdk-build-{args.platform}-{args.config.lower()}"
    command, env = configure_command(options)
    command[command.index("-S") + 1] = str(ENGINE_DIR)
    command[command.index("-B") + 1] = str(directory)
    command += ["-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_PLAYER=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF"]
    terminal.step(f'Building the SDK of the engine for "{args.platform}" in the "{args.config}" configuration')
    run(command, env=env)
    run(["cmake", "--build", directory, "--config", args.config, "--target", "haylen_sdk", "--parallel", str(args.jobs)])
    prefix = Path(args.output).resolve() if args.output else sdk_dir(args.platform, args.config)
    run(["cmake", "--install", directory, "--config", args.config, "--component", "haylen_sdk", "--prefix", prefix])
    terminal.success(f"Installed the SDK into `{shown_path(prefix)}`.")
    for mode in modes:
        check_consumer(mode, args.config, args.jobs, prefix)


def check_consumer(mode: str, config: str, jobs: int, sdk: Path) -> None:
    """Builds the consumer project of the engine tests, which adds the engine through `add_subdirectory`, CPM or the installed SDK like another project does, and compiles a C++ app against its public headers."""
    directory = BUILD_ROOT / "consumers" / f"{mode}-{config.lower()}"
    # The project shares the dependency cache of the build trees of the engine, instead of downloading every dependency into its own folder.
    cache = os.environ.get("CPM_SOURCE_CACHE", str(ROOT / ".cache" / "cpm"))
    command = ["cmake", "-S", CONSUMER_PROJECT, "-B", directory, f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", f"-DHAYLEN_CONSUMER_MODE={mode}", f"-DCPM_SOURCE_CACHE={cache}"]
    if host_name() != "windows" or shutil.which("ninja"):
        command += ["-G", "Ninja"]
    if host_name() == "macos":
        command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}")
    if mode == "package":
        command.append(f"-DCMAKE_PREFIX_PATH={sdk}")
    terminal.step(f"Building a project that adds the engine through {CONSUMER_MODES[mode]}")
    run(command)
    run(["cmake", "--build", directory, "--config", config, "--parallel", str(jobs)])
    terminal.success(f"A project adds the engine through {CONSUMER_MODES[mode]}.")


def cmake_app_executable(directory: Path, target: str) -> Path:
    folder = directory / "bin" / target
    if host_name() == "macos":
        return folder / f"{target}.app" / "Contents" / "MacOS" / target
    return folder / executable_name(target)


# Engine artifacts: the prebuilt engine that apps made from the templates use, so an app never compiles the engine.


def engine_sources_hash() -> str:
    """Hashes every engine file that reaches an artifact, so `haylen.py` knows when the artifacts are stale."""
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
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {"platforms": {}}


def write_manifest(platform_name: str, config: str, sources: str) -> None:
    manifest = read_manifest()
    manifest["version"] = engine_version()
    manifest["platforms"][platform_name] = {"config": config, "sources": sources}
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)
    (ARTIFACTS_DIR / "manifest.json").write_text(json.dumps(manifest, indent=4) + "\n", encoding="utf-8")


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
    # Apps link the system frameworks of the engine and declare its privacy reasons from the files next to the framework.
    shutil.copy2(headers.parent.parent / "framework" / "share" / "haylen" / "haylen-frameworks.json", output.parent)
    shutil.copy2(ENGINE_DIR / "platform" / "apple" / "PrivacyInfo.xcprivacy", output.parent)
    shutil.copy2(ENGINE_DIR / "platform" / "apple" / APPLE_SWIFT_HELPERS, output.parent)


def android_engine_sdk(abi: str) -> Path:
    """The SDK of the engine for one Android ABI, which links the release libraries of apps."""
    return ARTIFACTS_DIR / "android" / "sdk" / abi


def build_android_players(config: str, jobs: int) -> Path:
    """Builds `libhaylen.so`, the Lua player, and the SDK of the engine, for one ABI after the other with every job, so the configures never write into the shared CPM sources together and the compilers stay within the jobs, gathers the libraries in the `jniLibs` layout and installs each SDK into the artifacts."""
    libraries = ENGINE_BUILDS_DIR / f"android-{config.lower()}" / "jniLibs"
    shutil.rmtree(libraries, ignore_errors=True)
    for abi in ANDROID_ABIS:
        directory = ENGINE_BUILDS_DIR / f"android-{abi}-{config.lower()}"
        run(["cmake", "-S", ENGINE_DIR, "-B", directory, "-G", "Ninja", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", "-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", *android_options(abi)])
        run(["cmake", "--build", directory, "--target", "haylen", "haylen_sdk", "--parallel", str(jobs)])
        copy_into(directory / "lib" / "libhaylen.so", libraries / abi)
        shutil.rmtree(android_engine_sdk(abi), ignore_errors=True)
        run(["cmake", "--install", directory, "--component", "haylen_sdk", "--prefix", android_engine_sdk(abi)])
    return libraries


def build_android_artifacts(config: str, jobs: int) -> None:
    """Packages the players of every ABI with the Java side into the `haylen` Android library, together with the Kotlin transport of Varn from where CPM placed it for the native build, and publishes it with the libraries that only some apps add, `haylen-plugins`, `haylen-links` and `haylen-coroutines`, to the local Maven repository of the artifacts."""
    libraries = build_android_players(config, jobs)
    varn = cmake_cache_value(ENGINE_BUILDS_DIR / f"android-{ANDROID_ABIS[0]}-{config.lower()}", "varn_SOURCE_DIR")
    maven = ARTIFACTS_DIR / "android" / "maven"
    shutil.rmtree(maven, ignore_errors=True)
    tasks = [f":{module}:publishReleasePublicationToArtifactsRepository" for module in ANDROID_LIBRARIES]
    run([ensure_gradle(), "-p", ANDROID_LIBRARY_PROJECT, *tasks, f"--max-workers={jobs}", f"-PhaylenNativeLibraries={libraries}", f"-PhaylenVarnSourceDir={varn}", f"-PhaylenMavenDir={maven}"])


def web_sdk(backend: str) -> Path:
    """The SDK of the engine for the web with one backend, which links the release runtime of each app."""
    return ARTIFACTS_DIR / "web" / f"sdk-{backend}"


def build_web_artifacts(config: str, jobs: int) -> None:
    """Builds the player for WebGPU and for WebGL2, each next to the processor of its audio output and the worker of its content store, and the SDK of each backend. The player loads the app package at runtime, so one build serves every app in development, and a release links a runtime of its own against the SDK."""
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        command_build(build_options(platform_name, config, "haylen", jobs))
        directory = build_dir(platform_name, config)
        built = directory / "bin" / "haylen"
        destination = ARTIFACTS_DIR / "web" / backend
        shutil.rmtree(destination, ignore_errors=True)
        destination.mkdir(parents=True)
        for name in WEB_RUNTIME_FILES:
            shutil.copy2(built / name, destination)
        run(["cmake", "-B", directory, "-DHAYLEN_BUILD_SDK=ON"])
        run(["cmake", "--build", directory, "--target", "haylen_sdk", "--parallel", str(jobs)])
        shutil.rmtree(web_sdk(backend), ignore_errors=True)
        run(["cmake", "--install", directory, "--config", config, "--component", "haylen_sdk", "--prefix", web_sdk(backend)])


def desktop_artifact(name: str = "haylen") -> Path:
    return ARTIFACTS_DIR / "desktop" / f"{host_name()}-{host_arch()}" / executable_name(name)


def desktop_sdk() -> Path:
    """The SDK of this machine, which links the release executables of Windows and Linux apps."""
    return desktop_artifact().parent / "sdk"


def cmake_generator() -> list[str]:
    """Ninja, or the default generator on Windows without Ninja."""
    return ["-G", "Ninja"] if host_name() != "windows" or shutil.which("ninja") else []


def build_desktop_artifacts(config: str, jobs: int) -> None:
    """Builds the player, the content tool and the SDK of this machine in one build tree of the engine, and installs the SDK next to them."""
    directory = ENGINE_BUILDS_DIR / f"desktop-{config.lower()}"
    command = ["cmake", "-S", ENGINE_DIR, "-B", directory, *cmake_generator(), f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCMAKE_BUILD_TYPE={config}", "-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_TOOLS=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF"]
    if host_name() == "macos":
        command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}")
    run(command)
    run(["cmake", "--build", directory, "--config", config, "--target", "haylen", CONTENT_TOOL, "haylen_sdk", "--parallel", str(jobs)])
    destination = desktop_artifact().parent
    shutil.rmtree(destination, ignore_errors=True)
    destination.mkdir(parents=True)
    shutil.copy2(directory / "bin" / "haylen" / executable_name("haylen"), desktop_artifact())
    shutil.copy2(directory / "bin" / executable_name(CONTENT_TOOL), desktop_artifact(CONTENT_TOOL))
    run(["cmake", "--install", directory, "--config", config, "--component", "haylen_sdk", "--prefix", desktop_sdk()])


ARTIFACT_BUILDERS = {"apple": build_apple_artifacts, "android": build_android_artifacts, "web": build_web_artifacts, "desktop": build_desktop_artifacts}


def build_artifacts(platform_name: str, config: str, jobs: int) -> None:
    terminal.step(f'Building the "{platform_name}" artifacts of Haylen {engine_version()} in the "{config}" configuration')
    sources = engine_sources_hash()
    ARTIFACT_BUILDERS[platform_name](config, jobs)
    write_manifest(platform_name, config, sources)
    terminal.success(f'The "{platform_name}" artifacts of Haylen {engine_version()} are in `{shown_path(ARTIFACTS_DIR / platform_name)}`.')


def ensure_artifacts(platform_name: str, config: str, jobs: int) -> None:
    """Builds the artifacts of a platform when they are missing, were built with another configuration or are older than the engine sources."""
    entry = read_manifest()["platforms"].get(platform_name)
    if entry == {"config": config, "sources": engine_sources_hash()}:
        return
    terminal.info(f'The "{platform_name}" artifacts of the engine are missing or older than its sources, so they build first.')
    build_artifacts(platform_name, config, jobs)


def command_engine(args: argparse.Namespace) -> None:
    for platform_name in ARTIFACT_PLATFORMS if args.platform == "all" else [args.platform]:
        build_artifacts(platform_name, args.config, args.jobs)


# Apps: a package folder assembled with a platform template into a project under `build/apps`, then built and launched.


def resolve_app(value: str | None) -> Path:
    """Returns the folder of an app that a command names by its path, relative to the current folder or absolute, and stops with what an app folder is when the path names none."""
    if not value:
        raise BuildError(f"The command needs the folder of an app, named after it. {APP_FOLDER}")
    folder = Path(value).expanduser().resolve()
    if not folder.exists():
        raise BuildError(f"The path `{shown_path(folder)}` does not exist. {APP_FOLDER}")
    if not (folder / "app.json").is_file():
        raise BuildError(f'The path `{shown_path(folder)}` is not an app folder, because it holds no "app.json". {APP_FOLDER}')
    return folder


def resolve_lua_app(value: str | None) -> Path:
    """Returns the folder of a Lua app, the kind of app that the commands of the platform projects build, and stops with the command of C++ apps for the folder of a C++ app."""
    folder = resolve_app(value)
    if app_kind(folder) == "cpp":
        raise BuildError(f'The app `{shown_path(folder)}` is a C++ app, whose CMake project builds it for every platform. Run it with "{TOOL} run-cpp {shown_path(folder)}".')
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
        where = shown_path(folder / "app.json")
        document = json.loads((folder / "app.json").read_text(encoding="utf-8"))
        for key in ("name", "identifier", "version"):
            if not isinstance(document.get(key), str) or not document[key]:
                raise BuildError(f'The file `{where}` needs a "{key}" to build an app.')
        self.name: str = document["name"]
        self.identifier: str = document["identifier"]
        self.version: str = document["version"]
        self.orientation: str = document.get("orientation", "landscape")
        if self.orientation not in ANDROID_ORIENTATIONS:
            raise BuildError(f'The "orientation" of `{where}` must be "landscape", "portrait" or "any".')
        if not re.fullmatch(r"\d+(\.\d+){0,2}", self.version):
            raise BuildError(f'The "version" of `{where}` must be one to three numbers separated by dots, such as "1.2.0".')
        # The publisher of the app follows its name until it gives its own, as the engine reads it.
        self.description: str = document.get("description", self.name)
        self.company: str = document.get("company", self.name)
        self.copyright: str = document.get("copyright", f"Copyright \u00a9 {self.company}")
        self.website: str = document.get("website", "")

        # A transparent window clears to transparent, and an app without a taskbar button also leaves out its Dock icon on macOS.
        window = document.get("window", {})
        self.transparent: bool = window.get("transparent", False)
        self.show_in_taskbar: bool = window.get("showInTaskbar", True)
        splash = document.get("splash", {})
        self.background = parse_color(splash.get("background", document.get("clearColor", "#00000000" if self.transparent else "#FF000000")))
        self.dark_background = parse_color(splash["darkBackground"]) if "darkBackground" in splash else None
        self.splash_logo: Path | None = None
        if splash.get("logo"):
            self.splash_logo = folder / "content" / splash["logo"]
            if not self.splash_logo.is_file():
                raise BuildError(f'The splash logo `{shown_path(self.splash_logo)}` that `{where}` names does not exist. Give "splash.logo" the path of an image relative to "content".')

        # An app with a content server ships only the content it needs to start, and the server delivers the rest.
        content = document.get("content", {})
        self.content_server: str | None = content.get("updates", {}).get("baseUrl") if isinstance(content, dict) and isinstance(content.get("updates"), dict) else None
        rules = content.get("delivery", []) if isinstance(content, dict) else []
        self.content_packs: list[str] = sorted({rule["pack"] for rule in rules if isinstance(rule, dict) and isinstance(rule.get("pack"), str)})

        native = document.get("native", {})
        if not isinstance(native, dict):
            raise BuildError(f'The "native" section of `{where}` must map library names to their files or CMake projects.')
        self.native = [NativeLibrary.parse(folder, name, entry, folder / "app.json") for name, entry in native.items()]

        # The native library of a plugin joins the ones of `app.json`, so every platform builds and places it the same way.
        self.plugins, self.plugin_values = load_app_plugins(folder, document.get("plugins", {}), platform)
        for plugin in self.plugins:
            if plugin.native is None:
                continue
            if any(library.name == plugin.native.name for library in self.native):
                raise BuildError(f'The plugin "{plugin.id}" adds the native library "{plugin.native.name}", which `{where}` or another plugin already names. Rename one of the libraries.')
            self.native.append(plugin.native)

    @property
    def executable(self) -> str:
        """The name of the executable and the packages of desktop apps: the app folder in lowercase dash-case, which every package format accepts."""
        name = re.sub(r"[^a-z0-9]+", "-", self.folder.name.lower()).strip("-")
        if not name:
            raise BuildError(f'The app folder name "{self.folder.name}" gives no executable name. Rename the folder with letters or digits.')
        return name

    @property
    def build_folder(self) -> Path:
        """The folder under `build/apps` that holds everything `haylen.py` builds for the app, which no other app shares."""
        return APPS_DIR / build_folder_name(self.folder)

    @property
    def version_code(self) -> int:
        """Android needs an integer that grows with every version, so `1.2.3` becomes `1002003`."""
        parts = [int(part) for part in self.version.split(".")] + [0, 0]
        return parts[0] * 1_000_000 + parts[1] * 1_000 + parts[2]


def package_files(folder: Path) -> list[Path]:
    """Lists the files of the package of an app folder: `app.json`, `source`, `content` and, for every plugin that `app.json` lists, its `plugin.json` and its `source` folder."""
    plugins = json.loads((folder / "app.json").read_text(encoding="utf-8")).get("plugins", {})
    if not isinstance(plugins, dict):
        raise BuildError(f'The "plugins" section of `{shown_path(folder / "app.json")}` must map plugin ids to objects of parameter values.')

    roots = [folder / name for name in PACKAGE_FOLDERS]
    manifests = []
    for identifier in plugins:
        manifest = folder / "plugins" / identifier / "plugin.json"
        if not manifest.is_file():
            raise BuildError(f'The file `{shown_path(folder / "app.json")}` lists the plugin "{identifier}", whose `{shown_path(manifest)}` does not exist. Add the plugin with "{TOOL} plugin add <plugin folder or repository> --app {shown_path(folder)}".')
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
    files = package_files(folder)
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.relative_to(folder).as_posix())


# Native libraries: what the `native` section of `app.json` lists, prebuilt or built from a CMake project, placed where each platform package loads it.

NATIVE_PLATFORMS = ("macos", "ios", "tvos", "android", "windows", "linux")
# The slice of `APPLE_SLICES` that each Apple run platform builds, the target template and platform whose embed phase ships its libraries, and the SDK of the setting that links its static libraries.
APPLE_NATIVE_SLICES = {"macos": "macos", "catalyst": "ios-maccatalyst", "ios": "ios", "ios-simulator": "ios-simulator", "tvos": "tvos", "tvos-simulator": "tvos-simulator"}
APPLE_NATIVE_KEYS = {"macos": "macos-macosx", "ios-maccatalyst": "ios-macosx", "ios": "ios-iphoneos", "ios-simulator": "ios-iphonesimulator", "tvos": "tvos-appletvos", "tvos-simulator": "tvos-appletvsimulator"}
APPLE_STATIC_SDKS = {"ios-maccatalyst": "macosx", "ios": "iphoneos", "ios-simulator": "iphonesimulator", "tvos": "appletvos", "tvos-simulator": "appletvsimulator"}
# The condition of each platform whose apps may link native libraries statically, which keeps their symbols in the table of the app. The iOS one holds for Mac Catalyst too.
APPLE_STATIC_CONDITIONS = {"ios": "TARGET_OS_IOS", "tvos": "TARGET_OS_TV"}
XCFRAMEWORK_SLICES = {"macos": ("macos", None), "ios-maccatalyst": ("ios", "maccatalyst"), "ios": ("ios", None), "ios-simulator": ("ios", "simulator"), "tvos": ("tvos", None), "tvos-simulator": ("tvos", "simulator")}
FRAMEWORK_PLATFORMS = {"ios": "iPhoneOS", "ios-simulator": "iPhoneSimulator", "tvos": "AppleTVOS", "tvos-simulator": "AppleTVSimulator"}


@dataclasses.dataclass(frozen=True)
class NativeLibrary:
    """A native library of an app: prebuilt files per platform, or a target of a CMake project that `haylen.py` builds for each listed platform. On iOS and tvOS, apps may link it statically, and then the symbols that Lua reaches are listed."""

    name: str
    files: dict[str, Path]
    cmake: Path | None
    platforms: tuple[str, ...]
    static: bool
    symbols: tuple[str, ...]

    @staticmethod
    def parse(folder: Path, name: str, entry: object, source: Path) -> "NativeLibrary":
        """Reads an entry of the `native` section of `app.json`, or the `native` section of a plugin, whose paths are relative to the folder of the file."""
        where = f'The native library "{name}" in `{shown_path(source)}`'
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
                raise BuildError(f"{where} lists `{shown_path(path)}`, which does not exist.")
        cmake = folder / entry["cmake"] if "cmake" in entry else None
        if cmake is not None and not (cmake / "CMakeLists.txt").is_file():
            raise BuildError(f'{where} names the CMake project `{shown_path(cmake)}`, which has no "CMakeLists.txt".')

        link = entry.get("link", "dynamic")
        symbols = tuple(entry.get("symbols", ()))
        if link not in ("dynamic", "static"):
            raise BuildError(f'{where} links "dynamic" or "static", not "{link}".')
        if link == "static" and (not set(platforms) <= {"ios", "tvos"} or not symbols):
            raise BuildError(f"{where} links statically, which iOS, Mac Catalyst and tvOS apps do, and lists the symbols that Lua calls.")
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
    terminal.step(f'Building the native library "{library.name}" in `{shown_path(directory)}`')
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
    raise BuildError(f'The xcframework `{shown_path(path)}` has no slice for "{slice_name}". Add that slice to the xcframework, or leave the platform out of the "platforms" of the library.')


def copy_into(source: Path, folder: Path) -> Path:
    """Copies a file or a bundle, such as a library, a framework or a resource bundle, into a folder, keeping the links inside bundles."""
    folder.mkdir(parents=True, exist_ok=True)
    destination = folder / source.name
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=True, dirs_exist_ok=True)
    else:
        shutil.copy2(source, destination)
    return destination


def prepare_apple_native(app: App, generated: Path, run_platform: str, jobs: int) -> list[str]:
    """Places the libraries of an Apple run platform in `native/` of the generated folder, writes the file lists of the embed phase of every target template and platform and returns the `Haylen.xcconfig` setting that links the static libraries."""
    slice_name = APPLE_NATIVE_SLICES[run_platform]
    key = APPLE_NATIVE_KEYS[slice_name]
    platform = "macos" if slice_name == "macos" else slice_name.split("-")[0]
    folder = generated / "native"
    shutil.rmtree(folder, ignore_errors=True)
    embedded: list[Path] = []
    linked: list[Path] = []
    for library in app.native:
        if not library.ships_to(platform):
            continue
        source = build_apple_native(app, library, slice_name, jobs) if library.cmake else prebuilt_apple_native(library.files[platform], slice_name)
        placed = copy_into(source, folder / key)
        (linked if library.static else embedded).append(placed)

    for list_key in APPLE_NATIVE_KEYS.values():
        shipped = embedded if list_key == key else []
        write_if_changed(folder / f"{list_key}.xcfilelist", "".join(f"$(PROJECT_DIR)/{GENERATED_FOLDER}/native/{list_key}/{path.name}\n" for path in shipped))
        write_if_changed(folder / f"{list_key}-output.xcfilelist", "".join(f"$(TARGET_BUILD_DIR)/$(FRAMEWORKS_FOLDER_PATH)/{path.name}\n" for path in shipped))
    if not linked:
        return []
    flags = " ".join(f'"$(PROJECT_DIR)/{GENERATED_FOLDER}/native/{key}/{path.name}"' for path in linked)
    if slice_name != "ios-maccatalyst":
        return [f"HAYLEN_NATIVE_LDFLAGS[sdk={APPLE_STATIC_SDKS[slice_name]}*] = {flags}"]
    # The macOS target builds with the same SDK as Mac Catalyst, so only the build of the iOS target whose `IS_MACCATALYST` is `YES` links them.
    return [f"HAYLEN_NATIVE_LDFLAGS_MACCATALYST_YES = {flags}", "HAYLEN_NATIVE_LDFLAGS[sdk=macosx*] = $(HAYLEN_NATIVE_LDFLAGS_MACCATALYST_$(IS_MACCATALYST))"]


def write_apple_native_symbols(app: App, generated: Path) -> bool:
    """Writes `HaylenNativeSymbols.mm` for an app with static native libraries and returns whether it has any. Dead code stripping keeps what the table refers to, and the table lets `haylen.native` and `ffi.C` find the symbols without the app exporting them."""
    table = generated / "HaylenNativeSymbols.mm"
    libraries = [library for library in app.native if library.static]
    if not libraries:
        table.unlink(missing_ok=True)
        return False

    declarations = []
    registrations = []
    for library in libraries:
        condition = " || ".join(APPLE_STATIC_CONDITIONS[platform] for platform in library.platforms)
        entries = ", ".join(f'{{"{symbol}", reinterpret_cast<void*>(&{symbol})}}' for symbol in library.symbols)
        declarations += [f"#if {condition}", *(f'extern "C" void {symbol}(void);' for symbol in library.symbols), "#endif"]
        registrations += [f"#if {condition}", f'    haylen::platform::NativeLibraries::registerLinked("{library.name}", {{{entries}}});', "#endif"]
    lines = [
        "// Written by haylen.py from the native section of app.json. It links the symbols of the static native libraries into the app and registers them for haylen.native and ffi.C.",
        "#import <Foundation/Foundation.h>",
        "#include <TargetConditionals.h>",
        "",
        '#include "haylen/platform/NativeLibraries.hpp"',
        "",
        *declarations,
        "",
        "@interface HaylenNativeSymbols : NSObject",
        "@end",
        "",
        "@implementation HaylenNativeSymbols",
        "",
        "+ (void)load {",
        *registrations,
        "}",
        "",
        "@end",
        "",
    ]
    write_if_changed(table, "\n".join(lines))
    return True


def prepare_android_native(app: App, libraries: Path, jobs: int) -> None:
    """Places the libraries of an app in a `jniLibs` folder, building each ABI one after the other."""
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
            raise BuildError(f'The CMake target "{library.name}" of `{shown_path(library.cmake)}` built no shared library into `{shown_path(output)}`. Make the target a shared library named after the library.')
        placed.append(copy_into(built[0], folder))
    return placed


# Plugins: folders under `plugins/` of an app that give Lua a capability implemented natively on each platform, which `haylen.py` validates, packages and assembles into the platform projects.

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
# The target templates of `haylen/project.yml`, each with the folder of its `Info.plist`, the plugin platforms it builds for, since the iOS target also builds the Mac Catalyst app, and the platform of its engine frameworks.
APPLE_TEMPLATES = {"HaylenIOS": ("ios", ("ios", "catalyst"), "iOS"), "HaylenTVOS": ("tvos", ("tvos",), "tvOS"), "HaylenMacOS": ("macos", ("macos",), "macOS")}
# The entitlements file of each Apple plugin platform, in the project of the developer and in `haylen/`, with the target template and the setting that sign with it.
APPLE_ENTITLEMENTS = {
    "ios": ("ios/App.entitlements", "HaylenIOS", "CODE_SIGN_ENTITLEMENTS[sdk=iphone*]"),
    "catalyst": ("ios/Catalyst.entitlements", "HaylenIOS", "CODE_SIGN_ENTITLEMENTS[sdk=macosx*]"),
    "tvos": ("tvos/App.entitlements", "HaylenTVOS", "CODE_SIGN_ENTITLEMENTS"),
    "macos": ("macos/App.entitlements", "HaylenMacOS", "CODE_SIGN_ENTITLEMENTS"),
}
# The keys of a privacy manifest that the `privacy` section of a plugin may give.
PRIVACY_KEYS = {"NSPrivacyTracking", "NSPrivacyTrackingDomains", "NSPrivacyCollectedDataTypes", "NSPrivacyAccessedAPITypes"}


@dataclasses.dataclass(frozen=True, eq=False)
class Plugin:
    """A plugin folder with its manifest, which `haylen.py` validated, and the native library it adds to the app."""

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
            raise BuildError(f'The folder `{shown_path(folder)}` is not a plugin, because it holds no "plugin.json".')
        try:
            manifest = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise BuildError(f"The file `{shown_path(path)}` is not valid JSON: {error}.") from error
        problems = PluginManifestCheck.problems_of(folder, manifest)
        if problems:
            raise BuildError("\n".join([f"The plugin manifest `{shown_path(path)}` has these problems:", *(f"  {problem}" for problem in problems)]))
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
        """Checks a file that `haylen.py` copies into a project, which is a path inside the plugin or a reference to a file parameter, and so a file of the app."""
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
            self.report("id", 'must be in "dash-case", such as "camera-scanner".')
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
        self.check_keys("apple", apple, {"class", "sources", "packages", "frameworks", "infoPlist", "entitlements", "privacy", "resources", "buildScripts"}, set())
        if "class" in apple and not (isinstance(apple["class"], str) and OBJC_CLASS.fullmatch(apple["class"])):
            self.report("apple.class", 'must be the Objective-C name of the plugin class, such as "HaylenCameraScannerPlugin".')
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
        if "privacy" in apple:
            self.check_privacy(apple["privacy"])
        resources = apple.get("resources", [])
        if not isinstance(resources, list):
            self.report("apple.resources", "must list the files that the app bundle holds at its root.")
        for index, resource in enumerate(resources if isinstance(resources, list) else []):
            self.check_source(f"apple.resources[{index}]", resource)
        self.check_build_scripts(apple.get("buildScripts", []))
        self.check_references("apple", apple)

    def check_privacy(self, privacy: object) -> None:
        """Checks the privacy declarations of a plugin, which join the privacy manifest of the app."""
        if not self.is_object("apple.privacy", privacy, "must be an object with keys of a privacy manifest."):
            return
        self.check_keys("apple.privacy", privacy, PRIVACY_KEYS, set())
        if "NSPrivacyTracking" in privacy and not isinstance(privacy["NSPrivacyTracking"], bool):
            self.report("apple.privacy.NSPrivacyTracking", 'must be "true" or "false".')
        if "NSPrivacyTrackingDomains" in privacy:
            self.check_texts("apple.privacy.NSPrivacyTrackingDomains", privacy["NSPrivacyTrackingDomains"], None, "must list the domains that the plugin tracks through.")
        if "NSPrivacyCollectedDataTypes" in privacy and not (isinstance(privacy["NSPrivacyCollectedDataTypes"], list) and all(isinstance(entry, dict) for entry in privacy["NSPrivacyCollectedDataTypes"])):
            self.report("apple.privacy.NSPrivacyCollectedDataTypes", "must list objects that describe the data types that the plugin collects.")
        accessed = privacy.get("NSPrivacyAccessedAPITypes", [])
        valid = isinstance(accessed, list) and all(isinstance(entry, dict) and set(entry) == {"NSPrivacyAccessedAPIType", "NSPrivacyAccessedAPITypeReasons"} and isinstance(entry["NSPrivacyAccessedAPIType"], str) and isinstance(entry["NSPrivacyAccessedAPITypeReasons"], list) and entry["NSPrivacyAccessedAPITypeReasons"] and all(isinstance(reason, str) for reason in entry["NSPrivacyAccessedAPITypeReasons"]) for entry in accessed)
        if not valid:
            self.report("apple.privacy.NSPrivacyAccessedAPITypes", 'must list objects with an "NSPrivacyAccessedAPIType" and its "NSPrivacyAccessedAPITypeReasons", such as {"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryUserDefaults", "NSPrivacyAccessedAPITypeReasons": ["CA92.1"]}.')

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
            self.report("android.gradlePlugins", 'must list the id and the version of each Gradle plugin, such as {"id": "com.example.services", "version": "1.0.0"}.')
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
                self.report(f"{key}.to", 'must be a path inside the Android project, such as "app/services.json".')
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
            self.report("web.module", 'must be an ES module in the "web" folder of the plugin, such as "web/camera-scanner.js".')
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
        # The command `haylen.py plugin add` writes an empty text for every required parameter, which is the value the developer still has to fill in.
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
    where = shown_path(folder / "app.json")
    if not isinstance(section, dict):
        raise BuildError(f'The key "plugins" of `{where}` must map plugin ids to objects of parameter values.')

    problems: list[str] = []
    plugins: dict[str, Plugin] = {}
    for identifier in section:
        if not (folder / "plugins" / identifier / "plugin.json").is_file():
            problems.append(f'The key "plugins.{identifier}" names no plugin of the app, because `{shown_path(folder / "plugins" / identifier / "plugin.json")}` does not exist. Add the plugin with "{TOOL} plugin add <plugin folder or repository> --app {shown_path(folder)}".')
            continue
        try:
            plugins[identifier] = Plugin.load(folder / "plugins" / identifier)
        except BuildError as error:
            problems.append(str(error))

    values: dict[str, dict] = {}
    for plugin in plugins.values():
        problems += [f'The plugin "{plugin.id}" requires the plugin "{required}", which "app.json" does not list.' for required in plugin.requires if required not in section]
        values[plugin.id], found = parameter_values(folder, plugin, section[plugin.id], platform)
        problems += found
    if problems:
        raise BuildError("\n".join([f'The plugins of `{where}` cannot build for "{platform}":', *(f"  {line}" for problem in problems for line in problem.splitlines())]))
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


def plugin_section(app: App, plugin: Plugin, name: str, platforms: tuple[str, ...] = ()) -> dict | None:
    """Returns the `apple` or `android` section of a plugin with the values that `app.json` gives its parameters, or `None` when the plugin has none. For the files of some platforms, such as the entitlements of macOS, only the parameters that apply to one of them have values, so the keys that reference the others are left out."""
    if name not in plugin.manifest:
        return None
    values = app.plugin_values[plugin.id]
    if platforms:
        values = {key: value for key, value in values.items() if any(platform in plugin.parameters[key].get("platforms", plugin.manifest["platforms"]) for platform in platforms)}
    return substitute_parameters(plugin.manifest[name], values)


def plugin_file(app: App, plugin: Plugin, value: str) -> Path | None:
    """Resolves a file that a plugin copies into a project: a reference to a file parameter names a file of the app, or nothing while the parameter has no value, and any other path names a file of the plugin."""
    reference = PARAMETER_REFERENCE.fullmatch(value)
    if reference is None:
        return plugin.folder / value
    given = app.plugin_values[plugin.id].get(reference[1])
    if given is None:
        return None
    if not (app.folder / given).exists():
        raise BuildError(f'The parameter "{reference[1]}" of the plugin "{plugin.id}" names `{shown_path(app.folder / given)}`, which does not exist. Give it the path of a file relative to the app folder.')
    return app.folder / given


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
        text = Path(function["path"]).read_text(encoding="utf-8")
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
    text = source.read_text(encoding="utf-8")
    declared = re.findall(r"^\s*@program\s+(\w+)\s+(\w+)\s+(\w+)", text, re.MULTILINE)
    if len(declared) != 1 or declared[0][1] != "haylen_vs":
        raise BuildError(f'The shader `{shown_path(source)}` must declare exactly one "@program" whose vertex shader is "haylen_vs" from "haylen/material.glsl".')
    name = declared[0][0]

    def shdc(arguments: list, program: str) -> None:
        compiled = subprocess.run([str(part) for part in [ensure_shdc(), "--input", source.resolve(), *arguments]], cwd=SHADER_LIBRARY_DIR, capture_output=True, text=True)
        if compiled.returncode != 0:
            raise BuildError(f'The shader `{shown_path(source)}` does not compile as the "{program}" program, and "sokol-shdc" names the lines below.', f"{compiled.stdout}{compiled.stderr}".strip())

    sources: list[str] = []
    known: dict[str, int] = {}
    programs: dict[str, dict] = {}
    with tempfile.TemporaryDirectory() as temporary:
        folder = Path(temporary)
        for program, defines in SHADER_PROGRAMS.items():
            shdc(["--output", folder / program, "--slang", SHADER_SLANGS, "--format", "bare_yaml", *([f"--defines={':'.join(defines)}"] if defines else [])], program)
            reflection = parse_shdc_yaml((folder / f"{program}_reflection.yaml").read_text(encoding="utf-8"))
            programs[program] = {shader["slang"]: describe_shader_program(shader["programs"][0], sources, known) for shader in reflection["shaders"]}

        # The C header of one language holds the members of every uniform block, which the reflection YAML leaves out.
        shdc(["--output", folder / "reflection.h", "--slang", "glsl430", "--reflection"], "sprite")
        members = reflect_uniform_members((folder / "reflection.h").read_text(encoding="utf-8"), name)
        reflection = parse_shdc_yaml((folder / "sprite_reflection.yaml").read_text(encoding="utf-8"))["shaders"][0]["programs"][0]

    blocks = [{"name": block["struct_name"], "slot": block["slot"], "size": block["size"], "uniforms": members.get(block["struct_name"], [])} for block in reflection.get("uniform_blocks", []) if block["stage"] == "fragment" and block["struct_name"] not in SHADER_ENGINE_BLOCKS]
    textures = [{"name": view["texture"]["name"], "slot": view["texture"]["slot"]} for view in reflection.get("views", []) if view["texture"]["name"] not in SHADER_ENGINE_TEXTURES]
    document = {"format": "haylen-shader", "version": 1, "name": name, "blocks": blocks, "textures": textures, "sources": sources, "programs": programs}
    output.write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
    terminal.success(f"Compiled `{shown_path(source)}` into `{shown_path(output)}`.")


def shader_sources(folder: Path) -> list[Path]:
    """Lists the sources of an app that declare a program, which are the ones `haylen.py` compiles, while the others are files they include."""
    shaders = folder / "content" / "shaders"
    return sorted(path for path in shaders.rglob("*.glsl") if re.search(r"^\s*@program\b", path.read_text(encoding="utf-8"), re.MULTILINE)) if shaders.is_dir() else []


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
            terminal.error(str(error), error.details)


def command_shaders(args: argparse.Namespace) -> None:
    folder = resolve_app(args.app)
    if not shader_sources(folder):
        terminal.info(f"The app `{shown_path(folder)}` has no shaders under `{shown_path(folder / 'content' / 'shaders')}`.")
        return
    if args.force:
        for source in shader_sources(folder):
            compile_shader(source, source.with_suffix(".shader"))
        return
    compile_app_shaders(folder)
    terminal.success(f"The {len(shader_sources(folder))} shaders of `{shown_path(folder)}` are up to date.")


def app_kind(folder: Path) -> str:
    """Tells a C++ app, whose folder is also the CMake project that compiles it, from a Lua app."""
    return "cpp" if (folder / "CMakeLists.txt").is_file() else "lua"


def template_layers(kind: str) -> list[Path]:
    return [COMMON_TEMPLATE, TEMPLATES_DIR / kind]


def platform_layers(kind: str, template: str) -> list[Path]:
    return [layer / "platform" / template for layer in template_layers(kind)]


def platform_templates(kind: str | None = None) -> list[str]:
    """Lists the platform templates of a kind of app, the folders under `platform` of its layers, or of every kind."""
    layers = template_layers(kind) if kind else [COMMON_TEMPLATE, *(TEMPLATES_DIR / name for name in APP_KINDS)]
    return sorted({path.name for layer in layers if (layer / "platform").is_dir() for path in (layer / "platform").iterdir() if path.is_dir()})


def layered_files(layers: list[Path]) -> dict[str, Path]:
    """Lists the files of template layers by their path inside a layer, in the order of the layers, so the file of a later layer replaces the file of the same path in an earlier one."""
    files: dict[str, Path] = {}
    for layer in layers:
        if layer.is_dir():
            files |= project_files(layer)
    return dict(sorted(files.items()))


def copy_files(files: dict[str, Path], destination: Path) -> None:
    for name, source in files.items():
        (destination / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination / name)


# Platform projects: `platform/<template>` of an app belongs to the developer, and haylen.py builds it in place while it writes only its folder `haylen/`. An app without that folder uses a copy of the template that haylen.py keeps in the build folder of the app.


def write_if_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8")


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def files_hash(files: dict[str, Path]) -> str:
    """Hashes the relative paths and the contents of files."""
    digest = hashlib.sha256()
    for name, path in sorted(files.items()):
        digest.update(name.encode() + b"\0" + path.read_bytes())
    return digest.hexdigest()


def read_state(root: Path) -> dict:
    """Reads what haylen.py records about a platform project in `haylen/state.json`: the hashes of the last generation of `App.xcodeproj` and, for a copy of a template, the hash of the template."""
    path = root / GENERATED_FOLDER / "state.json"
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}


def write_state(root: Path, state: dict) -> None:
    write_if_changed(root / GENERATED_FOLDER / "state.json", json.dumps(state, indent=4, sort_keys=True) + "\n")


def is_owned(root: Path) -> bool:
    """Tells a copy of a template that haylen.py keeps under `build/apps` from the project of a developer."""
    return root.is_relative_to(APPS_DIR)


def project_root(app: App, template: str) -> Path:
    """Returns the project that haylen.py builds for an app with a platform template: `platform/<template>` of the app when it exists, or else the copy of the layers of the template for the kind of the app in the build folder of the app, which haylen.py makes again whenever the template changed."""
    own = app.folder / "platform" / template
    if own.is_dir():
        return own
    copy = app.build_folder / template
    files = layered_files(platform_layers(app_kind(app.folder), template))
    stamp = files_hash(files)
    if read_state(copy).get("template") != stamp:
        shutil.rmtree(copy, ignore_errors=True)
        copy_files(files, copy)
        write_state(copy, {"template": stamp})
    return copy


def value_text(value: object) -> str:
    return json.dumps(value, default=str, ensure_ascii=False)


def merge_plugin_keys(merged: dict, values: dict, owners: dict[str, str], owner: str, label: str, top: str | None = None) -> None:
    """Merges the keys of one source, such as a plugin, into the keys of the sources before it: objects merge key by key, arrays gain the items they lack, and any other value that differs fails the build with both sources named."""
    for key, value in values.items():
        first = top or key
        if key not in merged:
            merged[key] = copy.deepcopy(value)
            owners.setdefault(first, owner)
        elif isinstance(merged[key], dict) and isinstance(value, dict):
            merge_plugin_keys(merged[key], value, owners, owner, label, first)
        elif isinstance(merged[key], list) and isinstance(value, list):
            merged[key] += [item for item in value if item not in merged[key]]
        elif merged[key] != value:
            raise BuildError(f'The {label} key "{key}" is {value_text(merged[key])} for {owners[first]} and {value_text(value)} for {owner}. Give both the same value, or remove one of them.')


def complete_keys(own: dict, generated: dict, owners: dict[str, str], where: str, warnings: list[str], top: str | None = None) -> dict:
    """Completes the keys of a file of the developer with the generated ones: the value of the developer wins, a missing key takes the generated value, objects merge key by key and arrays gain the generated items they lack. A value of the developer that differs from the generated one stays, with a warning."""
    completed = copy.deepcopy(own)
    for key, value in generated.items():
        first = top or key
        if key not in completed:
            completed[key] = copy.deepcopy(value)
        elif isinstance(completed[key], dict) and isinstance(value, dict):
            completed[key] = complete_keys(completed[key], value, owners, where, warnings, first)
        elif isinstance(completed[key], list) and isinstance(value, list):
            completed[key] = completed[key] + [item for item in value if item not in completed[key]]
        elif completed[key] != value:
            warnings.append(f'The key "{key}" of `{where}` keeps its value {value_text(completed[key])}, while {owners[first]} gives {value_text(value)}.')
    return completed


def merge_privacy(manifests: list[dict]) -> dict:
    """Merges privacy manifests into one: the reasons of every accessed API type join the reasons of that type, the tracking domains and the collected data types gain the items they lack, and the app tracks when any manifest says so."""
    merged: dict = {}
    accessed: dict[str, list[str]] = {}
    for manifest in manifests:
        for entry in manifest.get("NSPrivacyAccessedAPITypes", []):
            reasons = accessed.setdefault(entry["NSPrivacyAccessedAPIType"], [])
            reasons += [reason for reason in entry["NSPrivacyAccessedAPITypeReasons"] if reason not in reasons]
        for key in ("NSPrivacyTrackingDomains", "NSPrivacyCollectedDataTypes"):
            if key in manifest:
                items = merged.setdefault(key, [])
                items += [item for item in manifest[key] if item not in items]
        if "NSPrivacyTracking" in manifest:
            merged["NSPrivacyTracking"] = merged.get("NSPrivacyTracking", False) or manifest["NSPrivacyTracking"]
    if accessed:
        merged["NSPrivacyAccessedAPITypes"] = [{"NSPrivacyAccessedAPIType": kind, "NSPrivacyAccessedAPITypeReasons": reasons} for kind, reasons in accessed.items()]
    return merged


def holds(built: object, needed: object, item: bool = False) -> bool:
    """Tells whether a value of a built app holds a required one: objects hold every required key, arrays hold an item for every required item, items of arrays and booleans must match, and any other value only has to exist, so a developer may word a usage description in their own way."""
    if isinstance(needed, dict):
        return isinstance(built, dict) and all(key in built and holds(built[key], value) for key, value in needed.items())
    if isinstance(needed, list):
        return isinstance(built, list) and all(any(holds(entry, wanted, True) for entry in built) for wanted in needed)
    return built == needed if item or isinstance(needed, bool) else True


def plist_snippet(key: str, value: object) -> str:
    """Writes one key and its value as the lines of a property list, such as an `Info.plist` or an entitlements file."""
    text = plistlib.dumps({key: value}).decode()
    return textwrap.dedent(text.split("<dict>\n", 1)[1].rsplit("</dict>", 1)[0]).rstrip()


@dataclasses.dataclass(frozen=True)
class Requirement:
    """Something the engine or a plugin needs that a built app lacks, with what to do about it and the snippet that adds it."""

    message: str
    advice: str
    snippet: str = ""


RELEASE_ADVICE = "A release ships the package of the app only inside its protected release, which the Release configuration of \"run\" and \"prepare\" builds."


def stop_on_release_problems(requirements: list[Requirement]) -> None:
    """Stops a run whose release build holds what a release never may, before the app launches."""
    problems = [requirement for requirement in requirements if requirement.advice == RELEASE_ADVICE]
    report_requirements(problems)
    if problems:
        raise BuildError(f"The release build holds the {len(problems)} files above that a release never ships.")


def report_requirements(requirements: list[Requirement]) -> None:
    """Prints what a built app lacks as warnings with the snippet to copy, which `run` shows before it launches the app, since the features that need them answer `unsupported` instead of stopping the app."""
    for requirement in requirements:
        terminal.warning(requirement.message, requirement.advice)
        if requirement.snippet:
            terminal.verbatim(textwrap.indent(requirement.snippet, "    "), error=True)


# Apple projects: `project.yml` includes `haylen/project.yml`, whose target templates bring the engine, the package and the plugins to the targets that name them, and `App.xcconfig` includes `haylen/Haylen.xcconfig`.


def apple_frameworks() -> dict[str, list[str]]:
    """The system frameworks of each Apple platform, by the names of `haylen-frameworks.json`, which the Apple artifacts publish from the lists of the engine."""
    return json.loads((ARTIFACTS_DIR / "apple" / "haylen-frameworks.json").read_text(encoding="utf-8"))


def apple_colorset(color: tuple[int, int, int, int], dark: tuple[int, int, int, int] | None = None) -> dict:
    """Describes a color set of an asset catalog, with a variant for the dark appearance when there is one."""
    def entry(value: tuple[int, int, int, int]) -> dict:
        red, green, blue, alpha = value
        return {"color": {"color-space": "srgb", "components": {"red": f"{red / 255:.3f}", "green": f"{green / 255:.3f}", "blue": f"{blue / 255:.3f}", "alpha": f"{alpha / 255:.3f}"}}, "idiom": "universal"}

    colors = [entry(color)]
    if dark:
        colors.append({**entry(dark), "appearances": [{"appearance": "luminosity", "value": "dark"}]})
    return {"colors": colors, "info": {"author": "xcode", "version": 1}}


def write_apple_splash(app: App, generated: Path) -> None:
    """Writes `Splash.xcassets`, whose `splash-logo` image, the logo of the app or the vector logo of the engine, and `splash-background` color, with its dark variant when `app.json` gives one, the launch screens of iOS and tvOS show."""
    catalog = generated / "Splash.xcassets"
    shutil.rmtree(catalog, ignore_errors=True)
    source = app.splash_logo or ENGINE_LOGO
    filename = f"splash-logo{source.suffix.lower()}"
    (catalog / "splash-logo.imageset").mkdir(parents=True)
    shutil.copy2(source, catalog / "splash-logo.imageset" / filename)
    image = {"images": [{"filename": filename, "idiom": "universal"}], "info": {"author": "xcode", "version": 1}}
    if source.suffix.lower() == ".svg":
        image["properties"] = {"preserves-vector-representation": True}
    write_if_changed(catalog / "splash-logo.imageset" / "Contents.json", json.dumps(image, indent=2) + "\n")
    write_if_changed(catalog / "splash-background.colorset" / "Contents.json", json.dumps(apple_colorset(app.background, app.dark_background), indent=2) + "\n")
    write_if_changed(catalog / "Contents.json", json.dumps({"info": {"author": "xcode", "version": 1}}, indent=2) + "\n")


def apple_plugin_keys(app: App, platforms: tuple[str, ...], section: str, keys: dict, owners: dict[str, str], label: str) -> dict:
    """Merges the `infoPlist` or `entitlements` keys of the plugins that build for any of the platforms into generated keys, and records the source of every key."""
    merged = copy.deepcopy(keys)
    owners.update({key: '"app.json"' for key in merged})
    for plugin in app.plugins:
        if plugin.supports(*platforms) and "apple" in plugin.manifest:
            merge_plugin_keys(merged, plugin_section(app, plugin, "apple", platforms).get(section, {}), owners, f'the plugin "{plugin.id}"', label)
    return merged


def read_plist(path: Path) -> dict:
    try:
        return plistlib.loads(path.read_bytes())
    except plistlib.InvalidFileException as error:
        raise BuildError(f"The file `{shown_path(path)}` is not a property list: {error}.") from error


def write_apple_plists(app: App, root: Path, generated: Path) -> None:
    """Writes `haylen/<platform>/Info.plist` for each target template: the `Info.plist` of the developer, completed with the orientations and the Dock setting of `app.json` and the `infoPlist` keys of the plugins, where the values of the developer win."""
    phone, pad = IOS_ORIENTATIONS[app.orientation]
    fills = {"ios": {"UISupportedInterfaceOrientations": phone, "UISupportedInterfaceOrientations~ipad": pad}, "tvos": {}, "macos": {} if app.show_in_taskbar else {"LSUIElement": True}}
    warnings: list[str] = []
    for template, (folder, platforms, _) in APPLE_TEMPLATES.items():
        own = root / folder / "Info.plist"
        if not own.is_file():
            raise BuildError(f'The Apple project `{shown_path(root)}` has no `{shown_path(own)}`, which the target template "{template}" completes into the "Info.plist" of its targets. Restore it from the template, which "{TOOL} platform diff {shown_path(app.folder)} --template apple" shows.')
        owners: dict[str, str] = {}
        generated_keys = apple_plugin_keys(app, platforms, "infoPlist", fills[folder], owners, '"Info.plist"')
        completed = complete_keys(read_plist(own), generated_keys, owners, shown_path(own), warnings)
        write_if_changed(generated / folder / "Info.plist", plistlib.dumps(completed, sort_keys=True).decode())
    for warning in warnings:
        terminal.warning(warning)


def write_apple_entitlements(app: App, root: Path, generated: Path) -> dict[str, dict[str, str]]:
    """Writes the entitlements of every Apple plugin platform that the developer or a plugin gives any, the file of the developer completed with the `entitlements` keys of the plugins, and returns the settings that sign each target template with them."""
    settings: dict[str, dict[str, str]] = {template: {} for template in APPLE_TEMPLATES}
    warnings: list[str] = []
    for platform, (path, template, setting) in APPLE_ENTITLEMENTS.items():
        own = root / path
        owners: dict[str, str] = {}
        generated_keys = apple_plugin_keys(app, (platform,), "entitlements", {}, owners, "entitlements")
        if not generated_keys and not own.is_file():
            continue
        completed = complete_keys(read_plist(own) if own.is_file() else {}, generated_keys, owners, shown_path(own), warnings)
        write_if_changed(generated / path, plistlib.dumps(completed, sort_keys=True).decode())
        settings[template][setting] = f"{GENERATED_FOLDER}/{path}"
    for warning in warnings:
        terminal.warning(warning)
    return settings


def engine_privacy() -> dict:
    return read_plist(ARTIFACTS_DIR / "apple" / "PrivacyInfo.xcprivacy")


def write_apple_privacy(app: App, root: Path, generated: Path) -> None:
    """Writes `PrivacyInfo.xcprivacy`, which merges the privacy manifest of the engine, the `privacy` keys of the Apple plugins and the `PrivacyInfo.xcprivacy` of the developer."""
    manifests = [engine_privacy()]
    manifests += [plugin_section(app, plugin, "apple").get("privacy", {}) for plugin in app.plugins if "apple" in plugin.manifest]
    if (root / "PrivacyInfo.xcprivacy").is_file():
        manifests.append(read_plist(root / "PrivacyInfo.xcprivacy"))
    write_if_changed(generated / "PrivacyInfo.xcprivacy", plistlib.dumps(merge_privacy(manifests), sort_keys=True).decode())


def write_apple_xcconfig(app: App, generated: Path, native: list[str]) -> None:
    """Writes `Haylen.xcconfig`, which `App.xcconfig` includes first, so the lines of the developer after the include win."""
    xcconfig = [
        "// Written by haylen.py from app.json. The file App.xcconfig includes it first, so its own settings win.",
        f"HAYLEN_PRODUCT_NAME = {app.name}",
        f"HAYLEN_DISPLAY_NAME = {app.name}",
        f"HAYLEN_BUNDLE_IDENTIFIER = {app.identifier}",
        f"MARKETING_VERSION = {app.version}",
        f"CURRENT_PROJECT_VERSION = {app.version}",
        *native,
        "OTHER_LDFLAGS = $(inherited) $(HAYLEN_NATIVE_LDFLAGS)",
        "",
    ]
    write_if_changed(generated / "Haylen.xcconfig", "\n".join(xcconfig))


def apple_spec(root: Path) -> dict:
    """Returns the `project.yml` of an Apple project as XcodeGen reads it, with its includes merged and without the target templates applied, so every target shows its own lists."""
    placeholder = root / GENERATED_FOLDER / "project.yml"
    if not placeholder.is_file():
        write_if_changed(placeholder, "{}\n")
    return json.loads(capture([ensure_xcodegen(), "dump", "--type", "json", "--spec", root / "project.yml"]))


def includes_generated(spec: dict) -> bool:
    """Tells whether the `project.yml` of a developer includes `haylen/project.yml`, which is what lets haylen.py generate `App.xcodeproj` again on its own."""
    return any((entry if isinstance(entry, str) else entry.get("path")) == f"{GENERATED_FOLDER}/project.yml" for entry in spec.get("include", []))


def template_targets(spec: dict, template: str) -> list[str]:
    return sorted(name for name, target in spec.get("targets", {}).items() if template in target.get("templates", []))


def apple_template_plugins(plugin: Plugin) -> dict[str, dict]:
    """Returns the target templates that the Apple part of a plugin joins, each with the XcodeGen keys that keep the plugin to iOS or to Mac Catalyst when it lists only one of them."""
    templates = {}
    for template, (_, platforms, _) in APPLE_TEMPLATES.items():
        supported = [platform for platform in platforms if plugin.supports(platform)]
        if supported:
            templates[template] = {} if len(supported) == len(platforms) else {"destinationFilters": ["iOS" if supported == ["ios"] else "macCatalyst"]}
    return templates


def apple_embed_script(folder: str) -> dict:
    """The build phase that copies the native libraries of a target template into the app and signs them like the app, from the file list of its platform."""
    script = "\n".join([
        "# Copies the libraries that haylen.py listed for this target and platform into the app and signs them like the app.",
        "set -e",
        'destination="$TARGET_BUILD_DIR/$FRAMEWORKS_FOLDER_PATH"',
        'mkdir -p "$destination"',
        "while IFS= read -r library; do",
        '  if [ -z "$library" ]; then',
        "    continue",
        "  fi",
        '  target="$destination/$(basename "$library")"',
        '  rm -rf "$target"',
        '  ditto "$library" "$target"',
        '  if [ "$CODE_SIGNING_ALLOWED" != "NO" ]; then',
        '    codesign --force --sign "${EXPANDED_CODE_SIGN_IDENTITY:--}" --timestamp=none --preserve-metadata=identifier,entitlements,flags "$target"',
        "  fi",
        'done < "$SCRIPT_INPUT_FILE_LIST_0"',
        "",
    ])
    lists = f"$(PROJECT_DIR)/{GENERATED_FOLDER}/native/{folder}-$(PLATFORM_NAME)"
    return {"name": "Embed native libraries", "inputFileLists": [f"{lists}.xcfilelist"], "outputFileLists": [f"{lists}-output.xcfilelist"], "script": script}


def write_apple_spec(app: App, root: Path, generated: Path, entitlements: dict[str, dict[str, str]], symbols: bool) -> None:
    """Copies the Apple sources and resources of the plugins into `haylen/plugins/` and writes `haylen/project.yml`, whose target templates give the targets the engine library and frameworks, the package, the splash assets, the completed Info.plist, entitlements and privacy manifest, the native libraries and the sources, Swift packages, system frameworks, resources and build scripts of the plugins of their platforms."""
    spec = apple_spec(root)
    frameworks = apple_frameworks()
    shutil.rmtree(generated / "plugins", ignore_errors=True)
    packages: dict[str, dict] = {}
    package_owners: dict[str, str] = {}
    bundled: dict[str, str] = {}
    templates = {template: {"sources": [], "postBuildScripts": []} for template in APPLE_TEMPLATES}
    linked: dict[str, dict[tuple[str, ...], list[str] | None]] = {template: {} for template in APPLE_TEMPLATES}
    for plugin in app.plugins:
        apple = plugin_section(app, plugin, "apple")
        if apple is None:
            continue

        copied = generated / "plugins" / plugin.id
        sources = []
        if "sources" in apple:
            shutil.copytree(plugin.folder / apple["sources"], copied / "sources", ignore=COPY_IGNORED)
            sources.append({"path": f"plugins/{plugin.id}/sources", "name": plugin.id, "group": "plugins"})
        for resource in plugin.manifest["apple"].get("resources", []):
            source = plugin_file(app, plugin, resource)
            if source is None:
                continue
            if source.name in bundled:
                raise BuildError(f'The plugins "{bundled[source.name]}" and "{plugin.id}" both place "{source.name}" at the root of the app bundle. Keep only one of them, or rename the file in one.')
            bundled[source.name] = plugin.id
            copy_into(source, copied / "resources")
        if (copied / "resources").is_dir():
            sources.append({"path": f"plugins/{plugin.id}/resources", "name": f"{plugin.id} resources", "group": "plugins", "buildPhase": "resources"})

        for name, package in apple.get("packages", {}).items():
            declared = {"url": package["url"], "exactVersion": package["exactVersion"]}
            if packages.setdefault(name, declared) != declared:
                raise BuildError(f'The plugins "{package_owners[name]}" and "{plugin.id}" ask for the Swift package "{name}" from different URLs or versions. Use versions of the plugins that pin the same package.')
            package_owners.setdefault(name, plugin.id)

        # A product or framework that two plugins link joins the template once, for every destination either plugin builds for.
        for template, filters in apple_template_plugins(plugin).items():
            templates[template]["sources"] += [{**source, **filters} for source in sources]
            templates[template]["postBuildScripts"] += apple.get("buildScripts", [])
            dependencies = [("package", name, product) for name, package in apple.get("packages", {}).items() for product in package["products"]]
            for dependency in [*dependencies, *(("sdk", framework) for framework in apple.get("frameworks", []))]:
                destinations = filters.get("destinationFilters")
                if dependency in linked[template] and (linked[template][dependency] is None or destinations is None or linked[template][dependency] != destinations):
                    destinations = None
                linked[template][dependency] = destinations

    included: dict[str, dict] = {}
    for template, (folder, _, listed) in APPLE_TEMPLATES.items():
        # XcodeGen refuses a dependency that a target lists twice, so a framework that a target of the template links itself stays out of the template.
        own = {dependency["sdk"] for target in template_targets(spec, template) for dependency in spec["targets"][target].get("dependencies", []) if "sdk" in dependency}
        engine = [framework for framework in frameworks[listed] if framework not in own]
        catalyst = [framework for framework in frameworks["macCatalyst"] if framework not in frameworks["iOS"] and framework not in own] if template == "HaylenIOS" else []
        dependencies = [{"framework": "Haylen.xcframework", "embed": False}, *({"sdk": framework} for framework in engine), *({"sdk": framework, "destinationFilters": ["macCatalyst"]} for framework in catalyst)]
        for dependency, destinations in linked[template].items():
            if dependency[0] == "sdk" and (dependency[1] in engine or dependency[1] in catalyst or dependency[1] in own):
                continue
            entry = {"package": dependency[1], "product": dependency[2]} if dependency[0] == "package" else {"sdk": dependency[1]}
            dependencies.append({**entry, **({"destinationFilters": destinations} if destinations else {})})

        sources = [{"path": "app", "type": "folder", "buildPhase": "resources"}, {"path": "PrivacyInfo.xcprivacy", "buildPhase": "resources"}, {"path": BOOTSTRAP_SOURCE}, {"path": APPLE_SWIFT_HELPERS}]
        if folder != "macos":
            sources.append({"path": "Splash.xcassets"})
        if symbols and folder != "macos":
            sources.append({"path": "HaylenNativeSymbols.mm"})
        scripts = templates[template]["postBuildScripts"]
        if any(library.ships_to(folder) for library in app.native):
            scripts = [apple_embed_script(folder), *scripts]
        settings = {"INFOPLIST_FILE": f"{GENERATED_FOLDER}/{folder}/Info.plist", **entitlements[template]}
        # The embed phase copies and signs whole library bundles, whose files the script sandbox would need listed one by one together with the temporary files of codesign, and the scripts of plugins read files of the build, so scripts run without it.
        if scripts:
            settings["ENABLE_USER_SCRIPT_SANDBOXING"] = "NO"
        included[template] = {"dependencies": dependencies, "sources": [*sources, *templates[template]["sources"]], "settings": {"base": settings}, **({"postBuildScripts": scripts} if scripts else {})}

    document = {**({"packages": packages} if packages else {}), "targetTemplates": included}
    header = "# Written by haylen.py from app.json, the plugins of the app and the engine artifacts. The file project.yml includes it, and each target takes its template in \"templates\".\n"
    write_if_changed(generated / "project.yml", header + json.dumps(document, indent=4) + "\n")


def development_bootstrap(release: str) -> str:
    """The bootstrap of the debug builds of a project, which read the package as it is, and which stops a build of the project where the condition of the preprocessor marks a release, since the project holds no protected release."""
    return "\n".join([
        '// Written by haylen.py for the debug builds of the app, which read its package as it is. A release build needs the protected release and the bootstrap of the app, which "haylen.py prepare" writes with "--config Release".',
        f"#if {release}",
        '#error "The project holds the development package of the app, which release builds never ship. Prepare it with \\"haylen prepare\\" and the app folder with \\"--config Release\\"."',
        "#endif",
        "",
    ])


def prepare_apple(app: App, root: Path, run_platform: str, config: str, engine_config: str, jobs: int) -> None:
    """Writes the folder `haylen/` of an Apple project, with the native libraries of a run platform, and nothing else of the project. The Release configuration writes the protected release of the app and its bootstrap, and the Debug configuration the package as it is."""
    require_host("apple")
    generated = root / GENERATED_FOLDER
    terminal.step(f"Preparing the Apple project `{shown_path(root)}`")
    generated.mkdir(parents=True, exist_ok=True)
    framework = ARTIFACTS_DIR / "apple" / "Haylen.xcframework"
    link = generated / "Haylen.xcframework"
    if not link.is_symlink() or link.readlink() != framework:
        link.unlink(missing_ok=True)
        link.symlink_to(framework)
    shutil.rmtree(generated / "app", ignore_errors=True)
    if config == "Release":
        shutil.copytree(build_release(app, CONTENT_PROFILES[run_platform], engine_config, jobs), generated / "app")
        write_bootstrap(app, CONTENT_PROFILES[run_platform], generated / BOOTSTRAP_SOURCE, engine_config, jobs)
    else:
        copy_package(app, generated / "app")
        write_if_changed(generated / BOOTSTRAP_SOURCE, development_bootstrap("!DEBUG"))
    write_if_changed(generated / APPLE_SWIFT_HELPERS, (ARTIFACTS_DIR / "apple" / APPLE_SWIFT_HELPERS).read_text(encoding="utf-8"))
    write_apple_splash(app, generated)
    write_apple_xcconfig(app, generated, prepare_apple_native(app, generated, run_platform, jobs))
    write_apple_plists(app, root, generated)
    write_apple_privacy(app, root, generated)
    write_apple_spec(app, root, generated, write_apple_entitlements(app, root, generated), write_apple_native_symbols(app, generated))


def apple_project_inputs(root: Path) -> str:
    """Hashes what `App.xcodeproj` is generated from: `project.yml`, `haylen/project.yml`, the XcodeGen version and the files of the plugins that the targets compile, since XcodeGen lists every file of a folder of sources."""
    digest = hashlib.sha256(XCODEGEN_VERSION.encode())
    for path in (root / "project.yml", root / GENERATED_FOLDER / "project.yml"):
        digest.update(path.read_bytes())
    plugins = root / GENERATED_FOLDER / "plugins"
    for path in sorted(plugins.rglob("*")) if plugins.is_dir() else []:
        digest.update(path.relative_to(plugins).as_posix().encode() + b"\0")
    return digest.hexdigest()


def trial_apple_project(root: Path) -> str:
    """Generates `App.xcodeproj` in a scratch folder that links every other entry of an Apple project and returns the hash of its `project.pbxproj`, which tells whether the project matches its spec without touching it."""
    with tempfile.TemporaryDirectory() as scratch:
        sandbox = Path(scratch) / root.name
        sandbox.mkdir()
        for entry in root.iterdir():
            if entry.name != "App.xcodeproj":
                (sandbox / entry.name).symlink_to(entry)
        capture([ensure_xcodegen(), "generate", "--quiet", "--spec", sandbox / "project.yml"], cwd=sandbox)
        return file_hash(sandbox / "App.xcodeproj" / "project.pbxproj")


def apple_project_action(included: bool, recorded: dict, inputs: str, current: str | None, template: str) -> str:
    """Decides what `run` does with `App.xcodeproj` before it builds: `keep` it when `project.yml` does not include `haylen/project.yml` or the inputs did not change since the last generation, `generate` it again when it is missing, holds no edits since the last generation or is an untouched copy of the template, and `compare` it with a trial generation otherwise, since only that tells whether it holds edits that a generation would lose."""
    if not included:
        return "keep"
    if current is None:
        return "generate"
    if recorded.get("inputs") == inputs:
        return "keep"
    if current == recorded.get("project") or (not recorded and current == template):
        return "generate"
    return "compare"


def generate_apple_project(app: App, root: Path, forced: bool) -> None:
    """Generates `App.xcodeproj` of an Apple project again with the pinned XcodeGen, when forced or when `apple_project_action` allows it, and records the hashes of the generation in `haylen/state.json`. A project whose edits would be lost stops the run with the way forward, and nothing of it changes."""
    project = root / "App.xcodeproj" / "project.pbxproj"
    inputs = apple_project_inputs(root)
    state = read_state(root)
    current = file_hash(project) if project.is_file() else None
    if not forced:
        template = file_hash(layered_files(platform_layers(app_kind(app.folder), "apple"))["App.xcodeproj/project.pbxproj"])
        action = apple_project_action(includes_generated(apple_spec(root)), state.get("xcodegen", {}), inputs, current, template)
        if action == "keep":
            return
        # A project that someone generated from the same inputs, such as the one a fresh clone of a repository holds, only needs its record.
        if action == "compare":
            if trial_apple_project(root) != current:
                raise BuildError(f'The project `{shown_path(root / "App.xcodeproj")}` changed since its last generation from "project.yml", or was never generated from it, so it stays as it is. Move the changes made in Xcode into "project.yml" and run "{TOOL} xcodegen {shown_path(app.folder)}", which generates the project again.')
            write_state(root, {**state, "xcodegen": {"inputs": inputs, "project": current}})
            return

    terminal.step(f'Generating `{shown_path(root / "App.xcodeproj")}` from "project.yml"')
    run([ensure_xcodegen(), "generate", "--quiet", "--spec", root / "project.yml"], cwd=root)
    write_state(root, {**state, "xcodegen": {"inputs": inputs, "project": file_hash(project)}})


def apple_simulator(family: str, requested: str | None) -> dict:
    """Finds the simulator a run targets: the requested name or id, else a booted one of the family, else the first available one."""
    devices = json.loads(capture(["xcrun", "simctl", "list", "devices", "available", "--json"]))["devices"]
    candidates = [device for runtime, entries in devices.items() if f"SimRuntime.{family}-" in runtime for device in entries]
    if requested:
        matches = [device for device in candidates if requested in (device["udid"], device["name"])]
        if not matches:
            raise BuildError(f'No available {family} simulator is named "{requested}". Use the command "xcrun simctl list devices available" to list them.')
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


def launch_apple(bundle: Path, args: argparse.Namespace, simulator: dict | None, arguments: list[str]) -> None:
    """Launches an app bundle on this Mac, a simulator or a device with the arguments of its command line and streams its output until it exits. The engine writes to the standard output on macOS and to the unified log elsewhere, which simulators and Mac Catalyst stream next to the output of the process."""
    desktop = args.platform in {"macos", "catalyst"}
    info = plistlib.loads((bundle / "Contents" / "Info.plist" if desktop else bundle / "Info.plist").read_bytes())
    executable, identifier = info["CFBundleExecutable"], info["CFBundleIdentifier"]
    if args.platform == "macos":
        run([bundle / "Contents" / "MacOS" / executable, *arguments])
    elif args.platform == "catalyst":
        with unified_log(executable, []):
            run([bundle / "Contents" / "MacOS" / executable, *arguments])
    elif simulator:
        udid = simulator["udid"]
        terminal.step(f'Launching "{identifier}" on the simulator "{simulator["name"]}"')
        if simulator["state"] != "Booted":
            run(["xcrun", "simctl", "boot", udid])
        run(["xcrun", "simctl", "bootstatus", udid, "-b"])
        run(["xcrun", "simctl", "install", udid, bundle])
        with unified_log(executable, ["xcrun", "simctl", "spawn", udid]):
            run(["xcrun", "simctl", "launch", "--console-pty", "--terminate-running-process", udid, identifier, *arguments])
    else:
        if not args.device:
            raise BuildError('Name the device with "--device". Use the command "xcrun devicectl list devices" to list them.')
        terminal.step(f'Launching "{identifier}" on the device "{args.device}"')
        run(["xcrun", "devicectl", "device", "install", "app", "--device", args.device, bundle])
        run(["xcrun", "devicectl", "device", "process", "launch", "--console", "--device", args.device, identifier, *arguments])


def apple_product(app: App, platform: str, config: str) -> Path:
    """Finds the app bundle that the last build of a platform left in the build folder of the app."""
    folder = app.build_folder / "xcode" / "Build" / "Products" / f"{config}{APPLE_RUNS[platform]['products']}"
    bundles = sorted(folder.glob("*.app"), key=lambda bundle: bundle.stat().st_mtime) if folder.is_dir() else []
    if not bundles:
        raise BuildError(f'No app of `{shown_path(app.folder)}` was built for "{platform}" in the "{config}" configuration. Build it with "{TOOL} run {shown_path(app.folder)} --platform {platform}".')
    return bundles[-1]


def build_apple(app: App, root: Path, args: argparse.Namespace) -> tuple[Path, dict | None]:
    """Builds the scheme of a run platform into the build folder of the app, outside the project, and returns the app bundle with the simulator it runs on."""
    settings = APPLE_RUNS[args.platform]
    simulator = apple_simulator(settings["simulator"], args.device) if "simulator" in settings else None
    destination = f"id={simulator['udid']}" if simulator else settings["destination"]
    if args.platform in {"macos", "catalyst"}:
        destination += f",arch={'arm64' if host_arch() == 'arm64' else 'x86_64'}"
    command = ["xcodebuild", "-project", root / "App.xcodeproj", "-scheme", settings["scheme"], "-configuration", args.config, "-destination", destination, "-derivedDataPath", app.build_folder / "xcode", "-jobs", str(args.jobs), "build"]
    if args.platform in {"ios", "tvos"}:
        command += ["-allowProvisioningUpdates", f"DEVELOPMENT_TEAM={apple_team()}"]
    terminal.step(f'Building the "{settings["scheme"]}" scheme for "{args.platform}" in the "{args.config}" configuration')
    run(command)
    return apple_product(app, args.platform, args.config), simulator


def apple_code(executable: Path) -> list[Path]:
    """Returns the files that hold the code of an app: its executable and, in the debug builds of Xcode, the `.debug.dylib` next to it, which holds the code while the executable only loads it."""
    debug = executable.with_name(executable.name + ".debug.dylib")
    return [executable, *([debug] if debug.is_file() else [])]


def apple_linked(executable: Path) -> set[str]:
    """Lists the system frameworks and libraries that the code of an app links, such as `Metal.framework` and `libz.tbd`."""
    linked = set()
    for line in (line for code in apple_code(executable) for line in capture(["otool", "-L", code]).splitlines()[1:]):
        path = line.strip().split(" (")[0]
        if found := re.search(r"/([^/]+)\.framework/", path):
            linked.add(f"{found[1]}.framework")
        elif found := re.search(r"/lib([^/.]+)[^/]*\.dylib$", path):
            linked.add(f"lib{found[1]}.tbd")
    return linked


def apple_entitlements(bundle: Path, executable: Path) -> dict:
    """Reads the entitlements that an app bundle is signed with, which apps for simulators carry in a section of their executable instead of their signature."""
    signed = subprocess.run(["codesign", "-d", "--entitlements", "-", "--xml", str(bundle)], capture_output=True)
    if signed.stdout.strip():
        return plistlib.loads(signed.stdout)
    with tempfile.TemporaryDirectory() as scratch:
        section = Path(scratch) / "entitlements"
        subprocess.run(["segedit", str(executable), "-extract", "__TEXT", "__entitlements", str(section)], capture_output=True)
        return plistlib.loads(section.read_bytes()) if section.is_file() and section.read_bytes().strip() else {}


def check_apple(app: App, root: Path, args: argparse.Namespace) -> list[Requirement]:
    """Checks the app bundle of the last build of a run platform against what the engine and every plugin of the platform need: `Info.plist` keys, system frameworks, entitlements, privacy declarations, plugin classes and resources."""
    settings = APPLE_RUNS[args.platform]
    template = settings["template"]
    folder = APPLE_TEMPLATES[template][0]
    bundle = apple_product(app, args.platform, args.config)
    contents = bundle / "Contents" if args.platform in {"macos", "catalyst"} else bundle
    resources = contents / "Resources" if args.platform in {"macos", "catalyst"} else bundle
    info = read_plist(contents / "Info.plist")
    executable = contents / "MacOS" / info["CFBundleExecutable"] if args.platform in {"macos", "catalyst"} else bundle / info["CFBundleExecutable"]
    linked = apple_linked(executable)
    entitlements = apple_entitlements(bundle, executable)
    privacy = read_plist(resources / "PrivacyInfo.xcprivacy") if (resources / "PrivacyInfo.xcprivacy").is_file() else {}
    symbols = {symbol for code in apple_code(executable) for symbol in capture(["nm", "-jU", code]).split()}
    plugin_platform = RUN_TARGETS[args.platform].plugins
    entitlements_file = APPLE_ENTITLEMENTS[plugin_platform][0]
    targets = f'the target of `{shown_path(root / "project.yml")}`'
    missing: list[Requirement] = []

    def need_framework(owner: str, framework: str) -> None:
        if framework not in linked:
            missing.append(Requirement(f'{owner} needs "{framework}", which the built app does not link.', f'Keep "templates: [{template}]" on {targets}, or add to its "dependencies":', f"- sdk: {framework}"))

    def need_plist(owner: str, key: str, value: object) -> None:
        if not holds(info, {key: value}):
            missing.append(Requirement(f'{owner} needs "{key}" in the "Info.plist" of the app, which the built app lacks.', f'Add to `{shown_path(root / folder / "Info.plist")}`:', plist_snippet(key, value)))

    def need_privacy(owner: str, needed: dict) -> None:
        for key, value in needed.items():
            if not holds(privacy, {key: value}):
                missing.append(Requirement(f'{owner} needs "{key}" in the privacy manifest of the app, which the built app lacks.', f'Keep "templates: [{template}]" on {targets}, or add to `{shown_path(root / "PrivacyInfo.xcprivacy")}`:', plist_snippet(key, value)))

    for framework in apple_frameworks()[settings["frameworks"]]:
        need_framework("Haylen", framework)
    if folder != "macos" and "UIApplicationSceneManifest" not in info:
        missing.append(Requirement('Haylen needs "UIApplicationSceneManifest" in the "Info.plist" of the app, which the built app lacks, since the runtime draws in the window of a scene.', f'Add to `{shown_path(root / folder / "Info.plist")}`:', plist_snippet("UIApplicationSceneManifest", {"UIApplicationSupportsMultipleScenes": folder == "ios"})))
    need_privacy("Haylen", engine_privacy())

    for plugin in app.plugins:
        apple = plugin_section(app, plugin, "apple", (plugin_platform,))
        if apple is None or not plugin.supports(plugin_platform):
            continue
        owner = f'The plugin "{plugin.id}"'
        if "class" in apple and f"_OBJC_CLASS_$_{apple['class']}" not in symbols:
            missing.append(Requirement(f'{owner} needs its Apple sources in the app, whose class "{apple["class"]}" the built app lacks, so the plugin does not load.', f'Keep "templates: [{template}]" on {targets}.'))
        for framework in apple.get("frameworks", []):
            need_framework(owner, framework)
        for key, value in apple.get("infoPlist", {}).items():
            need_plist(owner, key, value)
        for key, value in apple.get("entitlements", {}).items():
            if not holds(entitlements, {key: value}):
                missing.append(Requirement(f'{owner} needs the entitlement "{key}", which the built app is not signed with.', f'Add to `{shown_path(root / entitlements_file)}`:', plist_snippet(key, value)))
        need_privacy(owner, apple.get("privacy", {}))
        for resource in plugin.manifest["apple"].get("resources", []):
            source = plugin_file(app, plugin, resource)
            if source is not None and not (resources / source.name).exists():
                missing.append(Requirement(f'{owner} needs "{source.name}" at the root of the app bundle, which the built app lacks.', f'Keep "templates: [{template}]" on {targets}.'))
    if args.config == "Release":
        release = "Contents/Resources/app" if args.platform in {"macos", "catalyst"} else "app"
        missing += [Requirement(problem, RELEASE_ADVICE) for problem in release_problems(app, folder_entries(bundle), release, read_secrets(app))]
    return missing


def prepare_apple_run(app: App, args: argparse.Namespace) -> Path:
    root = project_root(app, "apple")
    prepare_apple(app, root, args.platform, args.config, args.engine_config, args.jobs)
    return root


def run_apple(app: App, root: Path, args: argparse.Namespace) -> None:
    generate_apple_project(app, root, forced=False)
    bundle, simulator = build_apple(app, root, args)
    requirements = check_apple(app, root, args)
    stop_on_release_problems(requirements)
    report_requirements(requirements)
    with device_development(app, root / GENERATED_FOLDER / "app", args) as arguments:
        launch_apple(bundle, args, simulator, arguments)


# Android projects: the Gradle scripts read `haylen/haylen.properties` and take `haylen/assets`, `haylen/res` and `haylen/jniLibs` as source folders of the app module and `haylen/plugins/<id>` as the modules of the plugins.


def java_property(value: str) -> str:
    """Escapes a value of a Java properties file, which Gradle reads as ISO 8859-1 text with backslash escapes, so any text survives."""
    units = value.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").encode("utf-16-be")
    codes = (int.from_bytes(units[index : index + 2], "big") for index in range(0, len(units), 2))
    return "".join(chr(code) if code < 0x80 else f"\\u{code:04x}" for code in codes)


def prepare_android_plugins(app: App, root: Path) -> dict[str, str]:
    """Copies the library module of every plugin into `haylen/plugins/<id>` and returns the properties that include the modules, apply their Gradle plugins and set their manifest placeholders. The files that plugins place in the project, such as `app/services.json`, go only into a copy of the template that haylen.py owns, since the project of a developer is theirs, and `check` names the ones it lacks."""
    modules: list[str] = []
    gradle_plugins: dict[str, str] = {}
    placeholders: dict[str, str] = {}
    owners: dict[str, dict[str, str]] = {"gradle": {}, "placeholder": {}, "file": {}}
    for plugin in app.plugins:
        android = plugin_section(app, plugin, "android")
        if android is None:
            continue
        shutil.copytree(plugin.folder / android["module"], root / GENERATED_FOLDER / "plugins" / plugin.id, ignore=COPY_IGNORED)
        modules.append(f"{plugin.id}={GENERATED_FOLDER}/plugins/{plugin.id}")

        for entry in android.get("gradlePlugins", []):
            if gradle_plugins.setdefault(entry["id"], entry["version"]) != entry["version"]:
                raise BuildError(f"The plugins \"{owners['gradle'][entry['id']]}\" and \"{plugin.id}\" apply the Gradle plugin \"{entry['id']}\" in different versions. Use versions of the plugins that apply the same one.")
            owners["gradle"].setdefault(entry["id"], plugin.id)
        for name, value in android.get("placeholders", {}).items():
            if placeholders.setdefault(name, parameter_text(value)) != parameter_text(value):
                raise BuildError(f"The plugins \"{owners['placeholder'][name]}\" and \"{plugin.id}\" give the manifest placeholder \"{name}\" different values. Give both the same value, or keep only one of the plugins.")
            owners["placeholder"].setdefault(name, plugin.id)

        for source, destination in android_plugin_files(app, plugin):
            if destination in owners["file"]:
                raise BuildError(f"The plugins \"{owners['file'][destination]}\" and \"{plugin.id}\" both place \"{destination}\" in the Android project. Keep only one of them.")
            owners["file"][destination] = plugin.id
            if is_owned(root):
                (root / destination).parent.mkdir(parents=True, exist_ok=True)
                if source.is_dir():
                    shutil.copytree(source, root / destination, dirs_exist_ok=True)
                else:
                    shutil.copy2(source, root / destination)

    return {
        "plugins": ",".join(modules),
        "gradlePlugins": ",".join(f"{identifier}={version}" for identifier, version in gradle_plugins.items()),
        **{f"placeholder.{name}": value for name, value in placeholders.items()},
    }


def android_plugin_files(app: App, plugin: Plugin) -> list[tuple[Path, str]]:
    """Returns the files that a plugin places in the Android project as their sources and their paths in the project, leaving out the ones whose parameter has no value."""
    files = []
    for entry in plugin.manifest["android"].get("files", []):
        source = plugin_file(app, plugin, entry["from"])
        destination = substitute_parameters(entry["to"], app.plugin_values[plugin.id])
        if source is not None and destination is not None:
            files.append((source, destination))
    return files


def write_android_plugins(app: App, assets: Path) -> None:
    """Writes `haylen-plugins.json` into the assets of an Android project: the id, version and parameter values of every plugin with an Android part, in load order, from which the runtime loads their classes when the process starts. The runtime never reads `app.json` or a `plugin.json` for them, which a protected release keeps inside its encrypted app domain."""
    plugins = [{"id": plugin.id, "version": plugin.version, "config": app.plugin_values[plugin.id]} for plugin in app.plugins if "android" in plugin.manifest]
    write_if_changed(assets / ANDROID_PLUGINS_FILE, json.dumps({"plugins": plugins}, indent=4) + "\n")


def android_splash_colors(color: tuple[int, int, int, int]) -> str:
    """Writes the resource file of the splash background, which Android reads as `#AARRGGBB`."""
    red, green, blue, alpha = color
    return f'<?xml version="1.0" encoding="utf-8"?>\n<!-- Written by haylen.py from the splash of app.json. -->\n<resources>\n    <color name="haylen_splash_background">#{alpha:02X}{red:02X}{green:02X}{blue:02X}</color>\n</resources>\n'


def write_android_splash(app: App, resources: Path) -> None:
    """Writes the splash background and logo of an app as resources that replace the defaults of the `haylen` library, which show the engine logo, with the dark background in the resources of the night mode when `app.json` gives one."""
    write_if_changed(resources / "values" / "haylen_splash.xml", android_splash_colors(app.background))
    if app.dark_background:
        write_if_changed(resources / "values-night" / "haylen_splash.xml", android_splash_colors(app.dark_background))
    if app.splash_logo:
        if app.splash_logo.suffix.lower() not in {".png", ".webp", ".jpg", ".jpeg"}:
            raise BuildError(f'Android splash logos are PNG, WebP or JPEG images, so `{shown_path(app.splash_logo)}` does not fit. Give "splash.logo" an image in one of those formats.')
        (resources / "drawable").mkdir(parents=True, exist_ok=True)
        shutil.copy2(app.splash_logo, resources / "drawable" / f"haylen_splash_logo{app.splash_logo.suffix.lower()}")


def prepare_android(app: App, root: Path, library: str, jobs: int, release: Path | None = None) -> None:
    """Writes the folder `haylen/` of an Android project: `haylen.properties` with the identity, version and orientation of the app, the native library its activity loads, the engine repository and version, the plugins and the build folder, in `assets/app` the files of a protected release or else the package with its index, the splash resources in `res`, the native libraries in `jniLibs` and the plugin modules in `plugins`."""
    generated = root / GENERATED_FOLDER
    terminal.step(f"Preparing the Android project `{shown_path(root)}`")
    for name in ("assets", "res", "jniLibs", "plugins"):
        shutil.rmtree(generated / name, ignore_errors=True)
    if release:
        # A release is a flat folder, which Android lists without an index.
        shutil.copytree(release, generated / "assets" / "app")
    else:
        files = copy_package(app, generated / "assets" / "app")
        # Android cannot list asset folders recursively, so the runtime reads the files of the package from this index.
        write_if_changed(generated / "assets" / "app" / "haylen-package-index.json", json.dumps(sorted(files)))
    write_android_plugins(app, generated / "assets")
    write_android_splash(app, generated / "res")
    prepare_android_native(app, generated / "jniLibs", jobs)
    values = {
        "repository": (ARTIFACTS_DIR / "android" / "maven").as_posix(),
        "engineVersion": engine_version(),
        "name": app.name,
        "identifier": app.identifier,
        "versionName": app.version,
        "versionCode": str(app.version_code),
        "orientation": ANDROID_ORIENTATIONS[app.orientation],
        "library": library,
        "buildDirectory": (app.build_folder / "gradle").as_posix(),
        **prepare_android_plugins(app, root),
    }
    lines = ["# Written by haylen.py from app.json and the plugins of the app. The Gradle scripts of the project read it."]
    write_if_changed(generated / "haylen.properties", "\n".join([*lines, *(f"{key}={java_property(value)}" for key, value in values.items())]) + "\n")


def android_device(requested: str | None) -> str:
    devices = [line.split()[0] for line in capture([adb(), "devices"]).splitlines()[1:] if line.strip().endswith("device")]
    if requested:
        if requested not in devices:
            raise BuildError(f'The device "{requested}" is not a connected Android device. Use the command "adb devices" to list them.')
        return requested
    if len(devices) != 1:
        raise BuildError('Name the Android device with "--device", because ' + ("none is connected." if not devices else f"{len(devices)} are connected: {', '.join(f'"{device}"' for device in devices)}."))
    return devices[0]


def aapt2() -> Path:
    tools = sorted((android_sdk() / "build-tools").glob("*/aapt2"), key=lambda path: [int(part) if part.isdigit() else 0 for part in path.parent.name.split(".")])
    if not tools:
        raise BuildError('The Android SDK has no build tools with "aapt2". Install them with the SDK Manager of Android Studio or with "sdkmanager".')
    return tools[-1]


def android_product(app: App, config: str) -> Path:
    """Finds the newest APK of a configuration that Gradle built into the build folder of the app, whatever product flavors the project defines."""
    variant = "release" if config == "Release" else "debug"
    folder = app.build_folder / "gradle" / "app" / "outputs" / "apk"
    apks = sorted((path for path in folder.rglob("*.apk") if variant in path.parent.parts), key=lambda path: path.stat().st_mtime) if folder.is_dir() else []
    if not apks:
        raise BuildError(f'No APK of `{shown_path(app.folder)}` was built in the "{config}" configuration. Build it with "{TOOL} run {shown_path(app.folder)} --platform android".')
    return apks[-1]


def build_android(app: App, root: Path, args: argparse.Namespace) -> Path:
    """Builds the APK of an Android project with Gradle, whose outputs and project cache go to the build folder of the app, outside the project."""
    variant = "Release" if args.config == "Release" else "Debug"
    terminal.step(f'Building the APK of `{shown_path(root)}` with Gradle in the "{args.config}" configuration')
    run([ensure_gradle(), "-p", root, f":app:assemble{variant}", f"--max-workers={args.jobs}", "--project-cache-dir", app.build_folder / "gradle-cache"])
    return android_product(app, args.config)


def launch_android(apk: Path, args: argparse.Namespace, device: str, arguments: list[str]) -> None:
    """Installs an APK on a device, starts it and streams the log of its process until it ends. Android starts apps without a command line, so the address of the development server travels as an extra of the intent, which only a debuggable app reads."""
    package = capture([aapt2(), "dump", "packagename", apk]).strip()
    terminal.step(f'Launching "{package}" on the Android device "{device}"')
    run([adb(), "-s", device, "install", "-r", apk])
    # The launcher intent starts the task of the app, so the launcher icon brings that task back as it is later. The shell of the device reads the command line, so the extra is quoted for it.
    extras = ["--es", ANDROID_DEVELOPMENT_EXTRA, shlex.quote(arguments[arguments.index("--dev-server") + 1])] if "--dev-server" in arguments else []
    run([adb(), "-s", device, "shell", "am", "start", "-W", "-a", "android.intent.action.MAIN", "-c", "android.intent.category.LAUNCHER", "-n", f"{package}/dev.haylen.HaylenActivity", *extras])
    process = capture([adb(), "-s", device, "shell", "pidof", package]).strip()
    if process:
        run([adb(), "-s", device, "logcat", "--pid", process])


def android_manifest(apk: Path) -> dict[str, set[str]]:
    """Reads the merged manifest of an APK with `aapt2` and returns the names of its elements by tag, such as the permissions, the activities, the providers and the meta-data."""
    elements: dict[str, set[str]] = {}
    tag = ""
    for line in capture([aapt2(), "dump", "xmltree", "--file", "AndroidManifest.xml", apk]).splitlines():
        if found := re.match(r"\s*E: (\S+)", line):
            tag = found[1]
        elif found := re.match(r'\s*A: http://schemas.android.com/apk/res/android:name\(0x[0-9a-f]+\)="([^"]*)"', line):
            elements.setdefault(tag, set()).add(found[1])
    return elements


ANDROID_NAMESPACE = "{http://schemas.android.com/apk/res/android}"
TOOLS_NAMESPACE = "{http://schemas.android.com/tools}"
ANDROID_COMPONENTS = ("activity", "activity-alias", "service", "receiver", "provider")


def android_module_needs(module: Path) -> list[tuple[str, str]]:
    """Reads what the manifest of a plugin module brings to the merged manifest of the app, as tags and names: its permissions, its components and its meta-data."""
    manifest = xml.etree.ElementTree.parse(module / "src" / "main" / "AndroidManifest.xml").getroot()
    script = next((module / name for name in ("build.gradle.kts", "build.gradle") if (module / name).is_file()), None)
    found = re.search(r'namespace\s*=\s*"([^"]+)"', script.read_text(encoding="utf-8")) if script else None
    needs = []
    for element in manifest.iter():
        name = element.get(f"{ANDROID_NAMESPACE}name", "")
        if element.tag not in ("uses-permission", "meta-data", *ANDROID_COMPONENTS) or not name or "${" in name or element.get(f"{TOOLS_NAMESPACE}node") == "remove":
            continue
        if name.startswith(".") and found:
            name = found[1] + name
        needs.append((element.tag, name))
    return needs


def check_android(app: App, root: Path, args: argparse.Namespace) -> list[Requirement]:
    """Checks the merged manifest and the native libraries of the last APK against what the engine and every plugin need, and the files that plugins place in a project of the developer."""
    apk = android_product(app, args.config)
    merged = android_manifest(apk)
    with zipfile.ZipFile(apk) as archive:
        entries = set(archive.namelist())
        compressed = [info.filename for info in archive.infolist() if info.filename.endswith(RELEASE_SHARD_SUFFIX) and info.compress_type != zipfile.ZIP_STORED]
    manifest = root / "app" / "src" / "main" / "AndroidManifest.xml"
    own = manifest.read_text(encoding="utf-8") if manifest.is_file() else ""
    modules = f'Keep the plugin modules of "{GENERATED_FOLDER}/haylen.properties" in `{shown_path(root / "settings.gradle.kts")}` and `{shown_path(root / "app" / "build.gradle.kts")}`.'
    missing: list[Requirement] = []

    def need(owner: str, tag: str, name: str) -> None:
        if name in merged.get(tag, set()) or (tag == "activity" and name in merged.get("activity-alias", set())):
            return
        if tag == "uses-permission":
            message = f'{owner} needs the permission "{name}", which the merged manifest of the built app lacks.'
            if re.search(rf'<uses-permission[^>]*"{re.escape(name)}"[^>]*tools:node="remove"', own):
                missing.append(Requirement(message, f'The file `{shown_path(manifest)}` removes it with the attribute "tools:node" set to "remove". Delete that entry, or keep it, and the calls that need the permission answer "unsupported".'))
            else:
                missing.append(Requirement(message, f'Add to `{shown_path(manifest)}`:', f'<uses-permission android:name="{name}" />'))
            return
        missing.append(Requirement(f'{owner} needs the {tag} "{name}", which the merged manifest of the built app lacks.', modules))

    need("Haylen", "activity", "dev.haylen.HaylenActivity")
    if any("android" in plugin.manifest for plugin in app.plugins):
        need("Haylen", "provider", "dev.haylen.HaylenPluginProvider")
    for library in app.native:
        if library.ships_to("android") and not any(entry.startswith("lib/") and entry.endswith(f"/lib{library.name}.so") for entry in entries):
            missing.append(Requirement(f'The app needs the native library "{library.name}", which the built app lacks.', f'Keep "{GENERATED_FOLDER}/jniLibs" among the "jniLibs" folders of `{shown_path(root / "app" / "build.gradle.kts")}`.'))

    for plugin in app.plugins:
        android = plugin_section(app, plugin, "android")
        if android is None:
            continue
        owner = f'The plugin "{plugin.id}"'
        for tag, name in android_module_needs(plugin.folder / android["module"]):
            need(owner, tag, name)
        for source, destination in android_plugin_files(app, plugin):
            if not (root / destination).exists():
                missing.append(Requirement(f'{owner} needs "{destination}" in the Android project, which the project lacks.', f'Copy `{shown_path(source)}` to `{shown_path(root / destination)}`.'))
    if args.config == "Release":
        missing += [Requirement(problem, RELEASE_ADVICE) for problem in release_problems(app, apk_entries(apk), "assets/app", read_secrets(app))]
        missing += [Requirement(f'The release holds the shard "{name}" compressed, while the runtime reads shards in place.', f'Keep "noCompress" of "hpak" in `{shown_path(root / "app" / "build.gradle.kts")}`.') for name in compressed]
    return missing


def apk_entries(apk: Path) -> list[tuple[str, int, Callable[[], bytes]]]:
    """Lists every entry of an APK with its size and the function that reads it."""
    with zipfile.ZipFile(apk) as archive:
        infos = [info for info in archive.infolist() if not info.is_dir()]
    return [(info.filename, info.file_size, functools.partial(read_zip_entry, apk, info.filename)) for info in infos]


def read_zip_entry(archive: Path, name: str) -> bytes:
    with zipfile.ZipFile(archive) as opened:
        return opened.read(name)


RELEASE_LIBRARY_PROJECT = """# Written by haylen.py for the release build of the app: the library that HaylenActivity loads, the Lua player with the bootstrap of the app linked against the SDK of the prebuilt engine, which exports only the entry points of Java and GameActivity and ships no symbols, while its symbols stay in a file next to the build for crash reports.
cmake_minimum_required(VERSION 3.28)
project(haylen_release LANGUAGES C CXX)
find_package(haylen REQUIRED CONFIG)
add_library({name} SHARED {bootstrap} "${{HAYLEN_ENGINE_DIR}}/src/platform/sokol/LuaPlayer.cpp")
target_link_libraries({name} PRIVATE haylen::runtime)
set_target_properties({name} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
target_link_options({name} PRIVATE "LINKER:--version-script=${{CMAKE_CURRENT_SOURCE_DIR}}/exports.map" "LINKER:--gc-sections")
add_custom_command(TARGET {name} POST_BUILD
  COMMAND "${{CMAKE_OBJCOPY}}" --only-keep-debug "$<TARGET_FILE:{name}>" "$<TARGET_FILE:{name}>.debug"
  COMMAND "${{CMAKE_OBJCOPY}}" --strip-all "$<TARGET_FILE:{name}>"
  VERBATIM
)
"""
# The symbols that Java and GameActivity look up in the library of an app.
RELEASE_LIBRARY_EXPORTS = """{
  global:
    JNI_OnLoad;
    Java_*;
    GameActivity_onCreate;
  local:
    *;
};
"""


def build_release_library(app: App, libraries: Path, args: argparse.Namespace) -> None:
    """Links the library of the release build of an Android app for every ABI, the Lua player with the bootstrap of the app, against the SDK of each ABI in the Android artifacts, into a `jniLibs` folder."""
    project = app.build_folder / "release-library"
    write_bootstrap(app, CONTENT_PROFILES["android"], project / BOOTSTRAP_SOURCE, args.engine_config, args.jobs)
    write_if_changed(project / "CMakeLists.txt", RELEASE_LIBRARY_PROJECT.format(name=ANDROID_RELEASE_LIBRARY, bootstrap=BOOTSTRAP_SOURCE))
    write_if_changed(project / "exports.map", RELEASE_LIBRARY_EXPORTS)
    for abi in ANDROID_ABIS:
        build = project / f"build-{abi}"
        config = next(android_engine_sdk(abi).glob("*/cmake/haylen"), None)
        if config is None:
            raise BuildError(f'The Android artifacts hold no SDK for "{abi}". Build them again with "{TOOL} engine --platform android".')
        terminal.step(f'Linking the release library of `{shown_path(app.folder)}` for "{abi}"')
        run(["cmake", "-S", project, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-Dhaylen_DIR={config}", *android_options(abi)])
        run(["cmake", "--build", build, "--parallel", str(args.jobs)])
        copy_into(build / f"lib{ANDROID_RELEASE_LIBRARY}.so", libraries / abi)


def prepare_android_run(app: App, args: argparse.Namespace) -> Path:
    """Prepares the Android project of a Lua app: a debug build plays the package with the Lua player of the `haylen` library, and a release build ships the protected release with a library of the app."""
    root = project_root(app, "android")
    if args.config != "Release":
        prepare_android(app, root, "haylen", args.jobs)
        return root
    prepare_android(app, root, ANDROID_RELEASE_LIBRARY, args.jobs, build_release(app, CONTENT_PROFILES["android"], args.engine_config, args.jobs))
    build_release_library(app, root / GENERATED_FOLDER / "jniLibs", args)
    return root


def run_android(app: App, root: Path, args: argparse.Namespace) -> None:
    device = android_device(args.device)
    apk = build_android(app, root, args)
    requirements = check_android(app, root, args)
    stop_on_release_problems(requirements)
    report_requirements(requirements)
    with device_development(app, root / GENERATED_FOLDER / "assets" / "app", args, device) as arguments:
        launch_android(apk, args, device, arguments)


# Web sites: the files of `platform/web` of an app, or of the template, go as they are into the site in the build folder of the app, and the generated files join them there.


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


def css_color(color: tuple[int, int, int, int]) -> str:
    red, green, blue, alpha = color
    return f"rgba({red}, {green}, {blue}, {alpha / 255:.3f})"


def write_web_settings(app: App, site: Path, release: list[str] | None) -> None:
    """Writes the splash logo, the web modules of the plugins and `config.json` with the files of the protected release that download before the app starts, or `app.zip` in development, and the sizes that let the loader show progress when the server sends no length."""
    downloads = [f"app/{name}" for name in release] if release is not None else ["app.zip"]
    if release is None:
        package_folder(app.folder, site / "app.zip")
    logo = app.splash_logo or ENGINE_LOGO
    logo_name = f"splash{logo.suffix.lower()}"
    shutil.copy2(logo, site / logo_name)
    splash = {"logo": logo_name, "background": css_color(app.background)}
    if app.dark_background:
        splash["darkBackground"] = css_color(app.dark_background)
    sizes = {path: (site / path).stat().st_size for path in (*downloads, "webgpu/haylen.wasm", "webgl2/haylen.wasm")}
    config = {"name": app.name, "identifier": app.identifier, "transparent": app.transparent, "splash": splash, "sizes": sizes, "plugins": write_web_plugins(app, site)}
    if release is not None:
        config["release"] = {"files": release}
    (site / "config.json").write_text(json.dumps(config, indent=4) + "\n", encoding="utf-8")


RELEASE_WEB_PROJECT = """# Written by haylen.py for the release build of the app: the runtime of the page with the bootstrap of the app, linked against the web SDK of the prebuilt engine.
cmake_minimum_required(VERSION 3.28)
project(haylen_release LANGUAGES C CXX)
find_package(haylen REQUIRED CONFIG)
add_executable(haylen_release {bootstrap} "${{HAYLEN_ENGINE_DIR}}/src/platform/sokol/LuaPlayer.cpp")
target_link_libraries(haylen_release PRIVATE haylen::runtime)
set_target_properties(haylen_release PROPERTIES OUTPUT_NAME "haylen")
# The names of the functions stay in a symbol map next to the build for crash reports, and the site gets none.
target_link_options(haylen_release PRIVATE --emit-symbol-map)
"""


def build_release_web(app: App, site: Path, args: argparse.Namespace) -> list[str]:
    """Places the protected release of an app in `app/` of its site with a runtime of its own for each backend, linked against the web SDK with the bootstrap of the app, and returns the files that download before the app starts: the manifests and the app domain, while the shards of the content stay on the site and stream by range."""
    release = build_release(app, CONTENT_PROFILES["web"], args.engine_config, args.jobs)
    shutil.copytree(release_folder(app, CONTENT_PROFILES["web"]), site / "app")
    tool = content_tool(args.engine_config, args.jobs)
    boot = app.build_folder / "release" / "web.boot"
    shutil.rmtree(boot, ignore_errors=True)
    run([tool, "stage", release_folder(app, CONTENT_PROFILES["web"]), "--keys", ensure_keys(app, tool), "--output", boot, "--app-only"])
    files = sorted(path.name for path in boot.iterdir())
    shutil.rmtree(boot)

    project = app.build_folder / "release-web"
    project.mkdir(parents=True, exist_ok=True)
    write_bootstrap(app, CONTENT_PROFILES["web"], project / BOOTSTRAP_SOURCE, args.engine_config, args.jobs)
    write_if_changed(project / "CMakeLists.txt", RELEASE_WEB_PROJECT.format(bootstrap=BOOTSTRAP_SOURCE))
    for backend in ("webgpu", "webgl2"):
        build = project / f"build-{backend}"
        terminal.step(f'Linking the release runtime of `{shown_path(app.folder)}` for "{backend}"')
        run([ensure_emsdk(), "cmake", "-S", project, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", f"-Dhaylen_DIR={web_sdk(backend) / 'lib' / 'cmake' / 'haylen'}"])
        run(["cmake", "--build", build, "--config", "Release", "--parallel", str(args.jobs)])
        destination = site / backend
        shutil.rmtree(destination, ignore_errors=True)
        destination.mkdir()
        for name in ("haylen.js", "haylen.wasm"):
            shutil.copy2(build / name, destination)
        for name in (WEB_AUDIO_WORKLET, WEB_CONTENT_STORE):
            shutil.copy2(web_sdk(backend) / "share" / "haylen" / "platform" / "web" / name, destination)
    return files


def prepare_web(app: App, args: argparse.Namespace) -> Path:
    """Makes the site of an app again: the files of `platform/web` of the app, or of the template, as they are, and next to them the runtimes and the generated files. The Release configuration ships the protected release with runtimes linked for the app, and development the prebuilt players with the package as it is."""
    own = app.folder / "platform" / "web"
    site = app.build_folder / "web"
    terminal.step(f"Preparing the site `{shown_path(site)}`")
    shutil.rmtree(site, ignore_errors=True)
    copy_files(project_files(own) if own.is_dir() else layered_files(platform_layers(app_kind(app.folder), "web")), site)
    release = None
    if args.config == "Release":
        release = build_release_web(app, site, args)
    else:
        for backend in ("webgpu", "webgl2"):
            shutil.copytree(ARTIFACTS_DIR / "web" / backend, site / backend)
    write_web_settings(app, site, release)
    return site


def check_web(app: App, site: Path, args: argparse.Namespace) -> list[Requirement]:
    """Checks the site of an app: every plugin with a web part in `config.json` with its module, plugins whose screens open popups on a page that the opener policy `same-origin` cuts off from them, and in the Release configuration anything a release never holds, such as Lua text, files of the package as they are, a zipped package, source maps or a key of the app."""
    if not (site / "config.json").is_file():
        raise BuildError(f'The site of `{shown_path(app.folder)}` was not made yet. Make it with "{TOOL} prepare {shown_path(app.folder)} --platform web".')
    listed = {entry["id"]: entry for entry in json.loads((site / "config.json").read_text(encoding="utf-8")).get("plugins", [])}
    missing: list[Requirement] = []
    if args.config == "Release":
        missing += [Requirement(problem, RELEASE_ADVICE) for problem in release_problems(app, folder_entries(site), "app", read_secrets(app))]
        missing += [Requirement(f'The site holds the zipped package "{path.relative_to(site).as_posix()}", while a release ships only its protected release.', RELEASE_ADVICE) for path in site.rglob("*.zip")]
    for plugin in app.plugins:
        if "web" not in plugin.manifest:
            continue
        owner = f'The plugin "{plugin.id}"'
        entry = listed.get(plugin.id)
        if entry is None or not (site / entry["module"]).is_file():
            missing.append(Requirement(f'{owner} needs its web module in the site, which "config.json" does not list.', f'Make the site again with "{TOOL} prepare {shown_path(app.folder)} --platform web".'))
        # Popups of screens are the one feature of plugins that the opener policy breaks, and a module registers its screens by name.
        screens = any("registerScreen(" in path.read_text(encoding="utf-8") for path in (plugin.folder / "web").rglob("*") if path.suffix in (".js", ".mjs"))
        if screens and args.coop == "same-origin":
            missing.append(Requirement(f'{owner} opens screens, whose popups the header "Cross-Origin-Opener-Policy: same-origin" cuts off from the app.', 'Serve the page with "--coop same-origin-allow-popups" or "--coop off".'))
    return missing


def start_web_development(app: App, site: Path) -> DevelopmentServer:
    """Starts the development server of a site that `prepare_web` made and writes its entry into `config.json`, so the page runs the app in development and connects to it. The manifest of the server holds the files of `app.zip`, so a page that loads late gets only what changed since."""
    with zipfile.ZipFile(site / "app.zip") as archive:
        manifest = {name: hashlib.sha256(archive.read(name)).hexdigest() for name in archive.namelist()}
    development = DevelopmentServer(app.folder, manifest)
    config = json.loads((site / "config.json").read_text(encoding="utf-8"))
    config["development"] = {"path": DEVELOPMENT_PATH, "token": development.token}
    (site / "config.json").write_text(json.dumps(config, indent=4) + "\n", encoding="utf-8")
    return development


# Apps that a launch starts on a device, a simulator, an emulator or this Mac in the Debug configuration connect to a development server of their own with "--dev-server", so every file saved in the app reloads in them as it does in the player and in the pages of the web.
REMOTE_PLATFORMS = {"ios", "tvos"}
ANDROID_DEVELOPMENT_EXTRA = "dev.haylen.developmentServer"


def lan_address() -> str:
    """Returns the address of this machine on the network that reaches other networks, which devices on the same network connect to. Connecting a UDP socket sends nothing."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect(("192.0.2.1", 9))
        return probe.getsockname()[0]


def development_address(platform_name: str, host: str, port: int, token: str) -> tuple[str, str]:
    """Returns the address the development server of a launch listens on and the WebSocket address the app connects to. The emulator and devices of Android reach this machine through "adb reverse", and simulators and this Mac share its loopback address, while an Apple device reaches it over the network, on every address of this machine unless "--host" names one."""
    remote = platform_name in REMOTE_PLATFORMS
    listen = "0.0.0.0" if remote and host == "127.0.0.1" else host
    reached = listen if listen != "0.0.0.0" else lan_address() if remote else "127.0.0.1"
    return listen, f"ws://{reached}:{port}{DEVELOPMENT_PATH}?token={token}"


def start_console_input(development: DevelopmentServer) -> None:
    """Reads the lines the developer types in an interactive terminal and sends them to the developer console of the connected apps."""
    if sys.stdin.isatty():
        threading.Thread(target=development.read_lines, args=(sys.stdin,), daemon=True).start()


class DevelopmentHandler(http.server.BaseHTTPRequestHandler):
    """Serves the development path alone, for apps that run outside a browser."""

    development: DevelopmentServer

    def do_GET(self) -> None:
        if urllib.parse.urlsplit(self.path).path != DEVELOPMENT_PATH:
            self.send_error(404, "The development server only serves its development path.")
            return
        self.development.accept(self)

    def log_message(self, format: str, *arguments) -> None:
        pass


@contextlib.contextmanager
def device_development(app: App, package: Path, args: argparse.Namespace, device: str | None = None) -> Iterator[list[str]]:
    """Runs the development server of an app that a launch starts in the Debug configuration, and yields the arguments of the command line that connect the app to it, which are none in the Release configuration. The manifest of the server holds the files of the package as the build ships them, so the app catches up on what changed since."""
    if args.config == "Release":
        yield []
        return
    manifest = DevelopmentServer.hash_files(package, [path.relative_to(package).as_posix() for path in sorted(package.rglob("*")) if path.is_file()])
    development = DevelopmentServer(app.folder, manifest)
    listen, address = development_address(args.platform, args.host, args.port, development.token)
    handler = type("Handler", (DevelopmentHandler,), {"development": development})
    try:
        server = http.server.ThreadingHTTPServer((listen, args.port), handler)
    except OSError as error:
        raise BuildError(f'The development server cannot listen on port {args.port} of "{listen}": {error.strerror}. Stop what uses the port, or pick another one with "--port".') from error
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    stop = threading.Event()
    threading.Thread(target=development.watch, args=(stop,), daemon=True).start()
    start_console_input(development)
    if device is not None:
        run([adb(), "-s", device, "reverse", f"tcp:{args.port}", f"tcp:{args.port}"])
    terminal.info("The app runs in development, and every file saved in the app reloads in it. Lines typed here run in its developer console.")
    try:
        yield ["--dev-server", address]
    finally:
        stop.set()
        server.shutdown()
        server.server_close()
        if device is not None:
            subprocess.run([str(adb()), "-s", device, "reverse", "--remove", f"tcp:{args.port}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)


def run_web(app: App, site: Path, args: argparse.Namespace) -> None:
    """Serves the site, in development with the Debug configuration, where the development server pushes every saved file of the app to the open pages, and as it ships with the Release configuration."""
    report_requirements(check_web(app, site, args))
    if args.config == "Release":
        serve(site, args.host, args.port, args.coep, args.coop, args.open)
        return
    development = start_web_development(app, site)
    stop = threading.Event()
    threading.Thread(target=development.watch, args=(stop,), daemon=True).start()
    start_console_input(development)
    terminal.info("The page runs in development, and every file saved in the app reloads in the open pages. Lines typed here run in their developer console.")
    try:
        serve(site, args.host, args.port, args.coep, args.coop, args.open, development)
    finally:
        stop.set()


# Windows and Linux: the CMake project of `platform/windows` or `platform/linux` includes `haylen/haylen.cmake`, which brings the SDK of the desktop artifacts, the package, the native libraries and the values of `app.json`, and haylen.py builds it in place and installs the app into the build folder of the app.

# Where the installed app keeps its executable, its package and its native libraries: next to each other on Windows, and in `lib/<executable>` on Linux, where `bin/<executable>` links to the executable.
DESKTOP_LAYOUTS = {"windows": ("", "{executable}.exe"), "linux": ("lib/{executable}", "bin/{executable}")}
# The folders next to the executable of a Linux app, whose names an executable without an extension cannot take.
LINUX_APP_FOLDERS = ("app", "lib")
# What the engine needs from the application manifest of a Windows executable, each with the text that shows it in the manifest and the snippet that adds it.
WINDOWS_MANIFEST_NEEDS = {
    "the Common Controls of version 6, which the message dialogs of Haylen need": ("Microsoft.Windows.Common-Controls", '<dependency>\n  <dependentAssembly>\n    <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>\n  </dependentAssembly>\n</dependency>'),
    "UTF-8 as the code page of the process, which the text of Haylen needs": (">UTF-8</activeCodePage>", '<activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>'),
}
WINDOWS_MANIFEST_RESOURCE = 24


def cmake_text(value: str) -> str:
    """Writes a value as a bracket argument of CMake, which keeps every character as it is."""
    level = 0
    while f"]{'=' * level}]" in value:
        level += 1
    return f"[{'=' * level}[{value}]{'=' * level}]"


def desktop_cmake(platform: str, engine_config: str, values: dict[str, str]) -> str:
    """Writes `haylen/haylen.cmake`: the values of `app.json`, the SDK of the desktop artifacts, and `haylen_app`, which makes a target the executable of the app, with the Lua player of the engine and the bootstrap of the app."""
    lines = [
        "# Written by haylen.py from app.json, the native libraries of the app and the engine artifacts. The file CMakeLists.txt of the project includes it before its targets.",
        *(f"set({name} {cmake_text(value)})" for name, value in values.items()),
        'set(HAYLEN_PACKAGE_DIR "${CMAKE_CURRENT_LIST_DIR}/app")',
        'set(HAYLEN_NATIVE_DIR "${CMAKE_CURRENT_LIST_DIR}/native")',
        f"find_package(haylen REQUIRED CONFIG PATHS {cmake_text(desktop_sdk().as_posix())} NO_DEFAULT_PATH)",
        "",
        "# Makes a target the executable of the app, the Lua player of the engine with the bootstrap of the app. The executable of the build tree plays the package and loads the native libraries next to it, as the installed app does.",
        "function(haylen_app target)",
        '  target_sources(${target} PRIVATE "${HAYLEN_ENGINE_DIR}/src/platform/sokol/LuaPlayer.cpp" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/HaylenBootstrap.cpp")',
        "  target_link_libraries(${target} PRIVATE haylen::runtime)",
        '  set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")',
        '  set(package "${CMAKE_BINARY_DIR}/bin/app")',
        '  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")',
    ]
    if platform == "windows":
        runtime = "MultiThreadedDebugDLL" if engine_config == "Debug" else "MultiThreadedDLL"
        lines += [
            f"  # The engine links the C runtime of its {engine_config} build, so every configuration of the app does too.",
            f"  set_target_properties(${{target}} PROPERTIES MSVC_RUNTIME_LIBRARY {runtime})",
            "  # Windows links folders as junctions, which need no privileges.",
            '  if(NOT EXISTS "${package}" AND CMAKE_HOST_WIN32)',
            '    file(TO_NATIVE_PATH "${package}" link)',
            '    file(TO_NATIVE_PATH "${HAYLEN_PACKAGE_DIR}" original)',
            '    execute_process(COMMAND cmd.exe /c mklink /J "${link}" "${original}" OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)',
            '  elseif(NOT EXISTS "${package}")',
            '    file(CREATE_LINK "${HAYLEN_PACKAGE_DIR}" "${package}" SYMBOLIC)',
            "  endif()",
            '  add_custom_target(${target}_native ALL COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${HAYLEN_NATIVE_DIR}" "${CMAKE_BINARY_DIR}/bin" VERBATIM)',
        ]
    else:
        lines += [
            '  set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN;$ORIGIN/lib" INSTALL_RPATH "$ORIGIN;$ORIGIN/lib")',
            '  target_link_options(${target} PRIVATE "LINKER:--enable-new-dtags" "$<$<CONFIG:Release>:LINKER:--strip-all>")',
            '  if(NOT EXISTS "${package}")',
            '    file(CREATE_LINK "${HAYLEN_PACKAGE_DIR}" "${package}" SYMBOLIC)',
            '    file(CREATE_LINK "${HAYLEN_NATIVE_DIR}" "${CMAKE_BINARY_DIR}/bin/lib" SYMBOLIC)',
            "  endif()",
        ]
    return "\n".join([*lines, "endfunction()", ""])


def rc_text(value: str) -> str:
    """Writes a value as a string of a resource script, where a quote doubles and a backslash starts an escape."""
    return '"' + value.replace("\\", "\\\\").replace('"', '""').replace("\n", "\\n") + '"'


def write_windows_resources(app: App, generated: Path) -> None:
    """Writes `HaylenApp.h`, which gives the resource script of the project the name, the version, the company and the copyright of `app.json` for the version resource of the executable."""
    numbers = [*app.version.split("."), "0", "0", "0"][:4]
    strings = {"NAME": app.name, "DESCRIPTION": app.description, "COMPANY": app.company, "COPYRIGHT": app.copyright, "EXECUTABLE": f"{app.executable}.exe", "VERSION": app.version}
    lines = [
        "// Written by haylen.py from app.json. The resource script of the project includes it.",
        *(f"#define HAYLEN_APP_{name} {rc_text(value)}" for name, value in strings.items()),
        f"#define HAYLEN_APP_VERSION_NUMBERS {', '.join(numbers)}",
        "",
    ]
    write_if_changed(generated / "HaylenApp.h", "\n".join(lines))


def desktop_entry_text(value: str) -> str:
    """Escapes a value of a desktop entry, whose strings take backslash escapes."""
    return value.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")


def complete_desktop_entry(text: str, generated: dict[str, str], where: str, warnings: list[str]) -> str:
    """Completes the `[Desktop Entry]` group of a desktop entry of the developer with the generated keys it lacks, where the keys of the developer win and one that differs stays with a warning."""
    lines = text.splitlines()
    group = next((index for index, line in enumerate(lines) if line.strip() == "[Desktop Entry]"), None)
    if group is None:
        raise BuildError(f'The desktop entry `{where}` has no "[Desktop Entry]" group. Restore it from the template.')
    end = next((index for index in range(group + 1, len(lines)) if lines[index].startswith("[")), len(lines))
    while end > group + 1 and not lines[end - 1].strip():
        end -= 1
    own = {line.split("=", 1)[0].strip(): line.split("=", 1)[1].strip() for line in lines[group + 1 : end] if "=" in line and not line.lstrip().startswith("#")}
    added = []
    for key, value in generated.items():
        if key not in own:
            added.append(f"{key}={desktop_entry_text(value)}")
        elif own[key] != desktop_entry_text(value):
            warnings.append(f'The key "{key}" of `{where}` keeps its value "{own[key]}", while "app.json" gives "{desktop_entry_text(value)}".')
    return "\n".join(["# Written by haylen.py from app.json and the desktop entry of the project.", *lines[:end], *added, *lines[end:]]) + "\n"


def complete_metainfo(text: str, generated: list[xml.etree.ElementTree.Element], where: str, warnings: list[str]) -> str:
    """Completes the AppStream metainfo of the developer with the generated elements of `component` it lacks, where an element of the developer wins and one whose text differs stays with a warning."""
    try:
        component = xml.etree.ElementTree.fromstring(text)
    except xml.etree.ElementTree.ParseError as error:
        raise BuildError(f"The AppStream metainfo `{where}` is not valid XML: {error}.") from error
    if component.tag != "component":
        raise BuildError(f'The AppStream metainfo `{where}` must hold one "component" element. Restore it from the template.')
    added = 0
    for element in generated:
        own = component.find(element.tag)
        if own is None:
            component.insert(added, element)
            added += 1
        elif own.text is not None and element.text is not None and own.text.strip() != element.text.strip():
            warnings.append(f'The element "{element.tag}" of `{where}` keeps its value "{own.text.strip()}", while "app.json" gives "{element.text.strip()}".')
    xml.etree.ElementTree.indent(component, "  ")
    header = "<!-- Written by haylen.py from app.json and the AppStream metainfo of the project. -->"
    return f'<?xml version="1.0" encoding="UTF-8"?>\n{header}\n{xml.etree.ElementTree.tostring(component, encoding="unicode")}\n'


def appstream_elements(app: App) -> list[xml.etree.ElementTree.Element]:
    """The elements of the AppStream metainfo that come from `app.json`, in the order the specification lists them. The developer id is the identifier of the app without its last part, the domain of its publisher."""
    def element(tag: str, text: str | None = None, **attributes: str) -> xml.etree.ElementTree.Element:
        created = xml.etree.ElementTree.Element(tag, attributes)
        created.text = text
        return created

    description = element("description")
    description.append(element("p", app.description))
    developer = element("developer", id=app.identifier.rsplit(".", 1)[0])
    developer.append(element("name", app.company))
    elements = [element("id", app.identifier), element("name", app.name), element("summary", app.description), description, developer, element("launchable", f"{app.identifier}.desktop", type="desktop-id")]
    if app.website:
        elements.append(element("url", app.website, type="homepage"))
    return elements


def write_linux_entries(app: App, root: Path, generated: Path) -> None:
    """Writes `app.desktop` and `app.metainfo.xml`, the desktop entry and the AppStream metainfo of the project completed with the name, the description, the executable, the icon, the window class, the publisher and the website of `app.json`."""
    warnings: list[str] = []
    entry = root / "resources" / "app.desktop"
    metainfo = root / "resources" / "app.metainfo.xml"
    for path in (entry, metainfo):
        if not path.is_file():
            raise BuildError(f'The Linux project `{shown_path(root)}` has no `{shown_path(path)}`, which haylen.py completes with the values of "app.json". Restore it from the template, which "{TOOL} platform diff {shown_path(app.folder)} --template linux" shows.')
    keys = {"Name": app.name, "Comment": app.description, "Exec": app.executable, "Icon": app.identifier, "StartupWMClass": app.identifier}
    write_if_changed(generated / "app.desktop", complete_desktop_entry(entry.read_text(encoding="utf-8"), keys, shown_path(entry), warnings))
    write_if_changed(generated / "app.metainfo.xml", complete_metainfo(metainfo.read_text(encoding="utf-8"), appstream_elements(app), shown_path(metainfo), warnings))
    for warning in warnings:
        terminal.warning(warning)


def prepare_desktop(app: App, args: argparse.Namespace) -> Path:
    """Writes the folder `haylen/` of a Windows or Linux project: `haylen.cmake`, the package as it is with the development bootstrap or the protected release with the bootstrap of the app, the native libraries of this machine, and the version resource of Windows or the completed desktop entry and AppStream metainfo of Linux."""
    require_host(args.platform)
    if args.platform == "linux" and app.executable in LINUX_APP_FOLDERS:
        raise BuildError(f'The app folder `{shown_path(app.folder)}` gives the executable "{app.executable}", which is the name of a folder next to the executable of a Linux app. Rename the app folder.')
    root = project_root(app, args.platform)
    generated = root / GENERATED_FOLDER
    terminal.step(f"Preparing the {args.platform.capitalize()} project `{shown_path(root)}`")
    for name in ("app", "native"):
        shutil.rmtree(generated / name, ignore_errors=True)
    if args.config == "Release":
        shutil.copytree(build_release(app, CONTENT_PROFILES[args.platform], args.engine_config, args.jobs), generated / "app")
        write_bootstrap(app, CONTENT_PROFILES[args.platform], generated / BOOTSTRAP_SOURCE, args.engine_config, args.jobs)
    else:
        copy_package(app, generated / "app")
        write_if_changed(generated / BOOTSTRAP_SOURCE, development_bootstrap("defined(NDEBUG)"))
    (generated / "native").mkdir(parents=True)
    prepare_host_native(app, generated / "native", args.jobs)

    values = {"HAYLEN_APP_NAME": app.name, "HAYLEN_APP_IDENTIFIER": app.identifier, "HAYLEN_APP_VERSION": app.version, "HAYLEN_APP_DESCRIPTION": app.description, "HAYLEN_APP_COMPANY": app.company, "HAYLEN_APP_COPYRIGHT": app.copyright, "HAYLEN_APP_WEBSITE": app.website, "HAYLEN_APP_EXECUTABLE": app.executable}
    if args.platform == "windows":
        write_windows_resources(app, generated)
    else:
        write_linux_entries(app, root, generated)
        values |= {"HAYLEN_DESKTOP_ENTRY": (generated / "app.desktop").as_posix(), "HAYLEN_METAINFO": (generated / "app.metainfo.xml").as_posix()}
    write_if_changed(generated / "haylen.cmake", desktop_cmake(args.platform, args.engine_config, values))
    return root


def desktop_folder(app: App, platform: str) -> Path:
    """The folder that the last Windows or Linux build of an app installs into, which is the app as it ships, apart from the copy of the template that an app without a project of its own builds."""
    return app.build_folder / "installed" / platform


def build_desktop(app: App, root: Path, args: argparse.Namespace) -> Path:
    """Builds a Windows or Linux project with CMake into the build folder of the app, outside the project, installs the app into its own folder there and returns the folder of the build."""
    build = app.build_folder / "cmake" / f"{args.platform}-{args.config.lower()}"
    # A build tree belongs to one project, so a project of the developer that replaces the copy of the template builds in a new tree.
    if (build / "CMakeCache.txt").is_file() and cmake_cache_value(build, "CMAKE_HOME_DIRECTORY") != root.as_posix():
        shutil.rmtree(build)
    terminal.step(f'Building the {args.platform.capitalize()} project `{shown_path(root)}` in the "{args.config}" configuration')
    run(["cmake", "-S", root, "-B", build, *cmake_generator(), f"-DCMAKE_BUILD_TYPE={args.config}"])
    run(["cmake", "--build", build, "--config", args.config, "--parallel", str(args.jobs)])
    folder = desktop_folder(app, args.platform)
    shutil.rmtree(folder, ignore_errors=True)
    run(["cmake", "--install", build, "--config", args.config, "--prefix", folder])
    return build


def windows_resources(data: bytes, kind: int) -> list[bytes]:
    """Reads the resources of one type from a Windows executable: the data of every name and language under the type in the resource directory of the PE file, or nothing when the file has none."""
    if data[:2] != b"MZ":
        return []
    header = int.from_bytes(data[0x3C:0x40], "little")
    if data[header : header + 4] != b"PE\0\0":
        return []
    sections, optional_size = int.from_bytes(data[header + 6 : header + 8], "little"), int.from_bytes(data[header + 20 : header + 22], "little")
    optional = header + 24
    directories = optional + (112 if int.from_bytes(data[optional : optional + 2], "little") == 0x20B else 96)
    root_rva = int.from_bytes(data[directories + 16 : directories + 20], "little")
    table = optional + optional_size
    mapped = []
    for index in range(sections):
        entry = table + index * 40
        size, address, raw_size, raw = (int.from_bytes(data[entry + offset : entry + offset + 4], "little") for offset in (8, 12, 16, 20))
        mapped.append((address, max(size, raw_size), raw))

    def offset(rva: int) -> int:
        return next(raw + rva - address for address, size, raw in mapped if address <= rva < address + size)

    def entries(directory: int) -> list[tuple[int, int]]:
        count = int.from_bytes(data[directory + 12 : directory + 14], "little") + int.from_bytes(data[directory + 14 : directory + 16], "little")
        return [(int.from_bytes(data[directory + 16 + index * 8 : directory + 20 + index * 8], "little"), int.from_bytes(data[directory + 20 + index * 8 : directory + 24 + index * 8], "little")) for index in range(count)]

    if not root_rva:
        return []
    root = offset(root_rva)
    found = []
    for name, child in entries(root):
        if name != kind or not child & 0x80000000:
            continue
        for _, language_table in entries(root + (child & 0x7FFFFFFF)):
            for _, leaf in entries(root + (language_table & 0x7FFFFFFF)):
                rva, size = int.from_bytes(data[root + leaf : root + leaf + 4], "little"), int.from_bytes(data[root + leaf + 4 : root + leaf + 8], "little")
                found.append(data[offset(rva) : offset(rva) + size])
    return found


def check_desktop(app: App, root: Path, args: argparse.Namespace) -> list[Requirement]:
    """Checks the folder of the last Windows or Linux build: the executable, the package and the native libraries where the engine finds them, the application manifest of Windows, the desktop entry of Linux and, in the Release configuration, what a release never ships."""
    folder = desktop_folder(app, args.platform)
    if not folder.is_dir():
        raise BuildError(f'No app of `{shown_path(app.folder)}` was built for "{args.platform}". Build it with "{TOOL} run {shown_path(app.folder)} --platform {args.platform} --config {args.config}".')
    place, executable = (part.format(executable=app.executable) for part in DESKTOP_LAYOUTS[args.platform])
    home = folder / place
    project = f'`{shown_path(root / "CMakeLists.txt")}`'
    missing: list[Requirement] = []
    if not (folder / executable).is_file():
        missing.append(Requirement(f'Haylen needs the executable "{executable}" in the built app, which it lacks.', f'Keep "haylen_app", the "OUTPUT_NAME" of "HAYLEN_APP_EXECUTABLE" and the install of the executable in {project}.'))
    if args.config != "Release" and not (home / "app" / "app.json").is_file():
        missing.append(Requirement(f'Haylen needs the package of the app in "{(Path(place) / "app").as_posix()}" of the built app, which it lacks.', f'Keep the install of "HAYLEN_PACKAGE_DIR" in {project}.'))
    native = root / GENERATED_FOLDER / "native"
    libraries = home / ("lib" if args.platform == "linux" else "")
    for library in sorted(native.iterdir()) if native.is_dir() else []:
        if not (libraries / library.name).exists():
            missing.append(Requirement(f'The app needs the native library "{library.name}" next to its executable, which the built app lacks.', f'Keep the install of "HAYLEN_NATIVE_DIR" in {project}.'))

    if args.platform == "windows" and (folder / executable).is_file():
        manifests = windows_resources((folder / executable).read_bytes(), WINDOWS_MANIFEST_RESOURCE)
        manifest = b"".join(manifests).decode("utf-8", "replace")
        for need, (marker, snippet) in WINDOWS_MANIFEST_NEEDS.items():
            if marker not in manifest:
                missing.append(Requirement(f"The application manifest of the built executable lacks {need}.", f'Add to `{shown_path(root / "resources" / "app.manifest")}`:', snippet))
    if args.platform == "linux" and not (folder / "share" / "applications" / f"{app.identifier}.desktop").is_file():
        missing.append(Requirement(f'Haylen needs the desktop entry "share/applications/{app.identifier}.desktop" in the built app, which it lacks, since Linux desktops find the name and the icon of the window of the app by the identifier.', f'Keep the install of "HAYLEN_DESKTOP_ENTRY" with the name of "HAYLEN_APP_IDENTIFIER" in {project}.'))
    if args.config == "Release":
        missing += [Requirement(problem, RELEASE_ADVICE) for problem in release_problems(app, folder_entries(folder), (Path(place) / "app").as_posix(), read_secrets(app))]
    return missing


def run_desktop(app: App, root: Path, args: argparse.Namespace) -> None:
    """Builds and installs a Windows or Linux app, checks it and runs its executable, which connects to the development server in the Debug configuration."""
    build_desktop(app, root, args)
    requirements = check_desktop(app, root, args)
    stop_on_release_problems(requirements)
    report_requirements(requirements)
    executable = desktop_folder(app, args.platform) / DESKTOP_LAYOUTS[args.platform][1].format(executable=app.executable)
    with device_development(app, root / GENERATED_FOLDER / "app", args) as arguments:
        run([executable, *arguments], cwd=executable.parent)


def ensure_appimage_tools() -> dict[str, Path]:
    """Returns the pinned appimagetool, AppImage runtime and patchelf that the AppImage generator of CPack runs, with appimagetool unpacked from its own AppImage, so it runs without FUSE."""
    arch = {"x64": "x86_64", "arm64": "aarch64"}[host_arch()]
    folder = TOOLS_DIR / f"appimage-{APPIMAGETOOL_VERSION}"
    tools = {"appimagetool": folder / "appimagetool" / "AppRun", "runtime": folder / f"runtime-{APPIMAGE_RUNTIME_VERSION}-{arch}", "patchelf": folder / "bin" / "patchelf"}
    if all(path.exists() for path in tools.values()):
        return tools

    require_host("linux")
    hashes = APPIMAGE_TOOLS_SHA256[arch]
    shutil.rmtree(folder, ignore_errors=True)
    image = folder / f"appimagetool-{arch}.AppImage"
    download(f"https://github.com/AppImage/appimagetool/releases/download/{APPIMAGETOOL_VERSION}/appimagetool-{arch}.AppImage", image, hashes["appimagetool"])
    image.chmod(0o755)
    capture([image, "--appimage-extract"], cwd=folder)
    (folder / "squashfs-root").rename(folder / "appimagetool")
    image.unlink()
    download(f"https://github.com/AppImage/type2-runtime/releases/download/{APPIMAGE_RUNTIME_VERSION}/runtime-{arch}", tools["runtime"], hashes["runtime"])
    archive = folder / "patchelf.tar.gz"
    download(f"https://github.com/NixOS/patchelf/releases/download/{PATCHELF_VERSION}/patchelf-{PATCHELF_VERSION}-{arch}.tar.gz", archive, hashes["patchelf"])
    with tarfile.open(archive) as package:
        package.extract("./bin/patchelf", folder, filter="data")
    archive.unlink()
    return tools


def ensure_inno_setup() -> Path:
    """Returns the compiler of the pinned Inno Setup, which its installer puts into `.tools` in portable mode, without an uninstaller or entries in the registry."""
    target = TOOLS_DIR / f"inno-setup-{INNO_SETUP_VERSION}" / "ISCC.exe"
    if target.exists():
        return target

    require_host("windows")
    installer = TOOLS_DIR / f"innosetup-{INNO_SETUP_VERSION}-x64.exe"
    download(f"https://github.com/jrsoftware/issrc/releases/download/is-{INNO_SETUP_VERSION.replace('.', '_')}/innosetup-{INNO_SETUP_VERSION}-x64.exe", installer, INNO_SETUP_SHA256)
    run([installer, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER", "/PORTABLE=1", f"/DIR={target.parent}"])
    installer.unlink()
    return target


def command_dist(args: argparse.Namespace) -> None:
    """Builds the packages that ship a Windows or Linux app in the Release configuration: the installer of Windows, or the `.deb` and the AppImage of Linux, which the CPack configuration of the project describes."""
    app = resolve_lua_app(args.app)
    compile_app_shaders(app)
    args.config = "Release"
    info = App(app, RUN_TARGETS[args.platform].plugins)
    ensure_artifacts("desktop", args.engine_config, args.jobs)
    root = prepare_desktop(info, args)
    build = build_desktop(info, root, args)
    requirements = check_desktop(info, root, args)
    stop_on_release_problems(requirements)
    report_requirements(requirements)

    tools = []
    if args.platform == "windows":
        tools = [f"-DCPACK_INNOSETUP_EXECUTABLE={ensure_inno_setup()}"]
    else:
        appimage = ensure_appimage_tools()
        tools = [f"-DCPACK_APPIMAGE_TOOL_EXECUTABLE={appimage['appimagetool']}", f"-DCPACK_APPIMAGE_PATCHELF_EXECUTABLE={appimage['patchelf']}", f"-DCPACK_APPIMAGE_RUNTIME_FILE={appimage['runtime']}"]
    staging = info.build_folder / "packages" / args.platform
    shutil.rmtree(staging, ignore_errors=True)
    terminal.step(f"Packaging `{shown_path(app)}` for {args.platform.capitalize()} with CPack")
    run(["cpack", "--config", build / "CPackConfig.cmake", "-C", "Release", "-B", staging, *tools])

    output = Path(args.output).expanduser().resolve() if args.output else app / "dist"
    output.mkdir(parents=True, exist_ok=True)
    packages = [path for path in staging.iterdir() if path.is_file() and path.suffix in {".exe", ".deb", ".AppImage"}]
    for package in packages:
        shutil.copy2(package, output / package.name)
        terminal.success(f"Packaged `{shown_path(output / package.name)}`.")


def folder_entries(folder: Path) -> list[tuple[str, int, Callable[[], bytes]]]:
    """Lists every file of a built app folder or bundle with its size and the function that reads it."""
    return [(path.relative_to(folder).as_posix(), path.stat().st_size, path.read_bytes) for path in sorted(folder.rglob("*")) if path.is_file()]


@dataclasses.dataclass(frozen=True)
class RunTarget:
    """A platform that `haylen.py run` builds for: the platform template of its project, the engine artifacts it needs, the plugin platform whose parameters it checks, the function that prepares its project or site, the one that builds and launches it and the one that checks what the built app lacks."""

    template: str
    artifacts: str
    plugins: str
    prepare: Callable[[App, argparse.Namespace], Path]
    run: Callable[[App, Path, argparse.Namespace], None]
    check: Callable[[App, Path, argparse.Namespace], list[Requirement]] | None


# A new platform is a folder under `platform` of a template layer and an entry here.
RUN_TARGETS = {
    "macos": RunTarget("apple", "apple", "macos", prepare_apple_run, run_apple, check_apple),
    "catalyst": RunTarget("apple", "apple", "catalyst", prepare_apple_run, run_apple, check_apple),
    "ios": RunTarget("apple", "apple", "ios", prepare_apple_run, run_apple, check_apple),
    "ios-simulator": RunTarget("apple", "apple", "ios", prepare_apple_run, run_apple, check_apple),
    "tvos": RunTarget("apple", "apple", "tvos", prepare_apple_run, run_apple, check_apple),
    "tvos-simulator": RunTarget("apple", "apple", "tvos", prepare_apple_run, run_apple, check_apple),
    "android": RunTarget("android", "android", "android", prepare_android_run, run_android, check_android),
    "web": RunTarget("web", "web", "web", prepare_web, run_web, check_web),
    "windows": RunTarget("windows", "desktop", "windows", prepare_desktop, run_desktop, check_desktop),
    "linux": RunTarget("linux", "desktop", "linux", prepare_desktop, run_desktop, check_desktop),
}


def command_run(args: argparse.Namespace) -> None:
    app = resolve_lua_app(args.app)
    compile_app_shaders(app)
    if args.platform is None:
        # The player of this machine runs the package folder in development mode, which reloads edited files, while changed shaders compile again in the background.
        info = App(app, host_name())
        build = build_options(host_name(), args.engine_config, "haylen", args.jobs)
        command_build(build)
        # The native libraries of the app wait in a folder of their own, which the player searches first.
        native = info.build_folder / "native" / "development"
        shutil.rmtree(native, ignore_errors=True)
        native_options = ["--native", native] if prepare_host_native(info, native, args.jobs) else []
        stop = threading.Event()
        threading.Thread(target=watch_app_shaders, args=(app, stop), daemon=True).start()
        terminal.step(f"Running `{shown_path(app)}` in the player with hot reload, until the app quits or Ctrl+C stops it")
        try:
            run([build_dir(build.platform, build.config) / "bin" / "haylen" / executable_name("haylen"), "--dev", *native_options, app])
        finally:
            stop.set()
        return

    target = RUN_TARGETS[args.platform]
    info = App(app, target.plugins)
    ensure_artifacts(target.artifacts, args.engine_config, args.jobs)
    target.run(info, target.prepare(info, args), args)


def command_prepare(args: argparse.Namespace) -> None:
    """Writes the generated folder `haylen/` of the project of a platform, or the folder of the site or the desktop app, and nothing else, so the project builds in Xcode, Android Studio or its own tools."""
    app = resolve_lua_app(args.app)
    compile_app_shaders(app)
    target = RUN_TARGETS[args.platform]
    info = App(app, target.plugins)
    ensure_artifacts(target.artifacts, args.engine_config, args.jobs)
    terminal.success(f'Prepared `{shown_path(target.prepare(info, args))}` for "{args.platform}".')


def command_xcodegen(args: argparse.Namespace) -> None:
    """Writes `haylen/` of the Apple project of an app and generates its `App.xcodeproj` again from `project.yml` with the pinned XcodeGen, or generates the project of the Apple template again."""
    if args.template:
        generate_template_project(args)
        return
    app = resolve_lua_app(args.app)
    info = App(app, RUN_TARGETS[args.platform].plugins)
    ensure_artifacts("apple", args.engine_config, args.jobs)
    root = project_root(info, "apple")
    prepare_apple(info, root, args.platform, args.config, args.engine_config, args.jobs)
    generate_apple_project(info, root, forced=True)
    terminal.success(f"Generated `{shown_path(root / 'App.xcodeproj')}` from `{shown_path(root / 'project.yml')}`.")


def generate_template_project(args: argparse.Namespace) -> None:
    """Generates `App.xcodeproj` of the Apple template of Lua apps again, in a new starter app with the folder `haylen/`, which links no plugins and no native libraries, like every app whose project keeps the committed one."""
    ensure_artifacts("apple", args.engine_config, args.jobs)
    template = TEMPLATES_DIR / "lua" / "platform" / "apple"
    starter = BUILD_ROOT / "templates" / "lua"
    shutil.rmtree(starter, ignore_errors=True)
    copy_files(layered_files(template_layers("lua")), starter)
    copy = starter / "platform" / "apple"
    prepare_apple(App(starter, "macos"), copy, "macos", "Debug", args.engine_config, args.jobs)
    terminal.step(f"Generating `{shown_path(template / 'App.xcodeproj')}` from \"project.yml\"")
    run([ensure_xcodegen(), "generate", "--quiet", "--spec", copy / "project.yml"], cwd=copy)
    shutil.rmtree(template / "App.xcodeproj")
    shutil.copytree(copy / "App.xcodeproj", template / "App.xcodeproj", ignore=shutil.ignore_patterns("xcuserdata", ".DS_Store"))
    terminal.success(f"Generated `{shown_path(template / 'App.xcodeproj')}` from `{shown_path(template / 'project.yml')}`.")


def command_check(args: argparse.Namespace) -> None:
    """Checks the last build of an app for a platform against what the engine and its plugins need, and prints each missing requirement with who needs it and the snippet that adds it."""
    app = resolve_app(args.app)
    target = RUN_TARGETS[args.platform]
    info = App(app, target.plugins)
    root = info.build_folder / "web" if args.platform == "web" else project_root(info, target.template)
    missing = target.check(info, root, args)
    report_requirements(missing)
    if missing:
        raise BuildError(f'The app built for "{args.platform}" lacks the {len(missing)} requirements of the engine or its plugins above.')
    terminal.success(f'The app built for "{args.platform}" has everything the engine and its plugins need.')


def copy_template(kind: str, template: str, destination: Path) -> None:
    copy_files(layered_files(platform_layers(kind, template)), destination)


def command_new(args: argparse.Namespace) -> None:
    """Creates an app of a kind from the layers of its template, the starter app with a project of every platform, which the developer owns from then on."""
    folder = Path(args.folder).expanduser().resolve()
    if folder.exists() and any(folder.iterdir()):
        raise BuildError(f"The folder `{shown_path(folder)}` already exists and is not empty. Name a new folder or an empty one.")

    slug = re.sub(r"[^a-z0-9]+", "-", folder.name.lower()).strip("-")
    if not slug:
        raise BuildError(f'The folder name "{folder.name}" gives no app name. Use a folder name with letters or digits.')
    name = args.name or " ".join(word.capitalize() for word in slug.split("-"))
    identifier = args.identifier or f"com.example.{slug.replace('-', '')}"
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+", identifier):
        raise BuildError(f'The identifier "{identifier}" is not a reverse domain identifier such as "com.example.game".')

    copy_files(layered_files(template_layers(args.kind)), folder)
    document = json.loads((folder / "app.json").read_text(encoding="utf-8"))
    document.update({"name": name, "identifier": identifier, "orientation": args.orientation})
    document["window"]["title"] = name
    # The starter app is laid out for landscape, so a portrait app turns its window and design resolution upright.
    if args.orientation == "portrait":
        for section in (document["window"], document["design"]):
            section["width"], section["height"] = section["height"], section["width"]
    (folder / "app.json").write_text(json.dumps(document, indent=4) + "\n", encoding="utf-8")
    terminal.success(f'Created the {APP_KINDS[args.kind]} app "{name}" with the identifier "{identifier}" in `{shown_path(folder)}`.')
    terminal.info(f'Run it with "{TOOL} {"run-cpp" if args.kind == "cpp" else "run"} {shown_path(folder)}".')


def command_platform_add(args: argparse.Namespace) -> None:
    """Creates the project of a platform template in `platform/<template>` of an app from the layers of the template for its kind, which the developer owns from then on."""
    app = resolve_app(args.app)
    kind = require_platform_template(app, args.template)
    destination = app / "platform" / args.template
    if destination.exists():
        raise BuildError(f'The app already has the project `{shown_path(destination)}`. Compare it with the template with "{TOOL} platform diff {shown_path(app)} --template {args.template}".')
    copy_template(kind, args.template, destination)
    terminal.success(f'Created `{shown_path(destination)}` from the "{args.template}" template.')


def require_platform_template(app: Path, template: str) -> str:
    """Returns the kind of an app, and stops when its kind has no template of the platform."""
    kind = app_kind(app)
    if template not in platform_templates(kind):
        raise BuildError(f"The {APP_KINDS[kind]} apps have no \"{template}\" template. Their platform templates are {', '.join(f'"{name}"' for name in platform_templates(kind))}.")
    return kind


def project_files(folder: Path) -> dict[str, Path]:
    """Lists the files of a platform project that belong to it, leaving out the generated folder at its root, build outputs and the state of Xcode for each person."""
    skipped = {".DS_Store", ".git", "build", ".gradle", ".cxx", ".kotlin", "xcuserdata"}
    files = {}
    for path in folder.rglob("*"):
        parts = path.relative_to(folder).parts
        if path.is_file() and parts[0] != GENERATED_FOLDER and not skipped.intersection(parts):
            files[path.relative_to(folder).as_posix()] = path
    return files


def command_platform_diff(args: argparse.Namespace) -> None:
    """Shows how the project of a platform in an app differs from the current template, without changing anything, so the developer adopts what they want of the template."""
    app = resolve_app(args.app)
    kind = require_platform_template(app, args.template)
    project = app / "platform" / args.template
    if not project.is_dir():
        raise BuildError(f'The app `{shown_path(app)}` has no "{args.template}" project. Create it with "{TOOL} platform add {shown_path(app)} {args.template}".')
    template = layered_files(platform_layers(kind, args.template))
    own = project_files(project)
    differences = 0
    for path in sorted(template.keys() | own.keys()):
        if path not in own:
            terminal.info(f"Only the template has `{shown_path(template[path])}`.")
        elif path not in template:
            terminal.info(f"Only the app has `{shown_path(own[path])}`.")
        elif template[path].read_bytes() != own[path].read_bytes():
            try:
                lines = difflib.unified_diff(template[path].read_text(encoding="utf-8").splitlines(keepends=True), own[path].read_text(encoding="utf-8").splitlines(keepends=True), f"template/{path}", f"app/{path}")
                terminal.verbatim("".join(lines).rstrip("\n"))
            except UnicodeDecodeError:
                terminal.info(f"The binary file `{shown_path(own[path])}` differs from `{shown_path(template[path])}`.")
        else:
            continue
        differences += 1
    terminal.success(f'The project differs from the "{args.template}" template in {differences} files.' if differences else f'The project matches the "{args.template}" template.')


def keytool() -> Path:
    """Finds the command `keytool` of the JDK that Gradle uses, the one of `JAVA_HOME`, or else the one on `PATH`."""
    home = os.environ.get("JAVA_HOME")
    if home and (Path(home) / "bin" / executable_name("keytool")).is_file():
        return Path(home) / "bin" / executable_name("keytool")
    found = shutil.which("keytool")
    if found is None:
        raise BuildError('The command "keytool" of the JDK was not found. Install JDK 17 or newer and set "JAVA_HOME" to its folder.')
    return Path(found)


def android_key_files(root: Path, kind: str) -> dict[str, Path]:
    """Names the files of the release or debug key of an Android project: the keystore, its certificate and the properties that the Gradle scripts read."""
    folder = root / ANDROID_KEY_FOLDER
    return {"keystore": folder / f"{kind}.jks", "certificate": folder / f"{kind}.pem", "properties": folder / f"{kind}.properties"}


def android_key_properties(kind: str, alias: str, password: str) -> str:
    """Writes the properties that the Gradle scripts of the template read to sign a build type with its key, with the keystore relative to them."""
    values = {"storeFile": f"{kind}.jks", "storePassword": password, "keyAlias": alias, "keyPassword": password}
    lines = [f'# Written by "haylen.py android-key". It holds the passwords of the {kind} key, so the folder stays out of the repository.']
    return "\n".join([*lines, *(f"{key}={java_property(value)}" for key, value in values.items())]) + "\n"


def command_android_key(args: argparse.Namespace) -> None:
    """Creates the release upload key or the debug key of the Android project of an app with `keytool`, exports its certificate as PEM and writes the properties that the signing of the Gradle scripts reads. An app without an Android project gets one from the template first."""
    app = resolve_app(args.app)
    if not args.alias:
        raise BuildError('The option "--alias" needs a name for the key.')
    if len(args.password) < ANDROID_KEY_PASSWORD_LENGTH:
        raise BuildError(f'The option "--password" needs at least {ANDROID_KEY_PASSWORD_LENGTH} characters, which "keytool" asks of every keystore.')

    root = app / "platform" / "android"
    files = android_key_files(root, args.kind)
    existing = [path for path in files.values() if path.exists()]
    if existing and not args.force:
        raise BuildError(f'The Android project already has a {args.kind} key in `{shown_path(existing[0])}`. Keep it, since an app signed with another key cannot update the installed one, or replace it with "--force".')
    tool = keytool()
    if not root.is_dir():
        copy_template(app_kind(app), "android", root)
        terminal.success(f'Created `{shown_path(root)}` from the "android" template.')
    for path in existing:
        path.unlink()

    # The password reaches `keytool` through the environment, so neither the printed commands nor the list of processes show it.
    environment = {**os.environ, "HAYLEN_KEY_PASSWORD": args.password}
    password = ["-storepass:env", "HAYLEN_KEY_PASSWORD"]
    files["keystore"].parent.mkdir(parents=True, exist_ok=True)
    terminal.step(f"Creating the {args.kind} key of `{shown_path(root)}`")
    run([tool, "-genkeypair", "-keystore", files["keystore"], "-storetype", "PKCS12", "-alias", args.alias, "-keyalg", "RSA", "-keysize", str(ANDROID_KEY_SIZE), "-validity", str(ANDROID_KEY_DAYS), "-dname", args.dname, *password], env=environment)
    run([tool, "-exportcert", "-rfc", "-keystore", files["keystore"], "-alias", args.alias, "-file", files["certificate"], *password], env=environment)
    files["properties"].write_text(android_key_properties(args.kind, args.alias, args.password), encoding="utf-8")
    terminal.success(f"Created the {args.kind} key `{shown_path(files['keystore'])}` with its certificate `{shown_path(files['certificate'])}`.")
    terminal.info(f"The {args.kind} builds of the project sign with it, through `{shown_path(files['properties'])}`, and the folder stays out of the repository.")


def write_app_json(folder: Path, document: dict) -> None:
    (folder / "app.json").write_text(json.dumps(document, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")


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
                raise BuildError(f'The repository `{args.plugin}` is not a plugin, because it holds no "plugin.json" at its root.')
            identifier = json.loads((checkout / "plugin.json").read_text(encoding="utf-8")).get("id")
            source = checkout.rename(Path(scratch) / identifier) if isinstance(identifier, str) and PLUGIN_ID.fullmatch(identifier) else checkout
        elif args.ref:
            raise BuildError('The option "--ref" picks a branch, tag or commit of a git repository, and a plugin folder has none. Leave the option out for a folder.')
        else:
            source = Path(args.plugin).expanduser().resolve()
        if not (source / "plugin.json").is_file():
            raise BuildError(f'The path `{shown_path(source)}` is not a plugin folder, because it holds no "plugin.json".')
        plugin = Plugin.load(source)
        target = app / "plugins" / plugin.id
        if source != target:
            shutil.rmtree(target, ignore_errors=True)
            shutil.copytree(source, target, ignore=COPY_IGNORED)

    document = json.loads((app / "app.json").read_text(encoding="utf-8"))
    listed = document.setdefault("plugins", {})
    values = listed.setdefault(plugin.id, {})
    for name, parameter in plugin.parameters.items():
        if name not in values and ("default" in parameter or parameter.get("required")):
            values[name] = parameter.get("default", "")
    write_app_json(app, document)

    terminal.success(f'Added the plugin "{plugin.id}" {plugin.version} to `{shown_path(app)}`.')
    missing = [name for name, value in values.items() if value == "" and plugin.parameters.get(name, {}).get("required")]
    if missing:
        names = ", ".join(f'"{name}"' for name in missing)
        terminal.warning(f'Fill in {names} under "plugins.{plugin.id}" of `{shown_path(app / "app.json")}` before the app builds.')
    for required in plugin.requires:
        if required not in listed:
            terminal.warning(f'The plugin "{plugin.id}" requires the plugin "{required}", which the app does not list yet. Add it with "{TOOL} plugin add" and its folder or repository.')


def command_plugin_remove(args: argparse.Namespace) -> None:
    """Deletes a plugin from `plugins/` of an app and from its `app.json`."""
    app = resolve_app(args.app)
    folder = app / "plugins" / args.id
    document = json.loads((app / "app.json").read_text(encoding="utf-8"))
    listed = document.get("plugins", {})
    if args.id not in listed and not folder.exists():
        raise BuildError(f'The app `{shown_path(app)}` has no plugin "{args.id}". List its plugins with "{TOOL} plugin list --app {shown_path(app)}".')

    shutil.rmtree(folder, ignore_errors=True)
    if folder.parent.is_dir() and not any(folder.parent.iterdir()):
        folder.parent.rmdir()
    if args.id in listed:
        del listed[args.id]
        if not listed:
            del document["plugins"]
        write_app_json(app, document)
    terminal.success(f'Removed the plugin "{args.id}" from `{shown_path(app)}`.')


def plugin_status(app: Path, identifier: str, listed: dict) -> list[str]:
    """Describes a plugin of an app in one line, followed by the problems that keep it from building for any of its platforms."""
    folder = app / "plugins" / identifier
    if not (folder / "plugin.json").is_file():
        return [f'{identifier:<24} Missing, because "app.json" lists it and `{shown_path(folder / "plugin.json")}` does not exist.']
    try:
        plugin = Plugin.load(folder)
    except BuildError as error:
        return [f"{identifier:<24} Invalid", *(f"    {line}" for line in str(error).splitlines())]

    problems: list[str] = []
    status = 'Not in "app.json", so no build carries it.'
    if identifier in listed:
        problems += [f'The plugin "{identifier}" requires the plugin "{required}", which "app.json" does not list.' for required in plugin.requires if required not in listed]
        for platform in plugin.manifest["platforms"]:
            problems += [problem for problem in parameter_values(app, plugin, listed[identifier], platform)[1] if problem not in problems]
        status = f"Problems: {len(problems)}" if problems else "Ready"

    return [f"{identifier:<24} {plugin.version:<10} {', '.join(plugin.manifest['platforms']):<48} {status}", *(f"    {problem}" for problem in problems)]


def command_plugin_list(args: argparse.Namespace) -> None:
    """Lists the plugins of an app with the problems that keep each one from building."""
    app = resolve_app(args.app)
    listed = json.loads((app / "app.json").read_text(encoding="utf-8")).get("plugins", {})
    folders = sorted(path.name for path in (app / "plugins").iterdir() if path.is_dir()) if (app / "plugins").is_dir() else []
    identifiers = [*listed, *(name for name in folders if name not in listed)]
    if not identifiers:
        terminal.info(f"The app `{shown_path(app)}` has no plugins.")
    for identifier in identifiers:
        terminal.info("\n".join(plugin_status(app, identifier, listed)))


def command_plugin_new(args: argparse.Namespace) -> None:
    """Creates a plugin from `templates/plugin`, with its id in the names of its files, classes and modules."""
    folder = Path(args.folder).expanduser().resolve()
    identifier = args.id or folder.name
    if not PLUGIN_ID.fullmatch(identifier):
        raise BuildError(f'The name "{identifier}" is not a plugin id. Plugin ids are "dash-case", such as "camera-scanner".')
    if identifier != folder.name:
        raise BuildError(f'A plugin folder is named after the id of the plugin, so the folder of "{identifier}" is named "{identifier}", not "{folder.name}".')
    if folder.exists() and any(folder.iterdir()):
        raise BuildError(f"The folder `{shown_path(folder)}` already exists and is not empty. Name a new folder or an empty one.")

    words = identifier.split("-")
    tokens = {"{{ID}}": identifier, "{{NAME}}": "".join(word.capitalize() for word in words), "{{TITLE}}": " ".join(word.capitalize() for word in words), "{{PACKAGE}}": "".join(words)}

    def substitute(text: str) -> str:
        for token, value in tokens.items():
            text = text.replace(token, value)
        return text

    for source in sorted(path for path in PLUGIN_TEMPLATE.rglob("*") if path.is_file() and path.name != ".DS_Store"):
        target = folder / substitute(source.relative_to(PLUGIN_TEMPLATE).as_posix())
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(substitute(source.read_text(encoding="utf-8")), encoding="utf-8")
    terminal.success(f'Created the plugin "{identifier}" in `{shown_path(folder)}`.')
    terminal.info(f'Add it to an app with "{TOOL} plugin add {shown_path(folder)} --app <app folder>".')


# The platforms a C++ app project runs on: this machine, the browser, Mac Catalyst, iOS, tvOS, their simulators and Android.
CPP_RUN_PLATFORMS = [host_name(), "web", *(name for name in APPLE_NATIVE_SLICES if name != "macos"), "android"]


def cpp_engine_options() -> list[str]:
    """The options that hand a C++ app project the engine of this checkout, which the project adds from "HAYLEN_ENGINE_SOURCE", with the compiler of its shaders and the dependency cache of the engine builds, instead of a download of every dependency into the folder of the project."""
    cache = os.environ.get("CPM_SOURCE_CACHE", str(ROOT / ".cache" / "cpm"))
    return [f"-DHAYLEN_ENGINE_SOURCE={ENGINE_DIR.as_posix()}", f"-DHAYLEN_SOKOL_SHDC={ensure_shdc()}", f"-DCPM_SOURCE_CACHE={cache}"]


def command_run_cpp(args: argparse.Namespace) -> None:
    """Builds a C++ app project, which compiles the engine through its own CMake, and runs it on this machine, in the browser, on an Apple simulator or device, on Mac Catalyst or on Android."""
    if not args.project:
        raise BuildError('The command needs the folder of a C++ app project, named after it, which is a CMake project that adds the engine and calls "haylen_add_app".')
    project = Path(args.project).expanduser().resolve()
    if not (project / "CMakeLists.txt").is_file():
        raise BuildError(f'The path `{shown_path(project)}` is not a CMake project, because it holds no "CMakeLists.txt". Name the folder of a project that adds the engine and calls "haylen_add_app".')
    target = args.target
    folder = CPP_BUILDS_DIR / build_folder_name(project)

    if args.platform == "web":
        site = bundle_web(project, target, args.config, folder, args.jobs, cpp_release_options(args))
        if args.config == "Release":
            stop_on_release_problems(cpp_release_problems(cpp_release_app(folder / f"web-{args.config.lower()}", target), site, "app"))
        if not args.build_only:
            serve(site, args.host, args.port, args.coep, args.coop, args.open)
    elif args.platform == "android":
        run_cpp_android(project, target, folder, args)
    elif args.platform in APPLE_NATIVE_SLICES and args.platform != "macos":
        run_cpp_apple(project, target, folder, args)
    else:
        directory = folder / f"{host_name()}-{args.config.lower()}"
        command = ["cmake", "-S", project, "-B", directory, *cpp_engine_options(), f"-DCMAKE_BUILD_TYPE={args.config}", *cpp_release_options(args)]
        if host_name() != "windows" or shutil.which("ninja"):
            command += ["-G", "Ninja"]
        if host_name() == "macos":
            command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={APPLE_MINIMUM_VERSIONS['macOS']}")
        terminal.step(f'Building the target "{target}" in `{shown_path(directory)}`')
        run(command)
        run(["cmake", "--build", directory, "--config", args.config, "--target", target, "--parallel", str(args.jobs)])
        executable = cmake_app_executable(directory, target)
        if args.config == "Release":
            app = cpp_release_app(directory, target)
            bundle = next((parent for parent in executable.parents if parent.suffix == ".app"), None)
            stop_on_release_problems(cpp_release_problems(app, bundle, "Contents/Resources/app") if bundle else cpp_release_problems(app, executable.parent, "app"))
        if not args.build_only:
            terminal.step(f'Running "{target}"')
            run([executable])


def cpp_release_options(args: argparse.Namespace) -> list[str]:
    """The options that make the Release configuration of a C++ app ship its protected release, built by the content tool of the desktop artifacts, as Lua apps do, and every other configuration the package as it is."""
    if args.config != "Release":
        return ["-DHAYLEN_PROTECTED_RELEASE=OFF"]
    return ["-DHAYLEN_PROTECTED_RELEASE=ON", f"-DHAYLEN_CONTENT_TOOL={content_tool(args.engine_config, args.jobs)}"]


def cpp_release_app(directory: Path, target: str) -> App:
    """The app whose package the release build of a C++ target shipped, which the build recorded in its release folder."""
    recorded = directory / "haylen-release" / target / "package.txt"
    if not recorded.is_file():
        raise BuildError(f'The build in `{shown_path(directory)}` made no protected release of "{target}". Build the project again in the Release configuration.')
    return App(Path(recorded.read_text(encoding="utf-8").strip()), host_name())


def cpp_release_problems(app: App, folder: Path, release: str) -> list[Requirement]:
    """Inspects the built app of a C++ project for anything a release never holds."""
    return [Requirement(problem, RELEASE_ADVICE) for problem in release_problems(app, folder_entries(folder), release, read_secrets(app))]


def run_cpp_apple(project: Path, target: str, folder: Path, args: argparse.Namespace) -> None:
    """Builds a C++ app for iOS, tvOS or their simulators with the Xcode generator, which compiles the launch screen and signs the bundle, or for Mac Catalyst with Ninja and the Mac Catalyst toolchain, for the architecture of this Mac, then launches it like `haylen.py run`."""
    require_host("apple")
    device = args.platform in {"ios", "tvos"}
    arch = "arm64" if device or host_arch() == "arm64" else "x86_64"
    directory = folder / f"{args.platform}-{args.config.lower()}"
    generator = "Ninja" if args.platform == "catalyst" else "Xcode"
    plugins = directory / CPP_PLUGINS_FILE
    command = ["cmake", "-S", project, "-B", directory, "-G", generator, *cpp_engine_options(), f"-DCMAKE_BUILD_TYPE={args.config}", *apple_slice_options(APPLE_NATIVE_SLICES[args.platform], arch), *cpp_release_options(args), f"-DHAYLEN_PLUGINS_FILE={plugins.as_posix()}"]
    if device:
        command.append(f"-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM={apple_team()}")
    terminal.step(f'Building the target "{target}" for "{args.platform}" in `{shown_path(directory)}`')

    # The first configuration records the package of the target, whose plugins give the second one their Apple parts.
    run(command)
    if write_cpp_apple_plugins(App(Path((directory / "bin" / target / "package.txt").read_text(encoding="utf-8").strip()), RUN_TARGETS[args.platform].plugins), args.platform, directory, plugins):
        run(command)
    run(["cmake", "--build", directory, "--config", args.config, "--target", target, "--parallel", str(args.jobs), *(["--", "-allowProvisioningUpdates"] if device else [])])

    # Ninja places the bundle in `bin/<target>`, while the Xcode generator places it in `bin`.
    settings = APPLE_RUNS[args.platform]
    bundle = next((directory / "bin").glob(f"**/{target}.app"))
    if args.config == "Release":
        stop_on_release_problems(cpp_release_problems(cpp_release_app(directory, target), bundle, "Contents/Resources/app" if args.platform == "catalyst" else "app"))
    if args.build_only:
        return
    simulator = apple_simulator(settings["simulator"], args.device) if "simulator" in settings else None
    launch_apple(bundle, args, simulator, [])


def write_cpp_apple_plugins(app: App, platform: str, directory: Path, fragment: Path) -> bool:
    """Copies the Apple sources and resources of the plugins of a C++ app that build for a run platform into its build folder and writes the CMake file that `haylen_add_app` reads, which compiles the sources into the app, links the system frameworks, bundles the resources and adds the `infoPlist` keys to its `Info.plist`, the parts that the target templates give Lua apps. Returns whether the file changed. Swift packages and build scripts need the Xcode project of the Apple template."""
    target_platform = RUN_TARGETS[platform].plugins
    copied = directory / "haylen-plugins"
    shutil.rmtree(copied, ignore_errors=True)
    sources: list[Path] = []
    resources: list[Path] = []
    frameworks: list[str] = []
    for plugin in app.plugins:
        apple = plugin_section(app, plugin, "apple", (target_platform,)) if plugin.supports(target_platform) else None
        if apple is None:
            continue
        unsupported = [key for key in ("packages", "buildScripts") if apple.get(key)]
        if unsupported:
            raise BuildError(f'The plugin "{plugin.id}" needs {" and ".join(f"\"{key}\"" for key in unsupported)} on Apple platforms, which only the Xcode project of the Apple template builds. Build the app from a Lua project, or use a plugin without them.')
        if "sources" in apple:
            shutil.copytree(plugin.folder / apple["sources"], copied / plugin.id / "sources", ignore=COPY_IGNORED)
            sources += sorted(path for path in (copied / plugin.id / "sources").rglob("*") if path.suffix in CPP_APPLE_PLUGIN_SOURCES)
        for resource in plugin.manifest["apple"].get("resources", []):
            source = plugin_file(app, plugin, resource)
            if source is not None:
                resources.append(copy_into(source, copied / plugin.id / "resources"))
        frameworks += [Path(framework).stem for framework in apple.get("frameworks", []) if Path(framework).stem not in frameworks]

    # The keys of the plugins go inside the dictionary of the Info.plist of the engine, which a property list of their own carries.
    keys = apple_plugin_keys(app, (target_platform,), "infoPlist", {}, {}, '"Info.plist"')
    entries = plistlib.dumps(keys, sort_keys=True).decode().split("<dict>", 1)[1].rsplit("</dict>", 1)[0].strip("\n") if keys else ""
    lines = [
        "# Written by haylen.py from app.json and the plugins of the app for haylen_add_app, which builds their Apple parts into the app.",
        f"set(HAYLEN_PLUGIN_SOURCES [==[{';'.join(path.as_posix() for path in sources)}]==])",
        f"set(HAYLEN_PLUGIN_RESOURCES [==[{';'.join(path.as_posix() for path in resources)}]==])",
        f"set(HAYLEN_PLUGIN_FRAMEWORKS [==[{';'.join(frameworks)}]==])",
        f"set(HAYLEN_PLUGIN_KEYS [==[{entries}\n]==])" if entries else "set(HAYLEN_PLUGIN_KEYS \"\")",
        "",
    ]
    text = "\n".join(lines)
    if fragment.is_file() and fragment.read_text(encoding="utf-8") == text:
        return False
    fragment.write_text(text, encoding="utf-8")
    return True


def run_cpp_android(project: Path, target: str, folder: Path, args: argparse.Namespace) -> None:
    """Builds the library of a C++ app for the ABI of the Android device and packages it with the package of the app into its Android project, whose activity loads it instead of the Lua player, then installs and launches it like `haylen.py run`."""
    device = android_device(args.device)
    abi = capture([adb(), "-s", device, "shell", "getprop", "ro.product.cpu.abi"]).strip()
    if abi not in ANDROID_ABIS:
        raise BuildError(f"The device \"{device}\" runs \"{abi}\", while the engine builds for {', '.join(f'"{name}"' for name in ANDROID_ABIS)}. Use a device or an emulator of one of those.")
    directory = folder / f"android-{abi}-{args.config.lower()}"
    terminal.step(f'Building the target "{target}" for "{abi}" in `{shown_path(directory)}`')
    run(["cmake", "-S", project, "-B", directory, "-G", "Ninja", *cpp_engine_options(), f"-DCMAKE_BUILD_TYPE={args.config}", *android_options(abi), *cpp_release_options(args)])
    run(["cmake", "--build", directory, "--target", target, "--parallel", str(args.jobs)])

    # The activity, the bridge and the other Java classes come from the `haylen` library of the artifacts, while the app brings the engine in its own library, and a release build brings the part of its protected release that ships.
    built = directory / "bin" / target
    app = App(Path((built / "package.txt").read_text(encoding="utf-8").strip()), "android")
    release = Path((built / "release.txt").read_text(encoding="utf-8").strip()) if args.config == "Release" else None
    ensure_artifacts("android", args.engine_config, args.jobs)
    root = project_root(app, "android")
    prepare_android(app, root, target, args.jobs, release)
    copy_into(built / f"lib{target}.so", root / GENERATED_FOLDER / "jniLibs" / abi)
    apk = build_android(app, root, args)
    requirements = check_android(app, root, args)
    report_requirements([requirement for requirement in requirements if requirement.advice != RELEASE_ADVICE])
    stop_on_release_problems(requirements)
    if not args.build_only:
        launch_android(apk, args, device, [])


def bundle_web(source: Path, target: str, config: str, folder: Path, jobs: int, options: list[str] | None = None) -> Path:
    """Builds a CMake web target for WebGPU and WebGL2 in a build folder and bundles both into its web folder with the web parts of the plugins of its app, whose page runs the backend the browser supports, and returns that folder. A protected release, which both builds make byte for byte the same, goes once into `app/` of the site, where its content streams by range."""
    output = folder / "web"
    shutil.rmtree(output, ignore_errors=True)
    releases = []
    for platform_name, backend in (("web", "webgpu"), ("web-webgl2", "webgl2")):
        directory = folder / f"{platform_name}-{config.lower()}"
        terminal.step(f'Building the target "{target}" for "{platform_name}" in `{shown_path(directory)}`')
        if options is not None or not (directory / "CMakeCache.txt").exists():
            renderer = "WGPU" if platform_name == "web" else "GLES3"
            run([ensure_emsdk(), "cmake", "-S", source, "-B", directory, "-G", "Ninja", *cpp_engine_options(), f"-DCMAKE_BUILD_TYPE={config}", f"-DHAYLEN_RENDER_BACKEND={renderer}", *(options or [])])
        run(["cmake", "--build", directory, "--target", target, "--parallel", str(jobs)])
        built = directory / "bin" / target
        destination = output / backend
        destination.mkdir(parents=True)
        for suffix in (".js", ".wasm", ".data"):
            if (built / f"{target}{suffix}").is_file():
                shutil.copy2(built / f"{target}{suffix}", destination)
        for name in (WEB_AUDIO_WORKLET, WEB_CONTENT_STORE):
            shutil.copy2(built / name, destination)
        if (built / "app").is_dir():
            releases.append(built / "app")
    if releases:
        if {path.name: file_hash(path) for path in releases[0].iterdir()} != {path.name: file_hash(path) for path in releases[-1].iterdir()}:
            raise BuildError(f'The WebGPU and WebGL2 builds of "{target}" made different protected releases, while the page serves one. Build them again from a clean build folder.')
        shutil.copytree(releases[0], output / "app")

    # The web parts of the plugins of the app go next to the page, which imports and loads them before the app starts, as the loader of the web template does.
    package = Path((built / "package.txt").read_text(encoding="utf-8").strip())
    (output / CPP_WEB_PLUGINS).write_text(json.dumps(write_web_plugins(App(package, "web"), output), indent=4) + "\n", encoding="utf-8")

    # The page is the shell of the target, with the script that picks the backend where Emscripten would put its own script, next to the engine logo that the default shell shows as its icon.
    shell = (built / f"{target}.shell.html").read_text(encoding="utf-8")
    picker = (ENGINE_DIR / "platform" / "web" / "backend-picker.html").read_text(encoding="utf-8").replace("{{TARGET}}", target)
    if "{{{ SCRIPT }}}" not in shell:
        raise BuildError(f'The web shell of "{target}" has no "{{{{{{ SCRIPT }}}}}}" placeholder, where the page loads the runtime. Add it to the shell that "WEB_SHELL" names.')
    (output / "index.html").write_text(shell.replace("{{{ SCRIPT }}}", picker.strip()), encoding="utf-8")
    shutil.copy2(built / ENGINE_LOGO.name, output)
    terminal.success(f'Bundled "{target}" for WebGPU and WebGL2 into `{shown_path(output)}`.')
    return output


# Protected releases: the content tool builds them from the package of an app with the keys of the app, which live outside every project.


def content_tool(config: str, jobs: int) -> Path:
    """Returns the content tool of this machine, built with the desktop artifacts when they are missing or older than the engine sources."""
    ensure_artifacts("desktop", config, jobs)
    return desktop_artifact(CONTENT_TOOL)


def user_config_folder() -> Path:
    """The folder where the tools of Haylen keep what belongs to the user rather than to a project, such as the keys of apps."""
    system = host_name()
    if system == "macos":
        return Path.home() / "Library" / "Application Support" / "Haylen"
    if system == "windows":
        return Path(os.environ["APPDATA"]) / "Haylen"
    return Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") / "haylen"


def keys_folder(identifier: str) -> Path:
    """The key folder of an app: under the folder that "HAYLEN_KEYS_DIR" names, as continuous integration gives it, or else under the configuration folder of the user, never inside a project."""
    root = os.environ.get(KEYS_VARIABLE)
    return (Path(root).expanduser() if root else user_config_folder() / "keys") / identifier


def ensure_keys(app: App, tool: Path) -> Path:
    """Returns the key folder of an app, and creates its keys on the machine of the developer when it has none yet. Continuous integration receives the keys and never creates them, since every release of an app needs the same keys."""
    folder = keys_folder(app.identifier)
    if (folder / "keys.json").is_file():
        return folder
    if os.environ.get(KEYS_VARIABLE):
        raise BuildError(f'The key folder `{shown_path(folder)}` holds no keys of "{app.identifier}". Give "{KEYS_VARIABLE}" a copy of the key folders made where the releases of the app were built first.')
    terminal.step(f'Creating the content keys of "{app.identifier}"')
    run([tool, "keys", "create", folder, "--identifier", app.identifier])
    terminal.warning(f"Created the keys of the protected releases of the app in `{shown_path(folder)}`.", f'Back up this folder, since every release of the app needs its keys, and give continuous integration a copy through "{KEYS_VARIABLE}".')
    return folder


def release_folder(app: App, profile: str) -> Path:
    return app.build_folder / "release" / profile


def build_release(app: App, profile: str, config: str, jobs: int) -> Path:
    """Builds the protected release of an app for a content profile into its build folder with the content tool, reusing the shards of the release before it and the build cache of the app, and returns the folder that ships with the app: the whole release, or for an app with a content server the part that its required content needs."""
    compile_app_shaders(app.folder)
    tool = content_tool(config, jobs)
    keys = ensure_keys(app, tool)
    folder = release_folder(app, profile)
    staging = folder.with_name(f"{profile}.new")
    shutil.rmtree(staging, ignore_errors=True)
    command = [tool, "build", app.folder, "--keys", keys, "--profile", profile, "--build", str(app.version_code), "--output", staging, "--cache", app.build_folder / "content-cache"]
    if (folder / "app.hmanifest").is_file():
        command += ["--previous", folder]
    terminal.step(f'Building the protected release of `{shown_path(app.folder)}` for the profile "{profile}"')
    run(command)
    shutil.rmtree(folder, ignore_errors=True)
    staging.rename(folder)
    if not app.content_server and not app.content_packs:
        warn_store_size(folder, profile)
        return folder

    # The shards of named packs go to the stores that deliver them, in a folder per pack, and with a server the shards that only later deliveries use stay in the release, which "content publish" puts on the server.
    shipped = folder.with_name(f"{profile}.shipped")
    packs = release_packs_folder(app, profile)
    shutil.rmtree(shipped, ignore_errors=True)
    shutil.rmtree(packs, ignore_errors=True)
    command = [tool, "stage", folder, "--keys", keys, "--output", shipped, "--app", app.folder, "--packs", packs]
    run(command if app.content_server else [*command, "--ship-deferred"])
    warn_store_size(shipped, profile)
    return shipped


def release_packs_folder(app: App, profile: str) -> Path:
    """The folder of the shards of the named packs of the last release of an app, one folder per pack, which stores deliver on their own."""
    return app.build_folder / "release" / f"{profile}.packs"


def warn_store_size(folder: Path, profile: str) -> None:
    """Warns when a release is larger than the store of its profile accepts inside an app."""
    if profile not in STORE_RELEASE_LIMITS:
        return
    limit, words = STORE_RELEASE_LIMITS[profile]
    size = sum(path.stat().st_size for path in folder.iterdir())
    if size > limit:
        terminal.warning(f"The protected release `{shown_path(folder)}` holds {size} bytes, more than {words}.", "Keep the content of the app within the limit, since the store refuses a larger app.")


def write_bootstrap(app: App, profile: str, destination: Path, config: str, jobs: int) -> None:
    """Writes the bootstrap of the release build of an app, which compiles in its identity and its sealed content keys, and keeps the file as it is when nothing changed."""
    tool = content_tool(config, jobs)
    run([tool, "bootstrap", "--keys", ensure_keys(app, tool), "--profile", profile, "--build", str(app.version_code), "--output", destination], echo=False)


def read_secrets(app: App) -> dict[str, bytes]:
    """Reads the content keys and the seed of the signing key of an app by their names, for the check that no release holds them in any form. Nothing prints them."""
    folder = keys_folder(app.identifier)
    if not (folder / "keys.json").is_file():
        return {}
    index = json.loads((folder / "keys.json").read_text(encoding="utf-8"))
    secrets = {f'the content key "{identifier}"': (folder / f"content-{identifier}.key").read_bytes() for identifier in index["contentKeys"]}
    secrets["the signing key"] = (folder / "signing.key").read_bytes()
    return secrets


def secret_forms(secret: bytes) -> list[bytes]:
    """The forms in which a key could show in a file: its bytes, its hexadecimal text in either case and its Base64 text."""
    return [secret, secret.hex().encode(), secret.hex().upper().encode(), base64.b64encode(secret), base64.urlsafe_b64encode(secret).rstrip(b"=")]


def release_problems(app: App, entries: list[tuple[str, int, Callable[[], bytes]]], release: str, secrets: dict[str, bytes]) -> list[str]:
    """Lists what a built release holds that it never may: Lua text, the index of a development package, a file of the package as it is, files in its release folder other than the manifests and shards, symbol files and a key of the app in any form. Each entry is a path inside the built app with its size and the function that reads it. The splash logo is the one file of the package that platforms show on their own."""
    package: dict[tuple[int, str], str] = {}
    for path in package_files(app.folder):
        if path != app.splash_logo and path.stat().st_size >= RELEASE_MATCH_MINIMUM:
            package[(path.stat().st_size, file_hash(path))] = path.relative_to(app.folder).as_posix()
    sizes = {size for size, _ in package}
    problems: list[str] = []
    released = False
    for path, size, read in entries:
        name = path.rsplit("/", 1)[-1]
        if path.startswith(f"{release}/"):
            released = released or name == RELEASE_MANIFESTS[0]
            if "/" in path[len(release) + 1 :] or (name not in RELEASE_MANIFESTS and not name.endswith(RELEASE_SHARD_SUFFIX)):
                problems.append(f'The release holds "{path}", while its folder "{release}" holds only manifests and shards.')
        if name.endswith(".lua"):
            problems.append(f'The release holds the Lua text "{path}".')
        if name == "haylen-package-index.json":
            problems.append(f'The release holds the index of a development package, "{path}".')
        if any(part.lower().endswith(SYMBOL_SUFFIXES) for part in path.split("/")):
            problems.append(f'The release holds the symbols "{path}", which stay with the developer.')
        if name.endswith(RELEASE_SHARD_SUFFIX):
            continue
        if size in sizes or secrets:
            data = read()
            match = package.get((size, hashlib.sha256(data).hexdigest()))
            if match:
                problems.append(f'The release holds "{path}", which is the file "{match}" of the package as it is.')
            for owner, secret in secrets.items():
                if any(form in data for form in secret_forms(secret)):
                    problems.append(f'The release holds {owner} of the app in "{path}".')
    if not released:
        problems.append(f'The release has no protected release in "{release}".')
    return problems


def content_release(app: App, args: argparse.Namespace) -> Path:
    """Returns the release folder that a content command names with "--release", or else the last release of the app for its platform."""
    folder = Path(args.release).resolve() if args.release else release_folder(app, CONTENT_PROFILES[args.platform])
    if not (folder / "app.hmanifest").is_file():
        raise BuildError(f'The folder `{shown_path(folder)}` holds no protected release. Build one with "{TOOL} content build {shown_path(app.folder)} --platform {args.platform}".')
    return folder


def command_content_build(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), RUN_TARGETS[args.platform].plugins)
    folder = build_release(app, CONTENT_PROFILES[args.platform], args.engine_config, args.jobs)
    release = release_folder(app, CONTENT_PROFILES[args.platform])
    shipped = "" if folder == release else f", and the part that ships with the app in `{shown_path(folder)}`"
    terminal.success(f"Built the protected release `{shown_path(release)}`{shipped}.")


def command_content_verify(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), RUN_TARGETS[args.platform].plugins)
    tool = content_tool(args.engine_config, args.jobs)
    run([tool, "verify", content_release(app, args), "--keys", ensure_keys(app, tool)])


def command_content_inspect(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), RUN_TARGETS[args.platform].plugins)
    tool = content_tool(args.engine_config, args.jobs)
    run([tool, "inspect", content_release(app, args), "--keys", ensure_keys(app, tool), *(["--chunks"] if args.chunks else [])], echo=False)


def command_content_diff(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), host_name())
    tool = content_tool(args.engine_config, args.jobs)
    run([tool, "diff", Path(args.earlier).resolve(), Path(args.later).resolve(), "--keys", ensure_keys(app, tool)], echo=False)


def command_content_publish(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), RUN_TARGETS[args.platform].plugins)
    compile_app_shaders(app.folder)
    tool = content_tool(args.engine_config, args.jobs)
    keys = ensure_keys(app, tool)
    terminal.step(f'Publishing the content of `{shown_path(app.folder)}` to the channel "{args.channel}" of `{shown_path(Path(args.output).resolve())}`')
    # The first generation of a channel keeps the shards of the release that ships with the app, so installed apps download only what changed.
    release = release_folder(app, CONTENT_PROFILES[args.platform])
    shipped = ["--release", release] if (release / "content.hmanifest").is_file() else []
    run([tool, "publish", app.folder, "--keys", keys, "--profile", CONTENT_PROFILES[args.platform], "--build", str(app.version_code), "--channel", args.channel, "--output", Path(args.output).resolve(), "--cache", app.build_folder / "content-cache", *shipped])


# Steam: the depots of a release for SteamPipe, which delivers the app with its protected content and the packs of the app as depots of their own, so a Steam app needs no content server of its own.
STEAM_PLATFORMS = ("macos", "windows", "linux")


def steam_depot_ids(args: argparse.Namespace, names: list[str]) -> dict[str, str]:
    """Reads the depot ID of every depot from the "--depot name=id" options, and stops when one lacks it."""
    ids = {}
    for entry in args.depot:
        name, _, value = entry.partition("=")
        if not value.isdigit():
            raise BuildError(f'The option "--depot {entry}" must name a depot and its numeric ID, such as "--depot {args.platform}=1001".')
        ids[name] = value
    missing = ", ".join('"' + name + '"' for name in names if name not in ids)
    if missing:
        raise BuildError(f'The depots {missing} have no ID. Give each one with "--depot <name>=<id>", the IDs of the depots that Steamworks shows.')
    return ids


def write_steam_scripts(output: Path, app_id: str, description: str, depots: dict[str, str], branch: str | None) -> None:
    """Writes the app build script and one depot build script per depot for "steamcmd +run_app_build", each depot taking its whole folder under `content/`."""
    scripts = output / "scripts"
    scripts.mkdir(parents=True, exist_ok=True)
    entries = "".join(f'\t\t"{depot}" "depot_build_{depot}.vdf"\n' for depot in depots.values())
    live = f'\t"SetLive" "{branch}"\n' if branch else ""
    (scripts / "app_build.vdf").write_text(f'"AppBuild"\n{{\n\t"AppID" "{app_id}"\n\t"Desc" "{description}"\n\t"ContentRoot" "../content/"\n\t"BuildOutput" "../output/"\n{live}\t"Depots"\n\t{{\n{entries}\t}}\n}}\n', encoding="utf-8")
    for name, depot in depots.items():
        (scripts / f"depot_build_{depot}.vdf").write_text(f'"DepotBuild"\n{{\n\t"DepotID" "{depot}"\n\t"ContentRoot" "../content/{name}/"\n\t"FileMapping"\n\t{{\n\t\t"LocalPath" "*"\n\t\t"DepotPath" "."\n\t\t"Recursive" "1"\n\t}}\n}}\n', encoding="utf-8")


def command_steam(args: argparse.Namespace) -> None:
    """Builds the release of an app for a desktop platform and lays it out for SteamPipe in `build/apps/<app>-<hash>/steam/`: the depot of the platform with the app and its protected release, a depot for every named pack whose shards install into `packs/` next to the executable or the bundle, and the build scripts of steamcmd."""
    app = App(resolve_lua_app(args.app), RUN_TARGETS[args.platform].plugins)
    args.config = "Release"
    depots = steam_depot_ids(args, [args.platform, *app.content_packs])
    output = Path(args.output).resolve() if args.output else app.build_folder / "steam"
    shutil.rmtree(output / "content", ignore_errors=True)
    platform_folder = output / "content" / args.platform
    place = ""
    if args.platform == "macos":
        root = prepare_apple_run(app, args)
        generate_apple_project(app, root, forced=False)
        bundle, _ = build_apple(app, root, args)
        stop_on_release_problems(check_apple(app, root, args))
        platform_folder.mkdir(parents=True)
        shutil.copytree(bundle, platform_folder / bundle.name, symlinks=True)
    else:
        root = prepare_desktop(app, args)
        build_desktop(app, root, args)
        stop_on_release_problems(check_desktop(app, root, args))
        shutil.copytree(desktop_folder(app, args.platform), platform_folder, symlinks=True)
        place = DESKTOP_LAYOUTS[args.platform][0].format(executable=app.executable)

    # The shards of a pack install next to the executable or the bundle, where the runtime looks for them, so a pack the player does not own leaves its files out and its assets unavailable.
    packs = release_packs_folder(app, CONTENT_PROFILES[args.platform])
    for pack in app.content_packs:
        shutil.copytree(packs / pack, output / "content" / pack / place / "packs")
    write_steam_scripts(output, args.app_id, f"{app.name} {app.version}", depots, args.branch)
    terminal.success(f"Staged the Steam depots of `{shown_path(app.folder)}` in `{shown_path(output)}`.")
    terminal.info(f'Upload them with "steamcmd +login <account> +run_app_build {shown_path(output / "scripts" / "app_build.vdf")} +quit".')


def command_content_compact(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), host_name())
    tool = content_tool(args.engine_config, args.jobs)
    run([tool, "compact", Path(args.tree).resolve(), "--keys", ensure_keys(app, tool), "--keep", str(args.keep)])


def command_content_keys(args: argparse.Namespace) -> None:
    app = App(resolve_app(args.app), host_name())
    tool = content_tool(args.engine_config, args.jobs)
    folder = ensure_keys(app, tool)
    run([tool, "keys", "rotate" if args.rotate else "show", folder], echo=False)
    terminal.info(f"The keys of the app live in `{shown_path(folder)}`.")


def command_package(args: argparse.Namespace) -> None:
    app = resolve_app(args.app)
    compile_app_shaders(app)
    output = Path(args.output).resolve()
    package_folder(app, output)
    terminal.success(f"Packaged `{shown_path(app)}` into `{shown_path(output)}`.")


class WebHandler(http.server.SimpleHTTPRequestHandler):
    """Serves a folder with the headers WebAssembly pages need: explicit MIME types, no caching and precompressed files. The web runtime is single-threaded, so the page stays in one browsing context group with the popups it opens, such as sign-in and payment pages, unless a page asks for cross-origin isolation with the opener policy `same-origin`."""

    coep = "require-corp"
    coop = "off"
    development: DevelopmentServer | None = None

    def do_GET(self) -> None:
        if self.development is not None and urllib.parse.urlsplit(self.path).path == DEVELOPMENT_PATH:
            self.development.accept(self)
            return
        super().do_GET()

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
        if not path.is_file():
            return super().send_head()
        return self.send_file(path)

    def send_file(self, path: Path):
        """Answers a file whole or by one byte range, with a strong validator, so the protected content of a release streams from the site the way a content delivery network serves it, and a range whose `If-Range` names another validator gets the whole file."""
        status = path.stat()
        size = status.st_size
        validator = f'"{size:x}-{status.st_mtime_ns:x}"'
        first, last = 0, size - 1
        requested = re.fullmatch(r"bytes=(\d+)-(\d*)", self.headers.get("Range", "").strip())
        partial = requested is not None and self.headers.get("If-Range", validator) == validator
        if partial:
            first = int(requested.group(1))
            last = min(int(requested.group(2)) if requested.group(2) else size - 1, size - 1)
            if first >= size:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return None
        self.send_response(206 if partial else 200)
        self.send_header("Content-Type", self.guess_type(str(path)))
        self.send_header("Content-Length", str(max(0, last - first + 1)))
        self.send_header("ETag", validator)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Access-Control-Expose-Headers", "Content-Range, Content-Length, ETag")
        if partial:
            self.send_header("Content-Range", f"bytes {first}-{last}/{size}")
        self.end_headers()
        return FileRange(path, first, max(0, last - first + 1))


class FileRange:
    """Reads one range of a file, so a server copies only the bytes a response promised."""

    def __init__(self, path: Path, offset: int, length: int) -> None:
        self.stream = path.open("rb")
        self.stream.seek(offset)
        self.remaining = length

    def read(self, size: int = -1) -> bytes:
        wanted = self.remaining if size < 0 else min(size, self.remaining)
        data = self.stream.read(wanted)
        self.remaining -= len(data)
        return data

    def close(self) -> None:
        self.stream.close()


def serve(directory: Path, host: str, port: int, coep: str = "require-corp", coop: str = "off", open_page: bool = False, development: DevelopmentServer | None = None) -> None:
    handler = functools.partial(type("Handler", (WebHandler,), {"coep": coep, "coop": coop, "development": development}), directory=str(directory))
    try:
        server = http.server.ThreadingHTTPServer((host, port), handler)
    except OSError as error:
        raise BuildError(f'The web server cannot listen on port {port} of "{host}": {error.strerror}. Stop what uses the port, or pick another one with "--port".') from error
    with server:
        url = f"http://{host}:{port}/"
        terminal.success(f"Serving `{shown_path(directory)}` at `{url}`")
        terminal.info("Press Ctrl+C to stop the server.")
        if open_page:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


# The development server: the page of `run --platform web` connects to it over a WebSocket on the same port as the site, and it pushes every file of the package that the developer saves, so the app reloads it in place.

DEVELOPMENT_PATH = "/haylen/development"
WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
# The largest file the server pushes, which is the largest message the WebSocket of the engine accepts by default.
DEVELOPMENT_MAX_FILE = 16 * 1024 * 1024
DEVELOPMENT_SETTLE_SECONDS = 0.05
BACKUP_SUFFIXES = (".swp", ".swo", ".swx", ".tmp", ".bak", ".orig")
WEBSOCKET_TEXT, WEBSOCKET_BINARY, WEBSOCKET_CLOSE, WEBSOCKET_PING, WEBSOCKET_PONG = 0x1, 0x2, 0x8, 0x9, 0xA


def websocket_accept(key: str) -> str:
    """Answers the key of a WebSocket handshake the way RFC 6455 asks."""
    return base64.b64encode(hashlib.sha1((key + WEBSOCKET_GUID).encode()).digest()).decode()


def websocket_frame(opcode: int, payload: bytes) -> bytes:
    """Encodes one whole frame from the server, which is never masked."""
    size = len(payload)
    if size < 126:
        header = bytes([0x80 | opcode, size])
    elif size < 1 << 16:
        header = bytes([0x80 | opcode, 126]) + size.to_bytes(2, "big")
    else:
        header = bytes([0x80 | opcode, 127]) + size.to_bytes(8, "big")
    return header + payload


def read_websocket_frame(stream) -> tuple[bool, int, bytes] | None:
    """Reads one frame from a client, which masks every frame, and returns whether it ends its message, its opcode and its payload, or nothing once the connection ended."""
    head = stream.read(2)
    if len(head) < 2:
        return None
    size = head[1] & 0x7F
    if size == 126:
        size = int.from_bytes(stream.read(2), "big")
    elif size == 127:
        size = int.from_bytes(stream.read(8), "big")
    mask = stream.read(4) if head[1] & 0x80 else b"\0\0\0\0"
    payload = stream.read(size)
    if len(payload) < size:
        return None
    return bool(head[0] & 0x80), head[0] & 0x0F, bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))


def development_watched(path: str) -> bool:
    """Tells whether a package path is one that hot reload applies, by the same rule as the engine: `app.json`, a file under `content`, a Lua module under `source`, and the manifest and the Lua modules of a plugin, none of them hidden or a backup."""
    parts = path.split("/")
    if any(part.startswith((".", "#")) or part.endswith("~") for part in parts) or path.endswith(BACKUP_SUFFIXES):
        return False
    if path == "app.json" or parts[0] == "content" and len(parts) > 1:
        return True
    if parts[0] == "source":
        return path.endswith(".lua")
    return len(parts) >= 3 and parts[0] == "plugins" and (parts[2:] == ["plugin.json"] or parts[2] == "source" and path.endswith(".lua"))


class DevelopmentClient:
    """One app connected to the development server, whose frames the watcher and its own handler thread send in turn."""

    def __init__(self, stream) -> None:
        self.stream = stream
        self.lock = threading.Lock()

    def send(self, opcode: int, payload: bytes) -> bool:
        try:
            with self.lock:
                self.stream.write(websocket_frame(opcode, payload))
                self.stream.flush()
            return True
        except OSError:
            return False

    def send_batch(self, message: dict, contents: list[bytes]) -> bool:
        """Sends the JSON message of a batch and then the bytes of each of its files, in one turn, so no other batch comes between them."""
        frames = [websocket_frame(WEBSOCKET_TEXT, json.dumps(message).encode())] + [websocket_frame(WEBSOCKET_BINARY, content) for content in contents]
        try:
            with self.lock:
                for frame in frames:
                    self.stream.write(frame)
                self.stream.flush()
            return True
        except OSError:
            return False


class DevelopmentServer:
    """Watches the package of an app in development and pushes every file the developer saves to every connected app, such as browser tabs, as a JSON message of the batch followed by the bytes of each file. A connection needs the token of the run, the shaders compile again as their sources change, and an app that connects late catches up: a fresh page gets the files that changed since the package it loaded was made, an app of an earlier run of the server gets every file, and an app that only lost its connection gets what changed since the package was made again."""

    def __init__(self, folder: Path, manifest: dict[str, str]) -> None:
        self.folder = folder
        self.manifest = manifest
        self.session = os.urandom(8).hex()
        self.token = base64.urlsafe_b64encode(os.urandom(18)).decode()
        self.revision = 0
        self.clients: list[DevelopmentClient] = []
        self.lock = threading.Lock()
        self.files = self.snapshot()
        self.lines = 0

    @staticmethod
    def hash_files(folder: Path, paths) -> dict[str, str]:
        return {path: hashlib.sha256((folder / path).read_bytes()).hexdigest() for path in paths}

    def snapshot(self) -> dict[str, tuple[int, int]]:
        """Records the time and size of every watched file of the package."""
        found: dict[str, tuple[int, int]] = {}
        roots = [self.folder / "app.json", self.folder / "source", self.folder / "content"]
        plugins = self.folder / "plugins"
        if plugins.is_dir():
            for plugin in plugins.iterdir():
                roots += [plugin / "plugin.json", plugin / "source"]
        for root in roots:
            for path in [root] if root.is_file() else root.rglob("*") if root.is_dir() else []:
                relative = path.relative_to(self.folder).as_posix()
                if path.is_file() and development_watched(relative):
                    stat = path.stat()
                    found[relative] = (stat.st_mtime_ns, stat.st_size)
        return found

    def scan(self) -> tuple[list[str], list[str]]:
        """Returns the files that changed and the files that went away since the last scan, leaving the files that are still being written for the next scan."""
        current = self.snapshot()
        changed = sorted(path for path, stamp in current.items() if self.files.get(path) != stamp)
        removed = sorted(path for path in self.files if path not in current)
        if changed:
            time.sleep(DEVELOPMENT_SETTLE_SECONDS)
            settled = self.snapshot()
            for path in [path for path in changed if settled.get(path) != current[path]]:
                changed.remove(path)
                if path in self.files:
                    current[path] = self.files[path]
                else:
                    del current[path]
        self.files = current
        return changed, removed

    def batch(self, changed: list[str], removed: list[str]) -> tuple[dict, list[bytes]]:
        """Reads the files of a batch, leaving out the files that are larger than the engine accepts, which the developer then ships with the build."""
        files, contents = [], []
        for path in changed:
            content = (self.folder / path).read_bytes()
            if len(content) > DEVELOPMENT_MAX_FILE:
                terminal.error(f"The file `{shown_path(self.folder / path)}` is larger than the {DEVELOPMENT_MAX_FILE} bytes that the development server pushes, so the running app keeps its old version until the app runs again.")
                continue
            files.append({"path": path, "size": len(content)})
            contents.append(content)
        return {"type": "files", "session": self.session, "revision": self.revision, "files": files, "removed": removed}, contents

    def catch_up(self, hello: dict) -> tuple[dict, list[bytes]] | None:
        """Returns the batch that brings an app that just connected up to date, or nothing when it is up to date."""
        if hello.get("session") == self.session and hello.get("revision") == self.revision:
            return None
        current = self.hash_files(self.folder, self.files)
        if hello.get("session") not in ("", self.session):
            return self.batch(sorted(current), [])
        changed = sorted(path for path, digest in current.items() if self.manifest.get(path) != digest)
        removed = sorted(path for path in self.manifest if path not in current and development_watched(path))
        return self.batch(changed, removed)

    def broadcast(self, changed: list[str], removed: list[str]) -> None:
        with self.lock:
            self.revision += 1
            message, contents = self.batch(changed, removed)
            clients = list(self.clients)
        for client in clients:
            if not client.send_batch(message, contents):
                self.drop(client)
        names = ", ".join(f'"{path}"' for path in [*changed, *removed])
        terminal.info(f"Sent {names} to {len(clients)} connected {'app' if len(clients) == 1 else 'apps'}.")

    def drop(self, client: DevelopmentClient) -> None:
        with self.lock:
            if client in self.clients:
                self.clients.remove(client)

    def watch(self, stop: threading.Event) -> None:
        """Scans the package twice a second, compiles the shaders whose sources changed, which the scan then pushes as compiled files, and reports their errors to the apps too."""
        shaders = self.folder / "content" / "shaders"
        seen = newest_shader_source(self.folder) if shaders.is_dir() else 0.0
        while not stop.wait(SHADER_WATCH_SECONDS):
            stamp = newest_shader_source(self.folder) if shaders.is_dir() else 0.0
            if stamp != seen:
                seen = stamp
                try:
                    compile_app_shaders(self.folder)
                except BuildError as error:
                    terminal.error(str(error), error.details)
                    for client in list(self.clients):
                        client.send(WEBSOCKET_TEXT, json.dumps({"type": "shaderError", "path": "content/shaders", "message": f"{error} {error.details}".strip()}).encode())
            changed, removed = self.scan()
            if changed or removed:
                self.broadcast(changed, removed)

    def report(self, message: dict) -> None:
        """Prints what a batch did in an app."""
        if message.get("restarted"):
            terminal.info(f"The app restarts: {message.get('reason', '')}")
        elif message.get("modules"):
            names = ", ".join(f'"{path}"' for path in message["modules"])
            resumed = " and resumed" if message.get("resumed") else ""
            terminal.success(f"The app reloaded {names}{resumed} in {round(message.get('milliseconds', 0))} ms.")
        elif message.get("assets"):
            names = ", ".join(f'"{path}"' for path in message["assets"])
            terminal.success(f"The app reloaded the assets {names}.")

    def accept(self, handler: http.server.BaseHTTPRequestHandler) -> None:
        """Turns a request of the development path into a WebSocket and serves it until the app leaves. A request without the token of the run is refused."""
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(handler.path).query)
        key = handler.headers.get("Sec-WebSocket-Key")
        if query.get("token") != [self.token] or key is None or handler.headers.get("Upgrade", "").lower() != "websocket":
            handler.send_error(403, "The development server needs the token of this run.")
            return
        # Browsers accept the upgrade only in an HTTP/1.1 status line, whatever version the server answers other requests with.
        handler.wfile.write(f"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: {websocket_accept(key)}\r\n\r\n".encode())
        handler.wfile.flush()
        handler.close_connection = True
        client = DevelopmentClient(handler.wfile)
        message = b""
        while (frame := read_websocket_frame(handler.rfile)) is not None:
            final, opcode, payload = frame
            if opcode == WEBSOCKET_CLOSE:
                client.send(WEBSOCKET_CLOSE, payload[:2])
                break
            if opcode == WEBSOCKET_PING:
                client.send(WEBSOCKET_PONG, payload)
                continue
            message += payload
            if not final:
                continue
            text, message = message, b""
            with contextlib.suppress(ValueError):
                self.receive(client, json.loads(text))
        self.drop(client)

    def send_line(self, line: str) -> int:
        """Sends a line of the developer console to every connected app, which runs it and answers with its result, and returns how many apps it reached."""
        with self.lock:
            self.lines += 1
            message = json.dumps({"type": "console", "id": self.lines, "line": line}).encode()
            clients = list(self.clients)
        reached = 0
        for client in clients:
            if client.send(WEBSOCKET_TEXT, message):
                reached += 1
            else:
                self.drop(client)
        if reached == 0:
            terminal.warning("No app is connected to the development server, so the line ran nowhere.")
        return reached

    def read_lines(self, stream) -> None:
        """Sends every line the developer types in the terminal to the connected apps as a line of their developer console, until the input ends."""
        for line in stream:
            if line.strip():
                self.send_line(line.strip())

    def receive(self, client: DevelopmentClient, message: dict) -> None:
        if message.get("type") == "report":
            self.report(message)
            return
        if message.get("type") == "consoleResult":
            output = str(message.get("output", ""))
            if message.get("ok"):
                terminal.success(output or "The line ran.")
            else:
                terminal.error(output or "The line failed.")
            return
        if message.get("type") != "hello":
            return
        with self.lock:
            update = self.catch_up(message)
            self.clients.append(client)
        terminal.info("An app connected to the development server.")
        if update is not None and (update[0]["files"] or update[0]["removed"]):
            client.send_batch(*update)


def command_serve(args: argparse.Namespace) -> None:
    directory = Path(args.directory).resolve()
    if not directory.is_dir():
        raise BuildError(f"The folder `{shown_path(directory)}` does not exist. Name the folder of a web page.")
    serve(directory, args.host, args.port, args.coep, args.coop, args.open)


def command_clean(_: argparse.Namespace) -> None:
    shutil.rmtree(BUILD_ROOT, ignore_errors=True)
    terminal.success(f"Removed `{shown_path(BUILD_ROOT)}`.")


def add_jobs_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--jobs", type=int, default=default_jobs(), help="Parallel jobs of the builds, one less than the processors of this machine by default.")


def add_build_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--platform", default=host_name(), choices=PLATFORMS, help="Platform of the build tree, this machine by default.")
    parser.add_argument("--config", default="Debug", choices=CONFIGS, help='Configuration of the build tree, "Debug" by default.')
    parser.add_argument("--backend", choices=["METAL", "D3D11", "GLCORE", "GLES3", "WGPU"], help="Graphics backend, the default of the platform otherwise.")
    parser.add_argument("--xcode", action="store_true", help="Use the Xcode generator on macOS.")
    add_sanitizer_option(parser)
    parser.add_argument("--target", help="CMake target to build, every target by default.")
    add_jobs_option(parser)


def add_sanitizer_option(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--sanitizers", choices=["address", "thread"], help="Build in a tree of its own with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer.")


def add_web_server_options(parser: argparse.ArgumentParser, port: int) -> None:
    parser.add_argument("--host", default="127.0.0.1", help='Address the local web server listens on, and the development server of a Debug run on a device, a simulator or this Mac. The LAN address of this machine lets phones and other computers open the page, which then runs without sound, because browsers offer "AudioWorklet" only to pages served over https or from "localhost". Apple devices reach the development server on every address of this machine unless it names one.')
    parser.add_argument("--port", type=int, default=port, help="Port of the local web server, and of the development server of a Debug run on a device, a simulator or this Mac.")
    parser.add_argument("--coep", default="require-corp", choices=["require-corp", "credentialless", "off"], help='The "Cross-Origin-Embedder-Policy" header. Pages that load third-party scripts, such as sign-in libraries, need "credentialless" or "off".')
    parser.add_argument("--coop", default="off", choices=["off", "same-origin-allow-popups", "same-origin"], help='The "Cross-Origin-Opener-Policy" header, none by default, since the single-threaded runtime needs no cross-origin isolation and "same-origin" cuts the page off from the sign-in and payment popups of plugins. The value "same-origin" with a "--coep" policy isolates the page for "SharedArrayBuffer" and threads.')
    parser.add_argument("--open", action="store_true", help="Open the page in the default browser.")


def add_app_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("app", nargs="?", help='The app folder, relative to the current folder or absolute, which holds "app.json", "source" and "content".')


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="haylen", description="Builds, tests and runs the Haylen engine, and creates, runs, checks and packages the apps that a developer names by their folder.")
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
    engine.add_argument("--platform", default="all", choices=[*ARTIFACT_PLATFORMS, "all"], help='Platform of the artifacts, "all" by default.')
    engine.add_argument("--config", default="Release", choices=CONFIGS, help='Configuration of the artifacts, "Release" by default.')
    add_jobs_option(engine)
    engine.set_defaults(handler=command_engine)

    new = commands.add_parser("new", help="Create an app with the starter code and a project of every platform template, which the developer owns.")
    new.add_argument("folder", help="Folder of the new app, which must not exist or be empty.")
    new.add_argument("--name", help="Display name, from the folder name by default.")
    new.add_argument("--identifier", help='Reverse domain identifier, "com.example.<folder>" by default.')
    new.add_argument("--orientation", default="landscape", choices=["landscape", "portrait", "any"], help='Orientation of the app, "landscape" by default.')
    new.add_argument("--kind", default="lua", choices=list(APP_KINDS), help='Kind of the app: "lua", whose code is Lua, or "cpp", a CMake project whose C++ code compiles the engine. "lua" by default.')
    new.set_defaults(handler=command_new)

    plugin = commands.add_parser("plugin", help="Add plugins to an app, remove them, list them or create a plugin.")
    actions = plugin.add_subparsers(dest="action", required=True, metavar="action")
    add_plugin = actions.add_parser("add", help='Copy a plugin folder or a plugin repository into "plugins" of an app and list it in "app.json".')
    add_plugin.add_argument("plugin", help="A plugin folder, or the git repository of a plugin, such as https://github.com/haylen-org/<plugin>.git.")
    add_plugin.add_argument("--ref", help="Branch, tag or commit of the repository, its default branch otherwise.")
    add_plugin.add_argument("--app", default=".", help="The app folder, the current folder by default.")
    add_plugin.set_defaults(handler=command_plugin_add)
    remove_plugin = actions.add_parser("remove", help='Delete a plugin from "plugins" of an app and from its "app.json".')
    remove_plugin.add_argument("id", help="Id of the plugin.")
    remove_plugin.add_argument("--app", default=".", help="The app folder, the current folder by default.")
    remove_plugin.set_defaults(handler=command_plugin_remove)
    list_plugins = actions.add_parser("list", help="List the plugins of an app with their status.")
    list_plugins.add_argument("--app", default=".", help="The app folder, the current folder by default.")
    list_plugins.set_defaults(handler=command_plugin_list)
    new_plugin = actions.add_parser("new", help="Create a plugin from templates/plugin.")
    new_plugin.add_argument("folder", help="Folder of the new plugin, named after its id, which must not exist or be empty.")
    new_plugin.add_argument("--id", help='Id of the plugin in "dash-case", the folder name by default.')
    new_plugin.set_defaults(handler=command_plugin_new)

    platform_project = commands.add_parser("platform", help="Create the project of a platform in an app or compare it with the template.")
    platform_actions = platform_project.add_subparsers(dest="action", required=True, metavar="action")
    add_platform = platform_actions.add_parser("add", help='Create "platform/<template>" of an app from the template of the platform.')
    add_app_argument(add_platform)
    add_platform.add_argument("template", choices=platform_templates(), help="The platform template to copy.")
    add_platform.set_defaults(handler=command_platform_add)
    diff_platform = platform_actions.add_parser("diff", help='Show how "platform/<template>" of an app differs from the current template, without changing anything.')
    add_app_argument(diff_platform)
    diff_platform.add_argument("--template", required=True, choices=platform_templates(), help="The platform template to compare with.")
    diff_platform.set_defaults(handler=command_platform_diff)

    run_app = commands.add_parser("run", help="Run an app: in the player of this machine with hot reload, or built from its project for a platform.")
    add_app_argument(run_app)
    run_app.add_argument("--platform", choices=list(RUN_TARGETS), help="Build the project of the app for a platform. Without it the desktop player runs the app folder in development mode.")
    run_app.add_argument("--device", help="Simulator name or id, Apple device id or Android serial.")
    run_app.add_argument("--config", default="Debug", choices=["Debug", "Release"], help='Configuration that the project of a platform builds: "Debug" with the package as it is, or "Release" with the protected release and the bootstrap of the app.')
    run_app.add_argument("--engine-config", default="Release", choices=CONFIGS, help='Configuration of the engine: of the player that runs the app on this machine, or of the artifacts the project of a platform uses. "Release" by default, and "Debug" only to debug the engine itself, because it runs many times slower.')
    add_jobs_option(run_app)
    add_web_server_options(run_app, 8000)
    run_app.set_defaults(handler=command_run)

    prepare = commands.add_parser("prepare", help='Write the folder "haylen" of the project of a platform, or the site or folder of the app, and nothing else.')
    add_app_argument(prepare)
    prepare.add_argument("--platform", required=True, choices=list(RUN_TARGETS), help="The platform whose project to prepare.")
    prepare.add_argument("--config", default="Debug", choices=["Debug", "Release"], help='Configuration that the project builds: "Debug" with the package as it is, or "Release" with the protected release and the bootstrap of the app.')
    prepare.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the project uses.")
    add_jobs_option(prepare)
    prepare.set_defaults(handler=command_prepare)

    xcodegen = commands.add_parser("xcodegen", help='Write "haylen" of the Apple project of an app and generate its "App.xcodeproj" again from "project.yml".')
    xcodegen.add_argument("app", nargs="?", default=".", help="The app folder, the current folder by default.")
    xcodegen.add_argument("--platform", default="macos", choices=list(APPLE_RUNS), help="Apple platform whose native libraries the project gets.")
    xcodegen.add_argument("--template", action="store_true", help='Generate the "App.xcodeproj" of the Apple template of the engine instead, after a change to its "project.yml".')
    xcodegen.add_argument("--config", default="Debug", choices=["Debug", "Release"], help='Configuration that the project builds: "Debug" with the package as it is, or "Release" with the protected release and the bootstrap of the app.')
    xcodegen.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the project uses.")
    add_jobs_option(xcodegen)
    xcodegen.set_defaults(handler=command_xcodegen)

    check = commands.add_parser("check", help="Check the last build of an app for a platform against what the engine and its plugins need.")
    add_app_argument(check)
    check.add_argument("--platform", required=True, choices=[name for name, target in RUN_TARGETS.items() if target.check], help="The platform whose last build to check.")
    check.add_argument("--config", default="Debug", choices=["Debug", "Release"], help="Configuration of the build to check.")
    check.add_argument("--coop", default="off", choices=["off", "same-origin-allow-popups", "same-origin"], help='The "Cross-Origin-Opener-Policy" header that the server of the site sends, on the web.')
    check.set_defaults(handler=command_check)

    android_key = commands.add_parser("android-key", help='Create the upload key of release builds, or the debug key, in "platform/android/keystore" of an app, with its certificate and the properties that sign its builds.')
    add_app_argument(android_key)
    kinds = android_key.add_mutually_exclusive_group()
    kinds.add_argument("--release", dest="kind", action="store_const", const="release", help="Create the upload key that signs release builds, which is the default.")
    kinds.add_argument("--debug", dest="kind", action="store_const", const="debug", help="Create a debug key of the project, which signs its debug builds instead of the debug key of the Android SDK.")
    android_key.set_defaults(kind="release")
    android_key.add_argument("--alias", default="upload", help='Alias of the key, "upload" by default.')
    android_key.add_argument("--password", default="upload", help=f'Password of the keystore and the key, at least {ANDROID_KEY_PASSWORD_LENGTH} characters, "upload" by default.')
    android_key.add_argument("--dname", default=ANDROID_KEY_NAME, help=f'Distinguished name of the certificate, "{ANDROID_KEY_NAME}" by default.')
    android_key.add_argument("--force", action="store_true", help="Replace a key that the project already has.")
    android_key.set_defaults(handler=command_android_key)

    run_cpp = commands.add_parser("run-cpp", help="Build and run a C++ app project, which compiles the engine through CMake.")
    run_cpp.add_argument("project", nargs="?", help='The folder of a CMake project that adds the engine and calls "haylen_add_app", relative to the current folder or absolute.')
    run_cpp.add_argument("--platform", default=host_name(), choices=CPP_RUN_PLATFORMS, help="The platform to build and run on, this machine by default.")
    run_cpp.add_argument("--target", default="main", help='The "haylen_add_app" target, "main" by default, as the C++ template names it.')
    run_cpp.add_argument("--device", help="Simulator name or id, Apple device id or Android serial.")
    run_cpp.add_argument("--config", default="Debug", choices=CONFIGS, help='Configuration of the project, "Debug" by default.')
    run_cpp.add_argument("--engine-config", default="Release", choices=CONFIGS, help='Configuration of the "haylen" Android library whose Java classes Android apps use.')
    run_cpp.add_argument("--build-only", action="store_true", help="Build and check the app without launching it, such as in continuous integration.")
    add_jobs_option(run_cpp)
    add_web_server_options(run_cpp, 8000)
    run_cpp.set_defaults(handler=command_run_cpp)

    content = commands.add_parser("content", help="Build, verify, inspect, compare, publish and compact the protected releases of an app, and show or rotate its keys.")
    content_actions = content.add_subparsers(dest="action", required=True, metavar="action")
    content_platforms = list(CONTENT_PROFILES)
    content_build = content_actions.add_parser("build", help="Build the protected release of an app for a platform into its build folder.")
    add_app_argument(content_build)
    content_build.add_argument("--platform", required=True, choices=content_platforms, help="The platform whose release to build.")
    content_build.set_defaults(handler=command_content_build)
    content_verify = content_actions.add_parser("verify", help="Check every manifest, shard and file of a protected release with the keys of the app.")
    add_app_argument(content_verify)
    content_verify.add_argument("--platform", required=True, choices=content_platforms, help="The platform whose last release to verify.")
    content_verify.add_argument("--release", help="A release folder to verify instead of the last release of the platform.")
    content_verify.set_defaults(handler=command_content_verify)
    content_inspect = content_actions.add_parser("inspect", help="List the manifests, shards and files of a protected release with their sizes, chunks and deduplication.")
    add_app_argument(content_inspect)
    content_inspect.add_argument("--platform", required=True, choices=content_platforms, help="The platform whose last release to inspect.")
    content_inspect.add_argument("--release", help="A release folder to inspect instead of the last release of the platform.")
    content_inspect.add_argument("--chunks", action="store_true", help="List every chunk of every file too.")
    content_inspect.set_defaults(handler=command_content_inspect)
    content_diff = content_actions.add_parser("diff", help="Compare two protected releases of an app: the chunks they share, add and drop, the files that changed and what an update downloads.")
    add_app_argument(content_diff)
    content_diff.add_argument("earlier", help="The folder of the earlier release.")
    content_diff.add_argument("later", help="The folder of the later release.")
    content_diff.set_defaults(handler=command_content_diff)
    content_publish = content_actions.add_parser("publish", help="Publish the content of an app as the next generation of an update channel in a tree of static files for a web server or content delivery network.")
    add_app_argument(content_publish)
    content_publish.add_argument("--platform", required=True, choices=content_platforms, help="The platform whose apps take the content.")
    content_publish.add_argument("--channel", default="stable", help='The update channel, "stable" by default.')
    content_publish.add_argument("--output", required=True, help="The folder of the publication tree, which keeps every earlier generation.")
    content_publish.set_defaults(handler=command_content_publish)
    content_compact = content_actions.add_parser("compact", help="Delete the manifests and packs of a publication tree that no channel reaches within its last generations.")
    add_app_argument(content_compact)
    content_compact.add_argument("tree", help="The folder of the publication tree.")
    content_compact.add_argument("--keep", type=int, default=CONTENT_KEPT_GENERATIONS, help=f"The generations of every channel to keep, {CONTENT_KEPT_GENERATIONS} by default.")
    content_compact.set_defaults(handler=command_content_compact)
    content_keys = content_actions.add_parser("keys", help="Show the key folder and the key IDs of an app, which are created when it has none, or add a new content key.")
    add_app_argument(content_keys)
    content_keys.add_argument("--rotate", action="store_true", help="Add a content key, which encrypts the content of later releases while the earlier keys stay for installed content.")
    content_keys.set_defaults(handler=command_content_keys)
    for action in (content_build, content_verify, content_inspect, content_diff, content_publish, content_compact, content_keys):
        action.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the content tool of the desktop artifacts.")
        add_jobs_option(action)

    dist = commands.add_parser("dist", help='Build the packages that ship a Windows or Linux app in the Release configuration: the installer of Windows, or the ".deb" and the AppImage of Linux.')
    add_app_argument(dist)
    dist.add_argument("--platform", required=True, choices=["windows", "linux"], help="The platform whose packages to build, which is this machine.")
    dist.add_argument("-o", "--output", help='Folder of the packages, "dist" in the app folder by default.')
    dist.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the project uses.")
    add_jobs_option(dist)
    dist.set_defaults(handler=command_dist)

    steam = commands.add_parser("steam", help="Build the release of an app for a desktop platform and stage its depots and build scripts for SteamPipe.")
    add_app_argument(steam)
    steam.add_argument("--platform", required=True, choices=STEAM_PLATFORMS, help="The desktop platform of the depot of the app.")
    steam.add_argument("--app-id", required=True, help="The ID of the app on Steam.")
    steam.add_argument("--depot", action="append", default=[], metavar="name=id", help="The ID of a depot: the one named after the platform, and one for every pack of the app. Repeat it for every depot.")
    steam.add_argument("--branch", help="The beta branch that the build goes live on, such as \"beta\". Without it the build waits in Steamworks to be set live.")
    steam.add_argument("--output", help="The folder of the staging, build/apps/<app>-<hash>/steam by default.")
    steam.add_argument("--engine-config", default="Release", choices=CONFIGS, help="Configuration of the engine artifacts the app links.")
    steam.add_argument("--device", help=argparse.SUPPRESS)
    add_jobs_option(steam)
    steam.set_defaults(handler=command_steam)

    package = commands.add_parser("package", help='Zip the "app.json", "source" and "content" of an app.')
    add_app_argument(package)
    package.add_argument("-o", "--output", default="app.zip", help="Zip file to write, app.zip in the current folder by default.")
    package.set_defaults(handler=command_package)

    shaders = commands.add_parser("shaders", help='Compile the shaders under "content/shaders" of an app into ".shader" files for every backend.')
    add_app_argument(shaders)
    shaders.add_argument("--force", action="store_true", help="Compile every shader, including the ones that are up to date.")
    shaders.set_defaults(handler=command_shaders)

    serve_folder = commands.add_parser("serve", help="Serve a folder with the headers WebAssembly pages need.")
    serve_folder.add_argument("directory", help="The folder of the web page.")
    add_web_server_options(serve_folder, 8000)
    serve_folder.set_defaults(handler=command_serve)

    coverage = commands.add_parser("coverage", help="Measure engine code coverage with LLVM source-based coverage.")
    add_jobs_option(coverage)
    add_sanitizer_option(coverage)
    coverage.set_defaults(handler=command_coverage)

    formatter = commands.add_parser("format", help='Format the C, C++ and Objective-C sources with "clang-format".')
    formatter.add_argument("--check", action="store_true", help="Fail instead of rewriting files.")
    formatter.set_defaults(handler=command_format)

    bench = commands.add_parser("bench", help="Build and run a benchmark of the engine in Release on this machine.")
    bench.add_argument("--suite", default="sprites", choices=["sprites", "core", "algorithms", "procedural", "physics", "ui", "render", "particles", "content", "nodes", "animation", "lua"], help="The sprite benchmark on the GPU, the core, algorithm, procedural, physics, UI, render, particle, content, node or animation benchmark on the CPU or the Lua sprite benchmark on the CPU.")
    add_jobs_option(bench)
    bench.set_defaults(handler=command_bench)

    sdk = commands.add_parser("sdk", help='Build the engine SDK and install it for "find_package(haylen)", and check that other projects consume the engine.')
    sdk.add_argument("--platform", default=host_name(), choices=sorted(DESKTOP_PLATFORMS | WEB_PLATFORMS), help="Platform of the SDK, this machine by default.")
    sdk.add_argument("--config", default="Release", choices=CONFIGS, help='Configuration of the SDK, "Release" by default.')
    sdk.add_argument("--output", help="Install prefix, build/sdk/haylen-<platform>-<config> by default.")
    sdk.add_argument("--check-consumers", nargs="*", choices=list(CONSUMER_MODES), metavar="mode", help='Then build a project that adds the engine in each listed way, "subdirectory", "cpm" or "package", or in every way without a list.')
    add_jobs_option(sdk)
    sdk.set_defaults(handler=command_sdk)

    commands.add_parser("clean", help="Remove every build tree, the engine artifacts and the build folders of apps.").set_defaults(handler=command_clean)
    return parser


def main() -> None:
    args = build_parser().parse_args()
    # The OpenSSL build that Varn adds is one step of the build, which runs as many jobs as `CMAKE_BUILD_PARALLEL_LEVEL` named when the tree was configured, so the commands never see the variable and every build keeps to its `--jobs`.
    os.environ.pop("CMAKE_BUILD_PARALLEL_LEVEL", None)
    try:
        args.handler(args)
    except BuildError as error:
        terminal.error(str(error), error.details)
        raise SystemExit(1) from None
    except KeyboardInterrupt:
        raise SystemExit(130) from None


if __name__ == "__main__":
    main()
