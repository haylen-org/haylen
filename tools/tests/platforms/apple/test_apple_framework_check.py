import re

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.platforms.apple.apple_framework_check import AppleFrameworkCheck
from tests.support.recorded_commands import RecordedCommands


class AppleFrameworkCheckTest:
    """The check of the system frameworks of a slice: a framework passes when its stub exports a symbol that the merged library leaves undefined, and the check fails with every framework that gives none, an umbrella framework that only re-exports others among them."""

    STUB = """--- !tapi-tbd
tbd-version:     4
targets:         [ arm64-ios ]
install-name:    '/System/Library/Frameworks/{name}.framework/{name}'
reexported-libraries:
  - targets:         [ arm64-ios ]
    libraries:       [ '/System/Library/Frameworks/Base.framework/Base' ]
exports:
  - targets:         [ arm64-ios ]
    symbols:         [ {symbols} ]
    objc-classes:    [ {classes} ]
reexports:
  - targets:         [ arm64-ios ]
    symbols:         [ _BaseReexported ]
...
"""
    # The symbols of `nm -g -P` of an archive: one object uses what the frameworks and a second object give, and the second object defines its own.
    SYMBOLS = """libhaylen.a[Window.o]:
_MTLCreateSystemDefaultDevice U
_OBJC_CLASS_$_CAMetalLayer U
_haylen_draw U
_BaseReexported U
libhaylen.a[Draw.o]:
_haylen_draw T 0 10
"""

    @pytest.fixture
    def check(self, tmp_path):
        recorded = RecordedCommands(tmp_path)
        sdk = tmp_path / "iPhoneOS.sdk"
        stubs = {
            "Metal": ("_MTLCreateSystemDefaultDevice", ""),
            "QuartzCore": ("_CATransform3DIdentity", "CAMetalLayer"),
            "MetalKit": ("_MTKModelIOVertexDescriptorFromMetal", "MTKView"),
            "Umbrella": ("", ""),
        }
        for name, (symbols, classes) in stubs.items():
            folder = sdk / "System" / "Library" / "Frameworks" / f"{name}.framework"
            folder.mkdir(parents=True)
            (folder / f"{name}.tbd").write_text(AppleFrameworkCheckTest.STUB.format(name=name, symbols=symbols, classes=classes), encoding="utf-8")
        outputs = {"--show-sdk-path": f"{sdk}\n", "nm": AppleFrameworkCheckTest.SYMBOLS}
        recorded.context.process.capture = lambda command, **options: outputs["nm" if "nm" in command else "--show-sdk-path"]
        return AppleFrameworkCheck(recorded.context)

    def test_frameworks_that_give_an_undefined_symbol_pass(self, check, tmp_path):
        check.check(tmp_path / "libhaylen.a", "iphoneos", ("Metal.framework", "QuartzCore.framework"))

    def test_frameworks_that_give_nothing_fail_with_their_names(self, check, tmp_path):
        with pytest.raises(BuildFailed, match=re.escape('The engine links "MetalKit.framework", "Umbrella.framework" on the slice of the SDK "iphoneos"')):
            check.check(tmp_path / "libhaylen.a", "iphoneos", ("Metal.framework", "MetalKit.framework", "Umbrella.framework"))

    def test_a_stub_gives_only_what_it_exports_itself(self):
        symbols = AppleFrameworkCheck.symbols(AppleFrameworkCheckTest.STUB.format(name="Kit", symbols="_KitStart, _KitStop", classes="KitView"))
        assert symbols == {"_KitStart", "_KitStop", "_OBJC_CLASS_$_KitView", "_OBJC_METACLASS_$_KitView"}
