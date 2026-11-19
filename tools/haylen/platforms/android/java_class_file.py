import struct
from dataclasses import dataclass

from haylen.core.errors.build_failed import BuildFailed


@dataclass(frozen=True)
class JavaClassFile:
    """The parts of a compiled Java or Kotlin class that the verification of Android apps reads: the binary name of the class, such as `dev/haylen/HaylenActivity`, and its native methods with their descriptors, which the library that the class loads must implement."""

    @dataclass(frozen=True)
    class Method:
        name: str
        descriptor: str

    MAGIC = bytes.fromhex("cafebabe")
    NATIVE = 0x0100
    UTF8 = 1
    CLASS = 7
    # The texts of the constants are modified UTF-8, which equals UTF-8 for the names of classes, methods and descriptors, so only those are decoded.
    # The bytes that each kind of constant holds after its tag, besides the text of a UTF-8 constant, whose length comes first.
    CONSTANT_SIZES = {3: 4, 4: 4, 5: 8, 6: 8, 7: 2, 8: 2, 9: 4, 10: 4, 11: 4, 12: 4, 15: 3, 16: 2, 17: 4, 18: 4, 19: 2, 20: 2}
    # A long or a double takes two entries of the constant pool.
    WIDE_CONSTANTS = (5, 6)

    name: str
    native_methods: tuple[Method, ...]

    @staticmethod
    def read(data: bytes, origin: str) -> JavaClassFile:
        """Reads a class file, and fails with its origin when the bytes are no class file."""
        if data[:4] != JavaClassFile.MAGIC:
            raise BuildFailed(f"The file {origin} is no Java class file. Build the engine SDK again.")
        try:
            return JavaClassFile.parse(data)
        except (struct.error, KeyError, IndexError, UnicodeDecodeError) as error:
            raise BuildFailed(f"The class file {origin} cannot be read, since it ends early or holds a constant that no class file holds. Build the engine SDK again.") from error

    @staticmethod
    def parse(data: bytes) -> JavaClassFile:
        (count,) = struct.unpack_from(">H", data, 8)
        texts: dict[int, bytes] = {}
        classes: dict[int, int] = {}
        offset = 10
        index = 1
        while index < count:
            tag = data[offset]
            offset += 1
            if tag == JavaClassFile.UTF8:
                (length,) = struct.unpack_from(">H", data, offset)
                texts[index] = data[offset + 2 : offset + 2 + length]
                offset += 2 + length
            else:
                if tag == JavaClassFile.CLASS:
                    (classes[index],) = struct.unpack_from(">H", data, offset)
                offset += JavaClassFile.CONSTANT_SIZES[tag]
            index += 2 if tag in JavaClassFile.WIDE_CONSTANTS else 1

        _, this_class, _, interfaces = struct.unpack_from(">HHHH", data, offset)
        offset += 8 + 2 * interfaces
        offset = JavaClassFile.skip_members(data, offset)

        (methods,) = struct.unpack_from(">H", data, offset)
        offset += 2
        natives = []
        for _ in range(methods):
            flags, name, descriptor, attributes = struct.unpack_from(">HHHH", data, offset)
            offset = JavaClassFile.skip_attributes(data, offset + 8, attributes)
            if flags & JavaClassFile.NATIVE:
                natives.append(JavaClassFile.Method(texts[name].decode("utf-8"), texts[descriptor].decode("utf-8")))
        return JavaClassFile(texts[classes[this_class]].decode("utf-8"), tuple(natives))

    @staticmethod
    def skip_members(data: bytes, offset: int) -> int:
        """Skips the fields of a class, which come before its methods."""
        (count,) = struct.unpack_from(">H", data, offset)
        offset += 2
        for _ in range(count):
            (attributes,) = struct.unpack_from(">H", data, offset + 6)
            offset = JavaClassFile.skip_attributes(data, offset + 8, attributes)
        return offset

    @staticmethod
    def skip_attributes(data: bytes, offset: int, count: int) -> int:
        for _ in range(count):
            (length,) = struct.unpack_from(">I", data, offset + 2)
            offset += 6 + length
        if offset > len(data):
            raise IndexError(offset)
        return offset
