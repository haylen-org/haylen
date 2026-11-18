import os
import shutil
import subprocess
from pathlib import Path

import pytest

from haylen.core.host import Host
from haylen.core.repository import Repository


class CatalystToolchainTest:
    """The toolchain of Mac Catalyst compiles and links against the `iOSSupport` folder of the macOS SDK, which lives inside the folder of Xcode, whose name may hold a space."""

    TOOLCHAIN = Repository.ENGINE / "cmake" / "haylen-catalyst.toolchain.cmake"

    @pytest.fixture(autouse=True)
    def needs_xcode(self):
        if Host.system() != "macos" or shutil.which("cmake") is None or shutil.which("ninja") is None:
            pytest.skip("The toolchain of Mac Catalyst builds on macOS with CMake and Ninja.")

    @staticmethod
    def spaced_sdk(folder: Path) -> Path:
        """Puts an `xcrun` first on the path that answers the macOS SDK through a folder whose name holds a space and runs the real one for everything else."""
        real = subprocess.run(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"], capture_output=True, text=True, check=True).stdout.strip()
        spaced = folder / "Xcode Folder" / "MacOSX.sdk"
        spaced.parent.mkdir(parents=True)
        spaced.symlink_to(real)

        xcrun = folder / "bin" / "xcrun"
        xcrun.parent.mkdir()
        xcrun.write_text(f'#!/bin/sh\nif [ "$*" = "--sdk macosx --show-sdk-path" ]; then\n  echo "{spaced}"\n  exit 0\nfi\nexec /usr/bin/xcrun "$@"\n', encoding="utf-8")
        xcrun.chmod(0o755)
        return xcrun.parent

    def test_an_app_builds_when_the_folder_of_xcode_holds_a_space(self, tmp_path):
        tools = CatalystToolchainTest.spaced_sdk(tmp_path)
        project = tmp_path / "project"
        project.mkdir()
        (project / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.28)\nproject(probe OBJC)\nadd_executable(probe main.m)\ntarget_link_libraries(probe "-framework UIKit")\n', encoding="utf-8"
        )
        (project / "main.m").write_text("#import <UIKit/UIKit.h>\n\nint main(int count, char* arguments[]) {\n    return UIApplicationMain(count, arguments, nil, nil);\n}\n", encoding="utf-8")

        environment = {**os.environ, "PATH": f"{tools}{os.pathsep}{os.environ['PATH']}"}
        build = tmp_path / "build"
        configure = ["cmake", "-S", str(project), "-B", str(build), "-G", "Ninja", f"-DCMAKE_TOOLCHAIN_FILE={CatalystToolchainTest.TOOLCHAIN}", "-DCMAKE_OSX_ARCHITECTURES=arm64"]
        configured = subprocess.run(configure, capture_output=True, text=True, env=environment, check=False)
        assert configured.returncode == 0, configured.stdout + configured.stderr

        built = subprocess.run(["cmake", "--build", str(build)], capture_output=True, text=True, env=environment, check=False)
        assert built.returncode == 0, built.stdout + built.stderr
