import re

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.platforms.android.android_library_check import AndroidLibraryCheck
from tests.support.recorded_commands import RecordedCommands


class AndroidLibraryCheckTest:
    """The check of the system libraries of an ABI: the player library passes when it needs every listed library and nothing else but the libraries of the toolchain, and the check fails with a listed library that it does not need and with a needed library that the list lacks."""

    # The dynamic section of a player library as `llvm-readelf -d` prints it.
    DYNAMIC = """
Dynamic section at offset 0x1f7d0 contains 34 entries:
  Tag                Type                 Name/Value
  0x0000000000000001 (NEEDED)             Shared library: [libEGL.so]
  0x0000000000000001 (NEEDED)             Shared library: [libandroid.so]
  0x0000000000000001 (NEEDED)             Shared library: [libmediandk.so]
  0x0000000000000001 (NEEDED)             Shared library: [libdl.so]
  0x0000000000000001 (NEEDED)             Shared library: [liblog.so]
  0x0000000000000001 (NEEDED)             Shared library: [libm.so]
  0x0000000000000001 (NEEDED)             Shared library: [libc.so]
  0x000000000000000e (SONAME)             Library soname: [libhaylen.so]
  0x000000000000001e (FLAGS)              BIND_NOW
"""

    @pytest.fixture
    def recorded(self, tmp_path):
        return RecordedCommands(tmp_path, answer=lambda command: AndroidLibraryCheckTest.DYNAMIC)

    def test_a_library_that_needs_exactly_the_list_and_the_toolchain_passes(self, recorded, tmp_path):
        library = tmp_path / "libhaylen.so"
        AndroidLibraryCheck(recorded.context).check(tmp_path / "llvm-readelf", library, "arm64-v8a", ("android", "log", "EGL", "mediandk"))
        assert recorded.commands == [[str(tmp_path / "llvm-readelf"), "-d", str(library)]]

    def test_a_listed_library_that_the_player_does_not_need_fails(self, recorded, tmp_path):
        with pytest.raises(BuildFailed, match=re.escape('The engine links "libjnigraphics.so" on Android, while')):
            AndroidLibraryCheck(recorded.context).check(tmp_path / "llvm-readelf", tmp_path / "libhaylen.so", "arm64-v8a", ("android", "log", "EGL", "jnigraphics", "mediandk"))

    def test_a_needed_library_that_the_list_lacks_fails(self, recorded, tmp_path):
        with pytest.raises(BuildFailed, match=re.escape('of "x86_64" needs "libmediandk.so", which "HAYLEN_ANDROID_LIBRARIES"')):
            AndroidLibraryCheck(recorded.context).check(tmp_path / "llvm-readelf", tmp_path / "libhaylen.so", "x86_64", ("android", "log", "EGL"))
