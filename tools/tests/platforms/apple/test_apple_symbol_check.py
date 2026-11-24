import re

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.platforms.apple.apple_symbol_check import AppleSymbolCheck
from tests.support.recorded_commands import RecordedCommands


class AppleSymbolCheckTest:
    """The check of the symbols of a slice: the routines that OpenSSL writes in assembly may stay visible, while a visible symbol of an object of OpenSSL that the compiler made, or of the namespace of Poco, fails the build with the symbols it found."""

    HEADERS = """Archive : libhaylen.a (architecture arm64)
libhaylen.a(libcrypto-lib-arm64cpuid.o) (architecture arm64):
Mach header
      magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
MH_MAGIC_64    ARM64        ALL  0x00      OBJECT     4        376 0x00000000
libhaylen.a(libcrypto-lib-evp_enc.o) (architecture arm64):
Mach header
      magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
MH_MAGIC_64    ARM64        ALL  0x00      OBJECT     5        616 SUBSECTIONS_VIA_SYMBOLS
libhaylen.a(HTTPClientSession.cpp.o) (architecture arm64):
Mach header
      magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
MH_MAGIC_64    ARM64        ALL  0x00      OBJECT     5        616 SUBSECTIONS_VIA_SYMBOLS
"""
    HIDDEN = """libhaylen.a:libcrypto-lib-arm64cpuid.o: 00000000000000e0 (__TEXT,__text) external _CRYPTO_memcmp
libhaylen.a:libcrypto-lib-arm64cpuid.o: 00000000000001b0 (__TEXT,__text) non-external .rndr_done
libhaylen.a:libcrypto-lib-evp_enc.o: 0000000000000010 (__TEXT,__text) private external _EVP_EncryptInit
libhaylen.a:HTTPClientSession.cpp.o: 0000000000000590 (__TEXT,__text) weak private external __ZN4Poco3Net17HTTPClientSession5resetEv
libhaylen.a:HTTPClientSession.cpp.o: 0000000000000600 (__DATA,__const) weak external automatically hidden __ZTVNSt3__117bad_function_callE
libhaylen.a:Window.cpp.o: 0000000000000040 (__TEXT,__text) external _haylen_native_init
"""
    VISIBLE = """libhaylen.a:libcrypto-lib-arm64cpuid.o: 00000000000000e0 (__TEXT,__text) external _CRYPTO_memcmp
libhaylen.a:libcrypto-lib-evp_enc.o: 0000000000000010 (__TEXT,__text) external _EVP_EncryptInit
libhaylen.a:HTTPClientSession.cpp.o: 0000000000000590 (__TEXT,__text) weak external __ZN4Poco3Net17HTTPClientSession5resetEv
"""

    @staticmethod
    def check(tmp_path, symbols: str) -> AppleSymbolCheck:
        recorded = RecordedCommands(tmp_path)
        outputs = {"lipo": "arm64\n", "otool": AppleSymbolCheckTest.HEADERS, "nm": symbols}
        recorded.context.process.capture = lambda command, **options: outputs[command[1]]
        return AppleSymbolCheck(recorded.context)

    def test_a_library_whose_openssl_and_poco_keep_their_symbols_hidden_passes_with_the_routines_in_assembly(self, tmp_path):
        AppleSymbolCheckTest.check(tmp_path, AppleSymbolCheckTest.HIDDEN).check(tmp_path / "libhaylen.a", "iphoneos")

    def test_a_visible_symbol_of_compiled_openssl_or_of_poco_fails_with_the_symbols(self, tmp_path):
        check = AppleSymbolCheckTest.check(tmp_path, AppleSymbolCheckTest.VISIBLE)
        expected = 'of the slice of the SDK "iphoneos" exports 2 symbols of OpenSSL or Poco, such as "_EVP_EncryptInit", "__ZN4Poco3Net17HTTPClientSession5resetEv",'
        with pytest.raises(BuildFailed, match=re.escape(expected)):
            check.check(tmp_path / "libhaylen.a", "iphoneos")
