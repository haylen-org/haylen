"""Writes the glTF files that the tests of the reader of the engine read, in `engine/tests/data/gltf/content/`, and checks that the files there are the ones it writes.

The files are made for the project: a cube, a cylinder and an icosphere, a hierarchy with a mirrored node and instances of one mesh, a skinned character, a mesh with morph targets, cameras with a light, a grid of 7 by 7 materials and one file for each part of the core of glTF 2.0, each as a GLB file, a `.gltf` file with files beside it or a `.gltf` file with `data:` URIs, and the broken files under `invalid/` that the reader must refuse. Run it with `uv run tools/gltf-fixtures.py` to write them, and with `--check` to list the files that differ, which fails when one does.
"""

import base64
import json
import math
import struct
import sys
from pathlib import Path

from gltf_builder import GltfBuilder
from jpeg_image import JpegImage
from png_image import Image


class GltfFixtures:
    """The glTF files of the tests of the reader, which `write` makes into a map from their paths to their bytes."""

    ROOT = Path(__file__).resolve().parents[1]
    FOLDER = ROOT / "engine" / "tests" / "data" / "gltf" / "content"

    @staticmethod
    def checker(size: int, first: tuple, second: tuple) -> bytes:
        """Returns a PNG file of a checker of two colors in cells of a quarter of its side, stored without compression so its bytes never change."""
        image = Image.blank(size, size)
        cell = size // 4
        for y in range(size):
            for x in range(size):
                color = first if (x // cell + y // cell) % 2 == 0 else second
                image.pixels[(y * size + x) * 4 : (y * size + x) * 4 + 4] = bytes(color)
        return image.encode(0)

    @staticmethod
    def gray_checker() -> bytes:
        """Returns a JPEG file of 16 by 16 pixels in four blocks of 8 by 8, light and dark."""
        grays = bytes(192 if (x // 8 + y // 8) % 2 == 0 else 64 for y in range(16) for x in range(16))
        return JpegImage(16, 16, grays).encode()

    @staticmethod
    def normalize(vector: tuple) -> tuple:
        length = math.sqrt(sum(component * component for component in vector))
        return tuple(component / length for component in vector)

    @staticmethod
    def cube() -> dict:
        """Returns a unit cube with four vertices on each face, with normals, tangents and texture coordinates, and its indices."""
        faces = [((1, 0, 0), (0, 0, -1)), ((-1, 0, 0), (0, 0, 1)), ((0, 1, 0), (1, 0, 0)), ((0, -1, 0), (1, 0, 0)), ((0, 0, 1), (1, 0, 0)), ((0, 0, -1), (-1, 0, 0))]
        mesh = {"positions": [], "normals": [], "tangents": [], "uvs": [], "indices": []}
        for normal, tangent in faces:
            bitangent = (normal[1] * tangent[2] - normal[2] * tangent[1], normal[2] * tangent[0] - normal[0] * tangent[2], normal[0] * tangent[1] - normal[1] * tangent[0])
            base = len(mesh["positions"])
            for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)):
                point = tuple(0.5 * normal[axis] + (u - 0.5) * tangent[axis] + (v - 0.5) * bitangent[axis] for axis in range(3))
                mesh["positions"].append(point)
                mesh["normals"].append(normal)
                mesh["tangents"].append((*tangent, 1.0))
                mesh["uvs"].append((u, 1 - v))
            mesh["indices"].extend((base, base + 1, base + 2, base, base + 2, base + 3))
        return mesh

    @staticmethod
    def cylinder(segments: int) -> dict:
        """Returns a cylinder of radius 0.5 and height 1 with its sides and its two caps."""
        mesh = {"positions": [], "normals": [], "tangents": [], "uvs": [], "indices": []}
        for index in range(segments + 1):
            angle = 2 * math.pi * index / segments
            normal = (math.cos(angle), 0.0, -math.sin(angle))
            for height in (-0.5, 0.5):
                mesh["positions"].append((0.5 * normal[0], height, 0.5 * normal[2]))
                mesh["normals"].append(normal)
                mesh["tangents"].append((-math.sin(angle), 0.0, -math.cos(angle), 1.0))
                mesh["uvs"].append((index / segments, 0.5 - height))
        for index in range(segments):
            low, high, next_low, next_high = 2 * index, 2 * index + 1, 2 * index + 2, 2 * index + 3
            mesh["indices"].extend((low, next_low, next_high, low, next_high, high))
        for height, normal in ((0.5, (0.0, 1.0, 0.0)), (-0.5, (0.0, -1.0, 0.0))):
            center = len(mesh["positions"])
            mesh["positions"].append((0.0, height, 0.0))
            mesh["normals"].append(normal)
            mesh["tangents"].append((1.0, 0.0, 0.0, 1.0))
            mesh["uvs"].append((0.5, 0.5))
            for index in range(segments + 1):
                angle = 2 * math.pi * index / segments
                mesh["positions"].append((0.5 * math.cos(angle), height, -0.5 * math.sin(angle)))
                mesh["normals"].append(normal)
                mesh["tangents"].append((1.0, 0.0, 0.0, 1.0))
                mesh["uvs"].append((0.5 + 0.5 * math.cos(angle), 0.5 + 0.5 * math.sin(angle)))
            for index in range(segments):
                first, second = center + 1 + index, center + 2 + index
                mesh["indices"].extend((center, first, second) if height > 0 else (center, second, first))
        return mesh

    @staticmethod
    def icosphere(subdivisions: int) -> list:
        """Returns the triangles of an icosphere of radius 0.5 as a list of their corners, each a unit direction."""
        golden = (1 + math.sqrt(5)) / 2
        corners = [
            GltfFixtures.normalize(point)
            for point in [
                (-1, golden, 0),
                (1, golden, 0),
                (-1, -golden, 0),
                (1, -golden, 0),
                (0, -1, golden),
                (0, 1, golden),
                (0, -1, -golden),
                (0, 1, -golden),
                (golden, 0, -1),
                (golden, 0, 1),
                (-golden, 0, -1),
                (-golden, 0, 1),
            ]
        ]
        faces = [
            (0, 11, 5),
            (0, 5, 1),
            (0, 1, 7),
            (0, 7, 10),
            (0, 10, 11),
            (1, 5, 9),
            (5, 11, 4),
            (11, 10, 2),
            (10, 7, 6),
            (7, 1, 8),
            (3, 9, 4),
            (3, 4, 2),
            (3, 2, 6),
            (3, 6, 8),
            (3, 8, 9),
            (4, 9, 5),
            (2, 4, 11),
            (6, 2, 10),
            (8, 6, 7),
            (9, 8, 1),
        ]
        triangles = [tuple(corners[index] for index in face) for face in faces]
        for _ in range(subdivisions):
            finer = []
            for a, b, c in triangles:
                ab, bc, ca = (GltfFixtures.normalize(tuple((p + q) / 2 for p, q in zip(x, y, strict=True))) for x, y in ((a, b), (b, c), (c, a)))
                finer.extend(((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)))
            triangles = finer
        return triangles

    @staticmethod
    def add_mesh(builder: GltfBuilder, mesh: dict, material: int | None, index_type: str) -> int:
        primitive = {
            "attributes": {
                "POSITION": builder.accessor(mesh["positions"], "float", "VEC3", target=GltfBuilder.ARRAY_BUFFER),
                "NORMAL": builder.accessor(mesh["normals"], "float", "VEC3", target=GltfBuilder.ARRAY_BUFFER, bounds=False),
                "TANGENT": builder.accessor(mesh["tangents"], "float", "VEC4", target=GltfBuilder.ARRAY_BUFFER, bounds=False),
                "TEXCOORD_0": builder.accessor(mesh["uvs"], "float", "VEC2", target=GltfBuilder.ARRAY_BUFFER, bounds=False),
            },
            "indices": builder.accessor(mesh["indices"], index_type, "SCALAR", target=GltfBuilder.ELEMENT_ARRAY_BUFFER),
        }
        if material is not None:
            primitive["material"] = material
        return builder.add("meshes", {"name": "Mesh", "primitives": [primitive]})

    @staticmethod
    def textured_cube() -> GltfBuilder:
        builder = GltfBuilder()
        builder.document["asset"]["copyright"] = "Made for the tests of Haylen."
        image = builder.image(GltfFixtures.checker(16, (230, 120, 40, 255), (40, 90, 200, 255)), "image/png")
        sampler = builder.add("samplers", {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497})
        texture = builder.add("textures", {"sampler": sampler, "source": image})
        material = builder.add("materials", {"name": "Checker", "pbrMetallicRoughness": {"baseColorTexture": {"index": texture}, "metallicFactor": 0.0, "roughnessFactor": 0.5}})
        mesh = GltfFixtures.add_mesh(builder, GltfFixtures.cube(), material, "unsignedShort")
        builder.add("nodes", {"name": "Cube", "mesh": mesh, "extras": {"spin": True}})
        builder.add("scenes", {"name": "Scene", "nodes": [0]})
        builder.document["scene"] = 0
        return builder

    @staticmethod
    def separate_cube(files: dict) -> None:
        """Adds the cube as a `.gltf` file with its buffer and its image in files beside it, the image in another folder, named by a URI with a percent-encoded character."""
        builder = GltfFixtures.textured_cube()
        document = builder.finish("cube.bin")
        image = document["images"][0]
        start, length = document["bufferViews"][image["bufferView"]]["byteOffset"], document["bufferViews"][image["bufferView"]]["byteLength"]
        files["textures/checker-board.png"] = bytes(builder.binary[start : start + length])
        document["images"][0] = {"uri": "../textures/checker%2Dboard.png", "name": "Checker"}
        files["cube/cube.gltf"] = GltfBuilder.text(document)
        files["cube/cube.bin"] = bytes(builder.binary)

    @staticmethod
    def embedded_cube(files: dict) -> None:
        """Adds the cube as a `.gltf` file whose buffer and image are `data:` URIs."""
        builder = GltfFixtures.textured_cube()
        document = json.loads(builder.embedded())
        view = document["images"][0]["bufferView"]
        start, length = document["bufferViews"][view]["byteOffset"], document["bufferViews"][view]["byteLength"]
        document["images"][0] = {"uri": "data:image/png;base64," + base64.b64encode(bytes(builder.binary[start : start + length])).decode()}
        files["cube-embedded.gltf"] = GltfBuilder.text(document)

    @staticmethod
    def cylinder_file(files: dict) -> None:
        """Adds a cylinder with indices of 32 bits and a material with a normal, an occlusion and an emissive texture."""
        builder = GltfBuilder()
        normal = builder.add("textures", {"source": builder.image(GltfFixtures.checker(8, (128, 128, 255, 255), (150, 128, 230, 255)), "image/png")})
        occlusion = builder.add("textures", {"source": builder.image(GltfFixtures.checker(8, (255, 255, 255, 255), (200, 200, 200, 255)), "image/png")})
        material = builder.add(
            "materials",
            {
                "name": "Metal",
                "normalTexture": {"index": normal, "scale": 0.75},
                "occlusionTexture": {"index": occlusion, "strength": 0.5},
                "emissiveTexture": {"index": occlusion},
                "emissiveFactor": [0.25, 0.5, 1.0],
                "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.85, 1.0], "metallicFactor": 1.0, "roughnessFactor": 0.25, "metallicRoughnessTexture": {"index": occlusion}},
            },
        )
        builder.add("nodes", {"name": "Cylinder", "mesh": GltfFixtures.add_mesh(builder, GltfFixtures.cylinder(24), material, "unsignedInt")})
        builder.add("scenes", {"nodes": [0]})
        files["cylinder.glb"] = builder.glb()

    @staticmethod
    def icosphere_file(files: dict) -> None:
        """Adds an icosphere without indices, with colors of four normalized bytes, and without a material, so it draws with the default material."""
        builder = GltfBuilder()
        triangles = GltfFixtures.icosphere(1)
        corners = [corner for triangle in triangles for corner in triangle]
        positions = [tuple(0.5 * value for value in corner) for corner in corners]
        colors = [(*(round(255 * (0.5 + 0.5 * value)) for value in corner), 255) for corner in corners]
        primitive = {
            "attributes": {
                "POSITION": builder.accessor(positions, "float", "VEC3"),
                "NORMAL": builder.accessor(corners, "float", "VEC3", bounds=False),
                "COLOR_0": builder.accessor(colors, "unsignedByte", "VEC4", normalized=True, bounds=False),
            }
        }
        builder.add("meshes", {"name": "Icosphere", "primitives": [primitive]})
        builder.add("nodes", {"name": "Icosphere", "mesh": 0})
        builder.add("scenes", {"nodes": [0]})
        files["icosphere.glb"] = builder.glb()

    @staticmethod
    def hierarchy_file(files: dict) -> None:
        """Adds two scenes, the second the default one, with a parent given by a matrix, children that share one mesh and one child mirrored by a negative scale."""
        builder = GltfBuilder()
        mesh = GltfFixtures.add_mesh(builder, GltfFixtures.cube(), None, "unsignedByte")
        quarter = math.sqrt(0.5)
        builder.add("nodes", {"name": "Root", "matrix": [0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 2, 3, 1], "children": [1, 2, 3]})
        builder.add("nodes", {"name": "Left", "mesh": mesh, "translation": [-2, 0, 0]})
        builder.add("nodes", {"name": "Mirrored", "mesh": mesh, "translation": [2, 0, 0], "scale": [-1, 1, 1]})
        builder.add("nodes", {"name": "Turned", "mesh": mesh, "rotation": [0, quarter, 0, quarter], "children": [4]})
        builder.add("nodes", {"name": "Leaf"})
        builder.add("nodes", {"name": "Alone", "mesh": mesh})
        builder.add("scenes", {"name": "Solo", "nodes": [5]})
        builder.add("scenes", {"name": "Family", "nodes": [0, 5]})
        builder.document["scene"] = 1
        files["hierarchy.glb"] = builder.glb()

    @staticmethod
    def character_file(files: dict) -> None:
        """Adds a skinned column of three joints with two sets of joints and weights and an animation of every path of a node with every interpolation."""
        builder = GltfBuilder()
        positions, joints, weights, more_joints, more_weights = [], [], [], [], []
        for level in range(4):
            for x, z in ((-0.25, -0.25), (0.25, -0.25), (0.25, 0.25), (-0.25, 0.25)):
                positions.append((x, float(level), z))
                lower = min(level, 2)
                joints.append((lower, min(lower + 1, 2), 0, 0))
                weights.append((0.5, 0.25, 0.0, 0.0) if lower < 2 else (0.75, 0.0, 0.0, 0.0))
                more_joints.append((0 if lower > 0 else 2, 0, 0, 0))
                more_weights.append((0.25, 0.0, 0.0, 0.0))
        indices = []
        for level in range(3):
            for side in range(4):
                a, b = level * 4 + side, level * 4 + (side + 1) % 4
                indices.extend((a, b, b + 4, a, b + 4, a + 4))
        primitive = {
            "attributes": {
                "POSITION": builder.accessor(positions, "float", "VEC3"),
                "JOINTS_0": builder.accessor(joints, "unsignedByte", "VEC4", bounds=False),
                "WEIGHTS_0": builder.accessor(weights, "float", "VEC4", bounds=False),
                "JOINTS_1": builder.accessor(more_joints, "unsignedShort", "VEC4", bounds=False),
                "WEIGHTS_1": builder.accessor(more_weights, "float", "VEC4", bounds=False),
            },
            "indices": builder.accessor(indices, "unsignedShort", "SCALAR"),
        }
        mesh = builder.add("meshes", {"name": "Column", "primitives": [primitive]})
        inverse = [(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -float(level), 0, 1) for level in range(3)]
        matrices = builder.accessor(inverse, "float", "MAT4", bounds=False)
        builder.add("nodes", {"name": "Character", "children": [1, 4]})
        builder.add("nodes", {"name": "Body", "mesh": mesh, "skin": 0})
        builder.add("nodes", {"name": "Hip", "children": [3]})
        builder.add("nodes", {"name": "Spine", "translation": [0, 1, 0], "children": [5]})
        builder.add("nodes", {"name": "Skeleton", "children": [2]})
        builder.add("nodes", {"name": "Head", "translation": [0, 1, 0]})
        builder.add("skins", {"name": "Rig", "inverseBindMatrices": matrices, "skeleton": 4, "joints": [2, 3, 5]})
        builder.add("scenes", {"nodes": [0]})
        times = builder.accessor([0.0, 0.5, 1.0], "float", "SCALAR")
        half = math.sqrt(0.5)
        rotations = builder.accessor([(0, 0, 0, 1), (0, 0, half, half), (0, 0, 0, 1)], "float", "VEC4", bounds=False)
        steps = builder.accessor([(0, 1, 0), (0, 1.25, 0), (0, 1, 0)], "float", "VEC3", bounds=False)
        spline = builder.accessor([(0, 0, 0), (1, 1, 1), (0, 0, 0), (0, 0, 0), (1.5, 1.5, 1.5), (0, 0, 0), (0, 0, 0), (1, 1, 1), (0, 0, 0)], "float", "VEC3", bounds=False)
        samplers = [{"input": times, "output": rotations}, {"input": times, "output": steps, "interpolation": "STEP"}, {"input": times, "output": spline, "interpolation": "CUBICSPLINE"}]
        channels = [{"sampler": 0, "target": {"node": 3, "path": "rotation"}}, {"sampler": 1, "target": {"node": 5, "path": "translation"}}, {"sampler": 2, "target": {"node": 5, "path": "scale"}}]
        builder.add("animations", {"name": "Wave", "samplers": samplers, "channels": channels})
        files["character.glb"] = builder.glb()

    @staticmethod
    def morph_file(files: dict) -> None:
        """Adds a quad with two morph targets of positions, normals and tangents, the first sparse, with default weights, the weights of its node and an animation of the weights in normalized bytes."""
        builder = GltfBuilder()
        positions = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        attributes = {
            "POSITION": builder.accessor(positions, "float", "VEC3"),
            "NORMAL": builder.accessor([(0, 0, 1)] * 4, "float", "VEC3", bounds=False),
            "TANGENT": builder.accessor([(1, 0, 0, 1)] * 4, "float", "VEC4", bounds=False),
        }
        indices = builder.accessor([0, 1, 2, 0, 2, 3], "unsignedByte", "SCALAR")
        raised = builder.add(
            "accessors",
            {
                "componentType": 5126,
                "count": 4,
                "type": "VEC3",
                "min": [0, 0, 0],
                "max": [0, 0, 0.5],
                "sparse": {"count": 1, "indices": {"bufferView": builder.view(struct.pack("<B", 2)), "componentType": 5121}, "values": {"bufferView": builder.view(struct.pack("<3f", 0, 0, 0.5))}},
            },
        )
        stretched = builder.accessor([(0, 0, 0), (0.5, 0, 0), (0.5, 0, 0), (0, 0, 0)], "float", "VEC3")
        zero = builder.accessor([(0, 0, 0)] * 4, "float", "VEC3", bounds=False)
        targets = [{"POSITION": raised, "NORMAL": zero, "TANGENT": zero}, {"POSITION": stretched}]
        builder.add("meshes", {"name": "Quad", "primitives": [{"attributes": attributes, "indices": indices, "targets": targets}], "weights": [0.0, 0.5]})
        builder.add("nodes", {"name": "Quad", "mesh": 0, "weights": [0.25, 0.75]})
        builder.add("scenes", {"nodes": [0]})
        times = builder.accessor([0.0, 1.0], "float", "SCALAR")
        weights = builder.accessor([0, 255, 255, 0], "unsignedByte", "SCALAR", normalized=True, bounds=False)
        builder.add("animations", {"samplers": [{"input": times, "output": weights}], "channels": [{"sampler": 0, "target": {"node": 0, "path": "weights"}}]})
        files["morph.glb"] = builder.glb()

    @staticmethod
    def camera_file(files: dict) -> None:
        """Adds cameras of both projections, one perspective without a far plane, and a light of an extension the file uses without requiring it."""
        builder = GltfBuilder()
        builder.document["extensionsUsed"] = ["KHR_lights_punctual"]
        builder.document["extensions"] = {"KHR_lights_punctual": {"lights": [{"type": "point", "color": [1, 0.9, 0.8], "intensity": 20}]}}
        builder.add("cameras", {"name": "Lens", "type": "perspective", "perspective": {"aspectRatio": 1.5, "yfov": 0.8, "zfar": 100, "znear": 0.1}})
        builder.add("cameras", {"name": "Endless", "type": "perspective", "perspective": {"yfov": 1.0, "znear": 0.05}})
        builder.add("cameras", {"name": "Plan", "type": "orthographic", "orthographic": {"xmag": 4, "ymag": 3, "zfar": 50, "znear": 0}})
        for camera in range(3):
            builder.add("nodes", {"camera": camera, "translation": [0, 0, 5 + camera]})
        builder.add("nodes", {"name": "Lamp", "translation": [0, 3, 0], "extensions": {"KHR_lights_punctual": {"light": 0}}})
        builder.add("scenes", {"nodes": [0, 1, 2, 3]})
        files["camera-light.gltf"] = GltfBuilder.text(builder.finish(None))

    @staticmethod
    def material_grid_file(files: dict) -> None:
        """Adds a grid of 7 by 7 spheres that share one sphere, with metalness growing along one axis and roughness along the other."""
        builder = GltfBuilder()
        corners = [corner for triangle in GltfFixtures.icosphere(1) for corner in triangle]
        position = builder.accessor([tuple(0.5 * value for value in corner) for corner in corners], "float", "VEC3")
        normal = builder.accessor(corners, "float", "VEC3", bounds=False)
        for row in range(7):
            for column in range(7):
                material = builder.add(
                    "materials", {"name": f"Metal {row} rough {column}", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.6, 0.2, 1.0], "metallicFactor": row / 6, "roughnessFactor": column / 6}}
                )
                mesh = builder.add("meshes", {"primitives": [{"attributes": {"POSITION": position, "NORMAL": normal}, "material": material}]})
                builder.add("nodes", {"mesh": mesh, "translation": [column * 1.25, row * 1.25, 0]})
        builder.add("scenes", {"nodes": list(range(49))})
        files["material-grid.glb"] = builder.glb()

    @staticmethod
    def primitive_modes_file(files: dict) -> None:
        """Adds one primitive of each mode of glTF, each with a vertex count that its mode joins."""
        builder = GltfBuilder()
        counts = {0: 3, 1: 4, 2: 3, 3: 3, 4: 3, 5: 4, 6: 4}
        primitives = []
        for mode, count in counts.items():
            points = [(math.cos(index), math.sin(index), float(mode)) for index in range(count)]
            primitives.append({"attributes": {"POSITION": builder.accessor(points, "float", "VEC3")}, "mode": mode})
        builder.add("meshes", {"primitives": primitives})
        builder.add("nodes", {"mesh": 0})
        builder.add("scenes", {"nodes": [0]})
        files["primitive-modes.gltf"] = builder.embedded()

    @staticmethod
    def accessor_types_file(files: dict) -> None:
        """Adds accessors of every type with every component type that glTF stores, the columns of small matrices padded, a sparse accessor over a buffer view and an accessor of an attribute of the app."""
        builder = GltfBuilder()
        builder.accessor([(1, 2, 3, 4), (5, 6, 7, 8)], "unsignedByte", "MAT2")
        builder.accessor([tuple(range(9)), tuple(range(-9, 0))], "short", "MAT3")
        builder.accessor([tuple(float(value) for value in range(16))], "float", "MAT4")
        builder.accessor([(-127, 127), (0, 64)], "byte", "VEC2", normalized=True)
        builder.accessor([(0, 32767, -32767)], "short", "VEC3", normalized=True)
        builder.accessor([(65535, 0, 1, 2)], "unsignedShort", "VEC4", normalized=True)
        builder.accessor([4000000000], "unsignedInt", "SCALAR")
        base = builder.accessor([(1.0, 1.0), (2.0, 2.0), (3.0, 3.0)], "float", "VEC2", bounds=False)
        base_view = builder.document["accessors"][base]["bufferView"]
        builder.add(
            "accessors",
            {
                "bufferView": base_view,
                "componentType": 5126,
                "count": 3,
                "type": "VEC2",
                "sparse": {
                    "count": 2,
                    "indices": {"bufferView": builder.view(struct.pack("<2H", 0, 2)), "componentType": 5123},
                    "values": {"bufferView": builder.view(struct.pack("<4f", 9, 9, 7, 7))},
                },
            },
        )
        temperature = builder.accessor([20.5, 21.0, 22.5], "float", "SCALAR")
        position = builder.accessor([(0, 0, 0), (1, 0, 0), (0, 1, 0)], "float", "VEC3")
        builder.add("meshes", {"primitives": [{"attributes": {"POSITION": position, "_TEMPERATURE": temperature}}]})
        builder.add("nodes", {"mesh": 0})
        builder.add("scenes", {"nodes": [0]})
        files["accessor-types.gltf"] = builder.embedded()

    @staticmethod
    def interleaved_file(files: dict) -> None:
        """Adds a triangle whose positions, normals and texture coordinates interleave in one buffer view with a stride of 32 bytes."""
        builder = GltfBuilder()
        vertices = [((0, 0, 0), (0, 0, 1), (0, 0)), ((1, 0, 0), (0, 0, 1), (1, 0)), ((0, 1, 0), (0, 0, 1), (0, 1))]
        data = b"".join(struct.pack("<8f", *position, *normal, *uv) for position, normal, uv in vertices)
        view = builder.view(data, stride=32, target=GltfBuilder.ARRAY_BUFFER)
        position = builder.add("accessors", {"bufferView": view, "byteOffset": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0]})
        normal = builder.add("accessors", {"bufferView": view, "byteOffset": 12, "componentType": 5126, "count": 3, "type": "VEC3"})
        uv = builder.add("accessors", {"bufferView": view, "byteOffset": 24, "componentType": 5126, "count": 3, "type": "VEC2"})
        builder.add("meshes", {"primitives": [{"attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv}}]})
        builder.add("nodes", {"mesh": 0})
        builder.add("scenes", {"nodes": [0]})
        files["interleaved.glb"] = builder.glb()

    @staticmethod
    def samplers_file(files: dict) -> None:
        """Adds textures with every filter and wrap mode, a JPEG image in a buffer view and a PNG image in a `data:` URI, and materials of every alpha mode, double-sided, that read a second set of texture coordinates."""
        builder = GltfBuilder()
        jpeg = builder.image(GltfFixtures.gray_checker(), "image/jpeg")
        png = builder.add("images", {"uri": "data:image/png;base64," + base64.b64encode(GltfFixtures.checker(4, (255, 255, 255, 255), (0, 0, 0, 128))).decode()})
        filters = [(9728, 9728), (9729, 9729), (9728, 9984), (9729, 9985), (9728, 9986), (9729, 9987)]
        wraps = [(33071, 33648), (33648, 10497), (10497, 33071)]
        for index, (magnify, minify) in enumerate(filters):
            wrap_s, wrap_t = wraps[index % 3]
            sampler = builder.add("samplers", {"magFilter": magnify, "minFilter": minify, "wrapS": wrap_s, "wrapT": wrap_t})
            builder.add("textures", {"sampler": sampler, "source": jpeg if index % 2 == 0 else png})
        builder.add("textures", {"source": png})
        builder.add("materials", {"name": "Masked", "alphaMode": "MASK", "alphaCutoff": 0.25, "doubleSided": True, "pbrMetallicRoughness": {"baseColorTexture": {"index": 1, "texCoord": 1}}})
        builder.add("materials", {"name": "Blended", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 0.5], "baseColorTexture": {"index": 6}}})
        builder.add("materials", {"name": "Solid", "alphaMode": "OPAQUE"})
        positions = builder.accessor([(0, 0, 0), (1, 0, 0), (0, 1, 0)], "float", "VEC3")
        first = builder.accessor([(0, 0), (1, 0), (0, 1)], "float", "VEC2", bounds=False)
        second = builder.accessor([(0, 0), (65535, 0), (0, 65535)], "unsignedShort", "VEC2", normalized=True, bounds=False)
        colors = builder.accessor([(1, 0, 0), (0, 1, 0), (0, 0, 1)], "float", "VEC3", bounds=False)
        primitives = [{"attributes": {"POSITION": positions, "TEXCOORD_0": first, "TEXCOORD_1": second, "COLOR_0": colors}, "material": material} for material in range(3)]
        builder.add("meshes", {"primitives": primitives})
        builder.add("nodes", {"mesh": 0})
        builder.add("scenes", {"nodes": [0]})
        files["samplers.gltf"] = builder.embedded()

    @staticmethod
    def invalid_files(files: dict) -> None:
        """Adds the broken files that the reader must refuse, each with the one mistake its name says."""
        cube = GltfFixtures.textured_cube().glb()
        files["invalid/truncated.glb"] = cube[: len(cube) // 2]
        files["invalid/wrong-magic.glb"] = b"glTX" + cube[4:]
        files["invalid/wrong-length.glb"] = cube[:8] + struct.pack("<I", len(cube) + 4) + cube[12:]
        base = json.loads(GltfFixtures.textured_cube().embedded())
        files["invalid/corrupt-json.gltf"] = GltfBuilder.text(base)[:-20] + b"]\n"

        def variant(name: str, change) -> None:
            document = json.loads(json.dumps(base))
            change(document)
            files["invalid/" + name] = GltfBuilder.text(document)

        variant("accessor-outside-buffer.gltf", lambda document: document["accessors"][0].update({"byteOffset": 4096}))
        variant("uri-parent.gltf", lambda document: document["buffers"][0].update({"uri": "../../outside.bin"}))
        variant("uri-absolute.gltf", lambda document: document["buffers"][0].update({"uri": "/etc/cube.bin"}))
        variant("uri-network.gltf", lambda document: document["buffers"][0].update({"uri": "https://example.com/cube.bin"}))
        variant("invalid-base64.gltf", lambda document: document["buffers"][0].update({"uri": "data:application/octet-stream;base64,AAAA*AAA"}))
        files["invalid/data-uri-too-large.gltf"] = GltfBuilder.text(base)

        duplicate = json.loads(json.dumps(base))
        builder = GltfBuilder()
        builder.binary = bytearray(base64.b64decode(duplicate["buffers"][0]["uri"].split(",", 1)[1]))
        builder.document = duplicate
        indices = builder.view(struct.pack("<2B", 1, 1))
        values = builder.view(struct.pack("<6f", 0, 0, 0, 0, 0, 0))
        builder.add(
            "accessors", {"componentType": 5126, "count": 4, "type": "VEC3", "sparse": {"count": 2, "indices": {"bufferView": indices, "componentType": 5121}, "values": {"bufferView": values}}}
        )
        duplicate["buffers"] = []
        del duplicate["buffers"]
        files["invalid/duplicate-sparse-index.gltf"] = builder.embedded()

    @staticmethod
    def write() -> dict:
        files = {}
        files["cube.glb"] = GltfFixtures.textured_cube().glb()
        GltfFixtures.separate_cube(files)
        GltfFixtures.embedded_cube(files)
        GltfFixtures.cylinder_file(files)
        GltfFixtures.icosphere_file(files)
        GltfFixtures.hierarchy_file(files)
        GltfFixtures.character_file(files)
        GltfFixtures.morph_file(files)
        GltfFixtures.camera_file(files)
        GltfFixtures.material_grid_file(files)
        GltfFixtures.primitive_modes_file(files)
        GltfFixtures.accessor_types_file(files)
        GltfFixtures.interleaved_file(files)
        GltfFixtures.samplers_file(files)
        GltfFixtures.invalid_files(files)
        return files

    @staticmethod
    def differences() -> list[str]:
        """Returns the files that are missing, differ from what the tool writes or are not its own."""
        files = GltfFixtures.write()
        found = {str(path.relative_to(GltfFixtures.FOLDER)) for path in GltfFixtures.FOLDER.rglob("*") if path.is_file()} if GltfFixtures.FOLDER.exists() else set()
        wrong = [name for name, data in files.items() if not (GltfFixtures.FOLDER / name).exists() or (GltfFixtures.FOLDER / name).read_bytes() != data]
        return sorted(wrong + sorted(found - set(files)))

    @staticmethod
    def main() -> None:
        if sys.argv[1:] == ["--check"]:
            wrong = GltfFixtures.differences()
            for name in wrong:
                print(f"The test file `{name}` differs from what `tools/gltf-fixtures.py` writes, so run it again.")
            if wrong:
                sys.exit(1)
            print("Every glTF test file is the one the tool writes.")
            return
        for name, data in GltfFixtures.write().items():
            path = GltfFixtures.FOLDER / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        print(f"Wrote the glTF test files in `{GltfFixtures.FOLDER.relative_to(GltfFixtures.ROOT)}`.")


if __name__ == "__main__":
    GltfFixtures.main()
