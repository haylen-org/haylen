import re
from pathlib import Path

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.native.native_library import NativeLibrary
from haylen.native.static_linking import StaticLinking


class StaticLinkingTest:
    @staticmethod
    def library(static: bool, symbols: tuple[str, ...]) -> NativeLibrary:
        return NativeLibrary("vendor", "app", {}, Path("vendor"), ("console", "android", "windows"), static, symbols)

    def test_a_target_that_loads_no_dynamic_library_links_every_library_statically_with_its_symbols(self):
        listed = StaticLinkingTest.library(False, ("vendor_open",))
        StaticLinking.check([listed], "console", "console", ())
        assert StaticLinking.links_statically(listed, ())

        with pytest.raises(InvalidManifest, match=re.escape('The native library "vendor" of "app" is dynamic, and the apps of "console" load no dynamic library.')):
            StaticLinking.check([StaticLinkingTest.library(False, ())], "console", "console", ())

    @pytest.mark.parametrize("platform", ["android", "windows"])
    def test_a_target_that_loads_dynamic_libraries_refuses_a_static_one_outside_ios_and_tvos(self, platform: str):
        with pytest.raises(InvalidManifest, match=f'Leave "{platform}" out of its platforms, or make it dynamic.'):
            StaticLinking.check([StaticLinkingTest.library(True, ("vendor_open",))], platform, platform, ("dynamicLibraries",))
        assert not StaticLinking.links_statically(StaticLinkingTest.library(False, ()), ("dynamicLibraries",))
