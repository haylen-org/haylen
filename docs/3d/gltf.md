# 📦 glTF

This guide explains how Haylen reads glTF 2.0 files, the one model format of the engine. It covers the two forms of a file, where their buffers and images come from, the document that the reader makes, the rules it checks, the limits that keep a hostile file harmless, the extensions it knows and what reading costs. The reader makes CPU-side data only, so a renderer, physics or a tool takes what it needs from it. The [`haylen.gltf` reference](../lua-api/gltf.md) lists every function, field and error of the Lua side.

## The two forms of a file

glTF 2.0 is the open format for 3D models and scenes. A file holds scenes of nodes, the meshes and cameras that nodes place, the materials and textures of the meshes, skins and animations, all in meters. It comes in two forms, which hold the same objects:

- **A GLB file** (`.glb`) is binary. A header of twelve bytes, with the magic `glTF`, the version `2` and the length of the file, comes first. Then comes a JSON chunk with the objects, and at most one binary chunk with the geometry, the animations and the images. One file holds the whole model, so GLB is the form to ship.
- **A `.gltf` file** is the JSON as text. Its buffers and images lie in files beside it, named by relative URIs such as `ship.bin` or `../textures/wood.png`, or inside the JSON as `data:` URIs in Base64.

Every 3D tool exports glTF, and the reader takes a file as the tool wrote it, with nothing to prepare first.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
print(#document.meshes, document.asset.generator)
```

## Files inside the package

A model reads from the content folder of the package like every asset, so its path has no `content/` prefix. A `.gltf` file names the files beside it relative to its own folder, with reserved characters percent-encoded, such as `%20` for a space. The reader refuses any URI that leaves the package: one with a scheme, such as `https://`, an absolute path such as `/etc/ship.bin`, and a path whose `..` climbs out of the content folder. Each refusal says that a glTF file reads only files inside its package.

```text
content/
  models/
    ship.gltf        names "ship.bin" and "../textures/hull.png"
    ship.bin
  textures/
    hull.png
```

Bytes in memory, such as a model that the app downloads, read as a GLB file or as a `.gltf` file whose resources are all `data:` URIs. A file beside bytes in memory does not exist, so a `.gltf` file that names one fails with the sentence that says to send a GLB file.

## Reading in three steps

A read runs in three steps, so the files beside a document can come onto the device between them, as [content delivery](../content/content.md) brings assets that download on demand:

1. **Open** reads the container and the JSON, and checks the shape of every object and the links between objects. It lists the files beside the document that its buffers and images name.
2. **Load** reads the buffers and the images: the binary chunk of a GLB file by range, the files beside a `.gltf` file, and the `data:` URIs. It checks that each image holds the format it names.
3. **Finish** checks the data of every accessor in one pass and hands the document out.

Opening and loading read files, so they belong on the I/O pool, and finishing is CPU work for the task pool. The function `gltf.readAsync` of Lua runs the steps that way. It brings the file onto the device, opens it on the I/O pool, brings the files beside it, loads them on the I/O pool and finishes on the task pool, and its promise resolves on the frame thread. The function `gltf.read` runs the steps at once on the frame thread, and fails for a file that is not on the device yet, as a synchronous load always does.

```lua
local async = require('async')
local gltf = require('haylen.gltf')

async.spawn(function()
    local document, failure = gltf.readAsync('models/ship.gltf'):await()
    if not document then
        print(failure)
        return
    end
    print(document.meshes[1].name)
end)
```

## The document

A document holds every object of the file by its index in the file: scenes, nodes, meshes with their primitives, materials, textures, samplers, images, cameras, skins, animations, accessors, buffer views and buffers. Each object keeps its `extras`, the data that the tool of the artist added, and its `extensions` as JSON. The document keeps the lists `extensionsUsed` and `extensionsRequired`, the `asset` with the version, the generator and the copyright, and the default `scene`.

- **Nodes** form trees. Each node knows its parent and its children, and gives its transform as a matrix or as translation, rotation and scale. The reader fills both forms, so a matrix also reads as its three parts.
- **Primitives** draw one material each, in one of the seven modes of glTF, from points to triangle fans, with or without indices. A primitive without a material draws with the default material of the specification, which a default `Material` is.
- **Materials** follow the metallic-roughness model: a base color factor and texture, metalness and roughness with their texture, a normal texture with its scale, an occlusion texture with its strength, an emissive texture and factor, the alpha mode with its cutoff and both faces. Each texture names the set of texture coordinates it reads at.
- **Skins** name their joints, their skeleton and their inverse bind matrices, and a primitive has as many sets of joints and weights as it needs.
- **Morph targets** move positions, normals and tangents, with default weights in the mesh and the weights of each node.
- **Animations** move the translation, the rotation, the scale and the morph weights of nodes, with the interpolations step, linear and cubic spline.
- **Cameras** are perspective, with or without a far plane, or orthographic.

An accessor reads as the numbers it stands for. Its elements follow the stride of its buffer view, the padding of the columns of small matrices is left out, the values of its sparse part replace the elements they name, and normalized integers turn into floats between `-1` or `0` and `1`. Images stay encoded until a decode asks for them, which goes through the image decoder of the engine with a largest side that the caller names.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
local primitive = document.meshes[1].primitives[1]
local positions = document:readAccessor(primitive.attributes.POSITION)
local indices = document:readUnsigned(primitive.indices)
local image = document:decodeImage(primitive.material.baseColorTexture.texture.source, {maxSize = 2048})
print(#positions / 3, #indices / 3, image.width)
```

## The rules it checks

A file that breaks a rule of glTF 2.0 is not valid, and the reader says so instead of loading something wrong. It checks every rule that the specification states as a must, with the checks of the official validator of glTF as its list:

- **The container**: the magic, the version, the declared length, the order, the sizes and the alignment of the chunks.
- **The shape of each object**: the keys it needs, the type, the range and the enum of each value, whole numbers, unique lists, and indices that name objects of the file.
- **The links**: buffer views inside their buffers with a valid stride, accessors inside their buffer views on the boundaries of their components, the formats that each use of an accessor takes, and buffer views that hold one kind of data.
- **Meshes, nodes, skins and animations**: attribute names, sets of texture coordinates without gaps, joints that pair with weights, vertex counts that the mode joins, morph targets that match, node trees without cycles, scenes of roots, matrices that split into translation, rotation and scale, skins whose joints share one hierarchy, and unique targets of animations.
- **The data**: finite floats, bounds that match the data, unit normals, tangents and rotations, colors within their range, indices that name a vertex and never restart a strip, inverse bind matrices without projection, times that grow from zero, sparse indices that grow, and weights that sum to one on unique joints of the skin. The tolerances of the lengths and the sums are those of the validator.

Every refusal names the file, the object in words and the rule, and says what to do, such as `The glTF file "models/ship.glb" is not valid, since the key "indices" of primitive 1 of mesh 0 names accessor 9, which the file lacks. Export the model again from the tool that made it, so it follows glTF 2.0.`. Indices count from zero, as the file counts them. In C++ the failure is a `gltf::ReadError`, whose `getProblem()` tells the rule apart.

## Limits

The reader treats every byte as hostile. It checks every offset, size and count in 64 bits before it uses them, walks node trees without recursion, and refuses a file that asks for more than its limits. The limits, `gltf::Limits` in C++, are:

| Limit | Default | What it bounds |
| --- | --- | --- |
| `maxJsonBytes` | 64 MiB | The JSON of the file. |
| `maxJsonDepth` | 128 | How deep objects and arrays of the JSON nest, so nothing that walks it recurses without bound. |
| `maxBufferBytes` | 1 GiB | The bytes of every buffer together, and the values that one accessor gives when it is read. |
| `maxDataUriBytes` | 32 MiB | The bytes of one `data:` URI, checked before it decodes. |
| `maxImageBytes` | 256 MiB | The encoded bytes of one image in a file beside the document. |
| `maxObjects` | 2 Mi | The objects of every list of the document together. |

## Extensions

Extensions add to glTF. The list `extensionsUsed` of a file names every extension it uses, and `extensionsRequired` names the ones it cannot load without. The reader knows the extension `EXT_texture_webp`, whose texture names a WebP image besides its fallback. A file that requires any other extension fails with a sentence that names it, and a file that requires `EXT_texture_avif` fails with a sentence that names the AVIF image format, which Haylen does not read. An extension that a file uses without requiring it keeps its JSON in the `extensions` of each object, where an app reads it.

Images are PNG and JPEG files as glTF 2.0 defines them. A file that uses `EXT_texture_webp` adds WebP images, which decode like the others through the image decoder of the engine.

## From C++

The reader lives in `haylen::gltf` in `haylen/3d/gltf/`, which the renderer, physics and tools include.

```cpp
#include "haylen/3d/gltf/AccessorReader.hpp"
#include "haylen/3d/gltf/Reader.hpp"

haylen::gltf::Reader reader = haylen::gltf::Reader::open(engine.getPackage(), "models/ship.gltf");
for (const std::string& file : reader.getExternalFiles()) {
    engine.getAssets().requireLocal(file);
}

reader.loadResources();
const haylen::gltf::Document document = reader.finish();
const std::vector<float> positions = haylen::gltf::AccessorReader::readFloats(document, *document.meshes[0].primitives[0].findAttribute("POSITION"));
```

The function `Reader::read` runs the three steps at once, from a package or from bytes in memory, and every step takes the limits as its last argument.

## Performance

The model benchmark, `haylen engine bench --suite models`, reads a skinned grid of a million vertices with normals, tangents, texture coordinates, joints and weights and two million triangles, a GLB file of 88 MiB in memory, and times each step on its own. On a Linux machine with an Intel Xeon Platinum 8488C, in the Release build:

| Step | Time |
| --- | --- |
| Open: the container, the JSON, the shape and the links | 0.03 ms |
| Open and load the buffers, which copies the binary chunk once | 46 ms |
| The rules of the data, one pass over a million vertices | 91 ms |
| Read the positions as floats | 4.3 ms |

So the rules of the data cost one linear pass, which runs on the task pool and never on the frame thread.

## Testing

The test files of the reader live in `engine/tests/data/gltf/content/`, made for the project by `tools/gltf-fixtures.py`: a cube in each of the three forms, a cylinder, an icosphere, a hierarchy with a mirrored node and instances, a skinned character, morph targets, cameras with a light, a grid of 7 by 7 materials, one file for each part of the core and the broken files under `invalid/`. The command `uv run tools/gltf-fixtures.py --check` fails when a file differs from what the tool writes. The fuzzer `haylen-gltf-fuzzer` of `haylen engine fuzz` starts from these files and also fuzzes the binary chunk and the `data:` URIs of documents whose shape is fixed.
