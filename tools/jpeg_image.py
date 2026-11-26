import math
import struct


class JpegImage:
    """Writes 8-bit grayscale images as baseline JPEG files of the JPEG File Interchange Format with the Python standard library alone, with the example tables of quantization and Huffman codes of the JPEG standard, for the images that tests read. The width and the height are multiples of eight, and blocks of one gray each keep the file the same on every machine, since their coefficients other than the first round to zero."""

    QUANTIZATION = (
        (16, 11, 10, 16, 24, 40, 51, 61),
        (12, 12, 14, 19, 26, 58, 60, 55),
        (14, 13, 16, 24, 40, 57, 69, 56),
        (14, 17, 22, 29, 51, 87, 80, 62),
        (18, 22, 37, 56, 68, 109, 103, 77),
        (24, 35, 55, 64, 81, 104, 113, 92),
        (49, 64, 78, 87, 103, 121, 120, 101),
        (72, 92, 95, 98, 112, 100, 103, 99),
    )
    DC_COUNTS = (0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0)
    DC_VALUES = tuple(range(12))
    AC_COUNTS = (0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D)
    AC_VALUES = bytes.fromhex(
        "01020300041105122131410613516107227114328191a1082342b1c11552d1f024336272"
        "82090a161718191a25262728292a3435363738393a434445464748494a53545556575859"
        "5a636465666768696a737475767778797a838485868788898a92939495969798999aa2a3"
        "a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2"
        "e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8f9fa"
    )

    def __init__(self, width: int, height: int, grays: bytes) -> None:
        self.width = width
        self.height = height
        self.grays = grays

    @staticmethod
    def zigzag() -> list[int]:
        """Returns the places of a block in the order that the coefficients are written, along its diagonals from the top left, which alternate their direction."""
        order = []
        for diagonal in range(15):
            cells = [(row, diagonal - row) for row in range(8) if 0 <= diagonal - row < 8]
            if diagonal % 2 == 0:
                cells.reverse()
            order.extend(row * 8 + column for row, column in cells)
        return order

    @staticmethod
    def codes(counts: tuple[int, ...], values: bytes | tuple[int, ...]) -> dict[int, tuple[int, int]]:
        """Returns the canonical Huffman code and its length of each value of a table."""
        table = {}
        code = 0
        position = 0
        for length, count in enumerate(counts, start=1):
            for _ in range(count):
                table[values[position]] = (code, length)
                code += 1
                position += 1
            code <<= 1
        return table

    @staticmethod
    def magnitude(value: int) -> tuple[int, int]:
        """Returns the size category of a coefficient and the bits that follow its code."""
        size = abs(value).bit_length()
        return size, value if value >= 0 else value + (1 << size) - 1

    def block(self, left: int, top: int) -> list[int]:
        """Returns the quantized coefficients of the block at a corner in the order of the zigzag."""
        samples = [[self.grays[(top + y) * self.width + left + x] - 128 for x in range(8)] for y in range(8)]
        coefficients = [0] * 64
        for v in range(8):
            for u in range(8):
                total = 0.0
                for y in range(8):
                    for x in range(8):
                        total += samples[y][x] * math.cos((2 * x + 1) * u * math.pi / 16) * math.cos((2 * y + 1) * v * math.pi / 16)
                scale = 0.25 * (math.sqrt(0.5) if u == 0 else 1.0) * (math.sqrt(0.5) if v == 0 else 1.0)
                coefficients[v * 8 + u] = round(total * scale / JpegImage.QUANTIZATION[v][u])
        return [coefficients[index] for index in JpegImage.zigzag()]

    def encode(self) -> bytes:
        dc = JpegImage.codes(JpegImage.DC_COUNTS, JpegImage.DC_VALUES)
        ac = JpegImage.codes(JpegImage.AC_COUNTS, JpegImage.AC_VALUES)
        bits = []
        previous = 0
        for top in range(0, self.height, 8):
            for left in range(0, self.width, 8):
                block = self.block(left, top)
                size, extra = JpegImage.magnitude(block[0] - previous)
                previous = block[0]
                bits.append(dc[size])
                bits.append((extra, size))
                zeros = 0
                for value in block[1:]:
                    if value == 0:
                        zeros += 1
                        continue
                    while zeros > 15:
                        bits.append(ac[0xF0])
                        zeros -= 16
                    size, extra = JpegImage.magnitude(value)
                    bits.append(ac[(zeros << 4) | size])
                    bits.append((extra, size))
                    zeros = 0
                if zeros:
                    bits.append(ac[0x00])

        data = bytearray()
        accumulator = 0
        count = 0
        for value, length in bits:
            accumulator = (accumulator << length) | value
            count += length
            while count >= 8:
                byte = (accumulator >> (count - 8)) & 0xFF
                data.append(byte)
                if byte == 0xFF:
                    data.append(0)
                count -= 8
        if count:
            byte = ((accumulator << (8 - count)) | ((1 << (8 - count)) - 1)) & 0xFF
            data.append(byte)
            if byte == 0xFF:
                data.append(0)

        quantization = bytes(JpegImage.QUANTIZATION[index // 8][index % 8] for index in JpegImage.zigzag())
        segments = [
            JpegImage.segment(0xE0, b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"),
            JpegImage.segment(0xDB, b"\x00" + quantization),
            JpegImage.segment(0xC0, struct.pack(">BHHB", 8, self.height, self.width, 1) + b"\x01\x11\x00"),
            JpegImage.segment(0xC4, b"\x00" + bytes(JpegImage.DC_COUNTS) + bytes(JpegImage.DC_VALUES)),
            JpegImage.segment(0xC4, b"\x10" + bytes(JpegImage.AC_COUNTS) + JpegImage.AC_VALUES),
            JpegImage.segment(0xDA, b"\x01\x01\x00\x00\x3f\x00"),
        ]
        return b"\xff\xd8" + b"".join(segments) + bytes(data) + b"\xff\xd9"

    @staticmethod
    def segment(marker: int, payload: bytes) -> bytes:
        return bytes((0xFF, marker)) + struct.pack(">H", len(payload) + 2) + payload
