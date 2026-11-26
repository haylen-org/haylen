# 📖 `haylen.gltf`

The module `haylen.gltf` reads glTF 2.0 files of the package into a read-only `Document`, without making anything to draw. Use it in tools and in apps that read what a model holds, such as its node names, its animations or its `extras`. The [glTF guide](../3d/gltf.md) explains the two forms of a file, where their resources come from, the rules that the reader checks and its limits.

```lua
local gltf = require('haylen.gltf')
```

## Functions

### gltf.read(path)

Reads the `.glb` or `.gltf` file at a path of the content folder, with the files beside it, and returns its `Document`. It runs on the frame thread, so it suits small files and tools. It fails for a file that is not on the device yet, and so does it for a file beside the document, since a synchronous read never waits for a download.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
for _, node in ipairs(document.nodes) do
    print(node.index, node.name)
end
```

### gltf.readAsync(path)

Reads the same as `gltf.read` off the frame thread and returns a promise of the `Document`. It first brings the file onto the device, then the files beside it that the document names, opens and loads them on the I/O pool and checks the data on the task pool. A failure rejects the promise with the sentence of the reader.

```lua
local async = require('async')
local gltf = require('haylen.gltf')

async.spawn(function()
    local document, failure = gltf.readAsync('models/ship.gltf'):await()
    if not document then
        print(failure)
        return
    end
    print(document.asset.generator)
end)
```

## Document

A read-only glTF document. Its properties are Lua tables that form one graph: a reference between objects is the table of the object itself, so `document.nodes[1].mesh.primitives[1].material` is a material table of `document.materials`. The graph builds once, the first time a property is read, and every read returns the same tables. Every object table holds `index`, its place in its list of the file counted from zero, as the file and the messages of the reader count it, so `document.nodes[1].index` is `0`. Objects keep the `name`, the `extras` and the `extensions` that the file gives them, as Lua values, and a field the file leaves out is nil. Enums are the names that the [enum names](#enum-names) list.

### Document properties

| Property | Type | Meaning |
| --- | --- | --- |
| `file` | string | The path the document was read from. |
| `asset` | table | `{version, minVersion, generator, copyright, extras}` of the file. |
| `scene` | table or nil | The scene to show when the app names none. |
| `scenes` | list | Scenes, each `{index, name, nodes}` with its root nodes. |
| `nodes` | list | [Nodes](#nodes). |
| `meshes` | list | [Meshes](#meshes) with their primitives. |
| `materials` | list | [Materials](#materials). |
| `textures` | list | Textures, each `{index, name, sampler, source}`, where `source` is an image and both may be nil. |
| `samplers` | list | Samplers, each `{index, name, magFilter, minFilter, wrapS, wrapT}`, with nil filters that the renderer chooses. |
| `images` | list | Images, each `{index, name, storage, bufferView, uri, path, mimeType}`. The `uri` and `path`, the path in the content folder, are those of an image in a file beside the document. |
| `cameras` | list | [Cameras](#cameras). |
| `skins` | list | Skins, each `{index, name, inverseBindMatrices, skeleton, joints}`, with an accessor, a node and a list of nodes. |
| `animations` | list | [Animations](#animations). |
| `accessors` | list | [Accessors](#accessors). |
| `bufferViews` | list | Buffer views, each `{index, name, buffer, byteOffset, byteLength, byteStride, target}`. |
| `buffers` | list | Buffers, each `{index, name, storage, uri, path, byteLength}`. |
| `extensionsUsed` | list | The names of the extensions the file uses. |
| `extensionsRequired` | list | The names of the extensions the file needs. |
| `extras` | any | The `extras` of the document. |
| `extensions` | table or nil | The `extensions` of the document, such as the lights of `KHR_lights_punctual`. |

### Nodes

A node is `{index, name, children, parent, camera, mesh, skin, matrix, translation, rotation, scale, localMatrix, weights}`. The `children` are node tables and `parent` the one node whose children list it. The `matrix`, a `Mat4`, is there only when the file gives one, and `translation` and `rotation`, a `Vec3` and a `Quat`, and `scale` always hold the transform, split from the matrix when the file gives one. The `localMatrix` is the transform relative to the parent, and `weights` the morph weights that replace those of the mesh, empty when the node keeps them.

### Meshes

A mesh is `{index, name, primitives, weights}`, with the default morph `weights`. Each primitive is `{attributes, indices, material, mode, targets, extras, extensions}`: `attributes` maps each attribute name, such as `POSITION` or `TEXCOORD_1`, to its accessor, `indices` is an accessor or nil, `material` a material or nil for the default material, and `targets` a list of morph targets, each mapping attribute names to accessors.

### Materials

A material is `{index, name, baseColorFactor, baseColorTexture, metallicFactor, roughnessFactor, metallicRoughnessTexture, normalTexture, normalScale, occlusionTexture, occlusionStrength, emissiveTexture, emissiveFactor, alphaMode, alphaCutoff, doubleSided, pbrExtras, pbrExtensions}`. The `baseColorFactor` is a `Vec4` and the `emissiveFactor` a `Vec3`, both linear. Each texture, nil when the material has none, is `{texture, texCoord, extras, extensions}`, where `texCoord` names the set of texture coordinates it reads at.

### Cameras

A camera is `{index, name, type}` with `perspective = {aspectRatio, yfov, zfar, znear}`, whose `aspectRatio` and `zfar` may be nil, or `orthographic = {xmag, ymag, zfar, znear}`. Distances are in meters and `yfov` in radians.

### Animations

An animation is `{index, name, duration, samplers, channels}`. Its `duration` is the latest time of its samplers in seconds. Each sampler is `{index, input, output, interpolation}`, with the accessors of the times and of the values. Each channel is `{sampler, node, path, extras, extensions, targetExtras, targetExtensions}`, where `sampler` is a sampler table of the same animation and `node` nil when an extension names the target.

### Accessors

An accessor is `{index, name, bufferView, byteOffset, componentType, normalized, count, type, min, max, sparse}`, where `min` and `max` are lists of numbers or nil. A sparse accessor has `sparse = {count, indicesBufferView, indicesByteOffset, indicesComponentType, valuesBufferView, valuesByteOffset}`.

### document:readAccessor(accessor)

Reads an accessor table of the document as a `FloatBuffer` of [`haylen.collections`](collections.md), with every component of every element in order, matrices by columns, the values of a sparse part in place and normalized integers turned into floats. An accessor of more values than a float buffer takes raises `The accessor holds more values than a float buffer takes, so read it from C++.`.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
local positions = document:readAccessor(document.meshes[1].primitives[1].attributes.POSITION)
print(#positions / 3)
```

