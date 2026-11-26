import base64
import json
import struct


class GltfBuilder:
    """Builds glTF 2.0 files with the Python standard library alone, for the test files of the reader of the engine: one buffer whose buffer views each start on a boundary of four bytes, accessors whose bounds are the exact bounds of their data, and the JSON of the other objects, written as a GLB file, as a `.gltf` file with its buffer beside it or as a `.gltf` file with its buffer and images in `data:` URIs."""

    GLB_MAGIC = 0x46546C67
    JSON_CHUNK = 0x4E4F534A
    BINARY_CHUNK = 0x004E4942
    # The struct codes, sizes and numbers of the component types of glTF.
    COMPONENTS = {"byte": ("b", 1, 5120), "unsignedByte": ("B", 1, 5121), "short": ("h", 2, 5122), "unsignedShort": ("H", 2, 5123), "unsignedInt": ("I", 4, 5125), "float": ("f", 4, 5126)}
    # The components and the rows of a column of each accessor type.
    TYPES = {"SCALAR": (1, 1), "VEC2": (2, 2), "VEC3": (3, 3), "VEC4": (4, 4), "MAT2": (4, 2), "MAT3": (9, 3), "MAT4": (16, 4)}
    ARRAY_BUFFER = 34962
    ELEMENT_ARRAY_BUFFER = 34963

    def __init__(self) -> None:
        self.document = {"asset": {"version": "2.0", "generator": "Haylen test files"}}
        self.binary = bytearray()

    def add(self, key: str, item: dict) -> int:
        """Appends an object to a list of the document and returns its index."""
        items = self.document.setdefault(key, [])
        items.append(item)
        return len(items) - 1

    def view(self, data: bytes, stride: int | None = None, target: int | None = None) -> int:
        """Appends bytes to the buffer, on a boundary of four bytes, as a new buffer view."""
        while len(self.binary) % 4:
            self.binary.append(0)
        view = {"buffer": 0, "byteOffset": len(self.binary), "byteLength": len(data)}
        if stride is not None:
            view["byteStride"] = stride
        if target is not None:
            view["target"] = target
        self.binary.extend(data)
        return self.add("bufferViews", view)

    @staticmethod
    def pack(values: list, component: str, kind: str) -> bytes:
        """Packs the elements of an accessor, with the padding that starts each column of a matrix on a boundary of four bytes."""
        code, size, _ = GltfBuilder.COMPONENTS[component]
        count, rows = GltfBuilder.TYPES[kind]
        data = bytearray()
        for element in values:
            element = element if isinstance(element, (list, tuple)) else (element,)
            assert len(element) == count
            for column in range(count // rows):
                for row in range(rows):
                    data.extend(struct.pack("<" + code, element[column * rows + row]))
                while count // rows > 1 and (rows * size) % 4 and len(data) % 4:
                    data.append(0)
        return bytes(data)

    @staticmethod
    def stored(value: float, component: str) -> float:
        """Returns a value as its component type stores it, a float rounded to 32 bits for floats."""
        code = GltfBuilder.COMPONENTS[component][0]
        return struct.unpack("<" + code, struct.pack("<" + code, value))[0]

    def accessor(self, values: list, component: str, kind: str, normalized: bool = False, target: int | None = None, bounds: bool = True, view: int | None = None) -> int:
        """Appends an accessor of the values, in a buffer view of its own unless one is named, with the exact bounds of its data."""
        if view is None:
            view = self.view(GltfBuilder.pack(values, component, kind), target=target)
        accessor = {"bufferView": view, "componentType": GltfBuilder.COMPONENTS[component][2], "count": len(values), "type": kind}
        if normalized:
            accessor["normalized"] = True
        if bounds:
            elements = [element if isinstance(element, (list, tuple)) else (element,) for element in values]
            stored = [[GltfBuilder.stored(value, component) for value in element] for element in elements]
            accessor["min"] = [min(element[index] for element in stored) for index in range(len(stored[0]))]
            accessor["max"] = [max(element[index] for element in stored) for index in range(len(stored[0]))]
        return self.add("accessors", accessor)

    def image(self, data: bytes, mime_type: str) -> int:
        """Appends an image held in a buffer view."""
        return self.add("images", {"bufferView": self.view(data), "mimeType": mime_type})

    def finish(self, uri: str | None) -> dict:
        """Returns the document with its buffer, named by a URI or taken from the binary chunk of a GLB file."""
        document = json.loads(json.dumps(self.document))
        if self.binary:
            buffer = {"byteLength": len(self.binary)}
            if uri is not None:
                buffer["uri"] = uri
            document["buffers"] = [buffer]
        return document

    @staticmethod
    def text(document: dict) -> bytes:
        return (json.dumps(document, indent=2) + "\n").encode()

    def glb(self) -> bytes:
        payload = json.dumps(self.finish(None), separators=(",", ":")).encode()
        payload += b" " * (-len(payload) % 4)
        chunks = struct.pack("<II", len(payload), GltfBuilder.JSON_CHUNK) + payload
        if self.binary:
            binary = bytes(self.binary) + bytes(-len(self.binary) % 4)
            chunks += struct.pack("<II", len(binary), GltfBuilder.BINARY_CHUNK) + binary
        return struct.pack("<III", GltfBuilder.GLB_MAGIC, 2, 12 + len(chunks)) + chunks

    def separate(self, binary_name: str) -> tuple[bytes, bytes]:
        """Returns the text of a `.gltf` file and the bytes of the buffer file beside it."""
        return GltfBuilder.text(self.finish(binary_name)), bytes(self.binary)

    def embedded(self) -> bytes:
        """Returns the text of a `.gltf` file whose buffer is a `data:` URI."""
        uri = "data:application/octet-stream;base64," + base64.b64encode(bytes(self.binary)).decode()
        return GltfBuilder.text(self.finish(uri))
