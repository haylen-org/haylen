import io
import zipfile
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.repository import Repository
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.java_class_file import JavaClassFile


@dataclass(frozen=True)
class AndroidNativeMethods:
    """The native methods that the Java and Kotlin classes of the engine declare, read from the compiled classes of the Android libraries in the local Maven repository of an engine SDK, with the JNI names by which the virtual machine looks them up in the library of an app. The classes of the engine, those of the package `dev.haylen`, resolve their natives by these names, while the classes of Varn that the `haylen` library carries resolve theirs in the objects of Varn that the runtime reaches."""

    @dataclass(frozen=True)
    class Native:
        class_name: str
        method: JavaClassFile.Method

        @property
        def short_name(self) -> str:
            """The JNI name of the method, which the virtual machine looks up first."""
            return f"Java_{AndroidNativeMethods.mangle(self.class_name)}_{AndroidNativeMethods.mangle(self.method.name)}"

        @property
        def long_name(self) -> str:
            """The JNI name of the method with its arguments, which an overloaded native method takes."""
            arguments = self.method.descriptor[1 : self.method.descriptor.index(")")]
            return f"{self.short_name}__{AndroidNativeMethods.mangle(arguments)}"

        @property
        def shown(self) -> str:
            return f"{self.class_name.replace('/', '.')}.{self.method.name}"

    ENGINE_PACKAGE = "dev/haylen/"
    CLASSES = "classes.jar"
    # The escapes of JNI names for the characters of binary names that a C name cannot hold.
    ESCAPES = {"/": "_", "_": "_1", ";": "_2", "[": "_3"}

    natives: tuple[Native, ...]

    @staticmethod
    def of_sdk(maven: Path, version: str) -> AndroidNativeMethods:
        """The native methods of the classes of the engine in every Android library of the SDK, which every Android app depends on."""
        natives = []
        for library in AndroidPlatform.LIBRARIES:
            archive = maven / "dev" / "haylen" / library / version / f"{library}-{version}.aar"
            if not archive.is_file():
                raise IncompatibleSdk(f'The engine SDK lacks the Android library `{Repository.shown(archive)}`. Build the SDK again with "haylen sdk build --target android".')
            natives += AndroidNativeMethods.of_library(archive)
        return AndroidNativeMethods(tuple(natives))

    @staticmethod
    def of_library(archive: Path) -> list[Native]:
        with zipfile.ZipFile(archive) as library, zipfile.ZipFile(io.BytesIO(library.read(AndroidNativeMethods.CLASSES))) as classes:
            names = sorted(name for name in classes.namelist() if name.startswith(AndroidNativeMethods.ENGINE_PACKAGE) and name.endswith(".class"))
            compiled = [JavaClassFile.read(classes.read(name), f"`{name}` of `{Repository.shown(archive)}`") for name in names]
        return [AndroidNativeMethods.Native(each.name, method) for each in compiled for method in each.native_methods]

    @staticmethod
    def mangle(name: str) -> str:
        """Writes a binary name, a method name or the arguments of a descriptor as JNI writes them in the name of a C function."""
        written = []
        for character in name:
            if character in AndroidNativeMethods.ESCAPES:
                written.append(AndroidNativeMethods.ESCAPES[character])
            elif character.isascii() and character.isalnum():
                written.append(character)
            else:
                written += [f"_0{unit:04x}" for unit in AndroidNativeMethods.utf16_units(character)]
        return "".join(written)

    @staticmethod
    def utf16_units(character: str) -> list[int]:
        encoded = character.encode("utf-16-be")
        return [int.from_bytes(encoded[index : index + 2], "big") for index in range(0, len(encoded), 2)]

    def missing(self, symbols: set[str]) -> list[Native]:
        """The native methods whose JNI functions a library does not define, by either of their names."""
        return [native for native in self.natives if native.short_name not in symbols and native.long_name not in symbols]