### document:readUnsigned(accessor)

Reads an accessor of unsigned integers that are not normalized, such as indices or joints, as a list of integers. Another accessor raises `The function that reads unsigned integers takes an accessor of unsigned integers that are not normalized.`.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
local indices = document:readUnsigned(document.meshes[1].primitives[1].indices)
print(#indices / 3)
```

### document:decodeImage(image, options)

Decodes an image table of the document into an `Image` of [`haylen.graphics`](graphics.md) on the frame thread. The option `maxSize`, from 1 to the largest side of an image, refuses an image longer on a side before a pixel is allocated, and the message of a failure names the image and the file, such as `image 0 of models/ship.glb`.

```lua
local gltf = require('haylen.gltf')

local document = gltf.read('models/ship.glb')
local image = document:decodeImage(document.images[1], {maxSize = 1024})
print(image.width, image.height)
```

### document:decodeImageAsync(image, options)

Decodes the same as `document:decodeImage` on the task pool and returns a promise of the `Image`.

```lua
local async = require('async')
local gltf = require('haylen.gltf')

async.spawn(function()
    local document = gltf.read('models/ship.glb')
    local image = document:decodeImageAsync(document.images[1]):await()
    print(image.width)
end)
```

## Enum names

| Field | Names |
| --- | --- |
| `componentType`, `indicesComponentType` | `'byte'`, `'unsignedByte'`, `'short'`, `'unsignedShort'`, `'unsignedInt'`, `'float'` |
| `type` of an accessor | `'scalar'`, `'vec2'`, `'vec3'`, `'vec4'`, `'mat2'`, `'mat3'`, `'mat4'` |
| `mode` | `'points'`, `'lines'`, `'lineLoop'`, `'lineStrip'`, `'triangles'`, `'triangleStrip'`, `'triangleFan'` |
| `magFilter`, `minFilter` | `'nearest'`, `'linear'`, `'nearestMipmapNearest'`, `'linearMipmapNearest'`, `'nearestMipmapLinear'`, `'linearMipmapLinear'` |
| `wrapS`, `wrapT` | `'clampToEdge'`, `'mirroredRepeat'`, `'repeat'` |
| `alphaMode` | `'opaque'`, `'mask'`, `'blend'` |
| `type` of a camera | `'perspective'`, `'orthographic'` |
| `path` | `'translation'`, `'rotation'`, `'scale'`, `'weights'` |
| `interpolation` | `'step'`, `'linear'`, `'cubicSpline'` |
| `target` | `'arrayBuffer'`, `'elementArrayBuffer'` |
| `storage` | `'glb'`, `'file'`, `'dataUri'`, `'bufferView'` |

## Errors

A file that breaks a rule of glTF 2.0 raises the sentence of the reader, which names the file, the object in words and the rule, and says what to do, such as `The glTF file "models/ship.glb" is not valid, since the key "indices" of primitive 1 of mesh 0 names accessor 9, which the file lacks. Export the model again from the tool that made it, so it follows glTF 2.0.`. The [rules](../3d/gltf.md#the-rules-it-checks) and the [limits](../3d/gltf.md#limits) of the guide list what it checks. These errors also happen:

- A file that the package lacks raises `The glTF file "<path>" is not in the content folder of the package. Check the path of the model, which is relative to the content folder.`.
- A file that is not on the device yet raises `The asset "<path>" is not on the device yet, since it downloads when the app asks for it. Load it with "loadAsync" or a preload group, which bring it first.`, which `gltf.readAsync` avoids.
- A file that requires an extension the reader lacks raises `The glTF file "<path>" requires the extension "<name>", which Haylen does not support. Export the model again without that extension.`.
- A table that is not an object of the list that a method takes, or that belongs to another document, raises `The argument 2 is not a table of the list "<list>" of this document.`.
