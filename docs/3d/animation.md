# 🏃 3D animation

This guide explains how Haylen animates 3D models and scenes with the module `haylen.animation3d`. It covers tracks, the keys of one property over time, the four ways values run between keys, clips that hold the tracks of one animation, the clips of glTF files, players that run a clip over time and report its events, and the edits that make new clips, skeletons with their poses, skins with their palettes and the weights of their vertices, skinning on the CPU, crowds of characters posed on the job system, and morph targets with the face expressions of face tracking. The [`haylen.animation3d` reference](../lua-api/animation3d.md) lists every function, option and error.

## Tracks

A track holds the keys of one property of one target. A key is a time in seconds and a value. The target names a node or a bone, such as `Hips`, and the property names what moves:

- `position`, `rotation` and `scale` move the transform of a node or a bone.
- `weights` moves the morph weights of a mesh.
- Any other path names a property of a component of the target, such as `light.intensity` or `material.baseColor`. The part of the engine that plays the track on the target reads the path.

Each track holds values of one type: a number, a `vec2`, a `vec3` or a `vec4`, a `quat` for rotations, a `color` in linear light, the `weights` of the morph targets of a mesh with one number per target, a `boolean` or a `string`. Booleans and strings change only by steps, so they suit things that switch, such as whether a node shows or which face a sprite of a character wears.

```lua
local animation3d = require('haylen.animation3d')
local m = require('haylen.math')

local turn = m.fromAxisAngle(m.up(), math.pi / 2)
local spin = animation3d.newTrack({target = 'Turret', property = 'rotation', type = 'quat', times = {0, 1}, values = {0, 0, 0, 1, turn.x, turn.y, turn.z, turn.w}})
local glow = animation3d.newTrack({target = 'Lamp', property = 'light.intensity', times = {0, 0.5, 1}, values = {1, 4, 1}})
local face = animation3d.newTrack({target = 'Hero', property = 'face', type = 'string', interpolation = 'step', times = {0, 0.8}, strings = {'calm', 'angry'}})
print(spin:sample(0.5), glow:sample(0.25), face:sample(1))
```

A track never changes once made, and every edit returns a new track. A time before the first key gives the first value and a time after the last key gives the last value. Two keys at one time make an instant jump: the later key shows from that time on.

## Interpolation

The interpolation of a track says how values run between keys:

- `step` holds the value of a key until the next key.
- `linear` runs in a straight line. A rotation runs along the shorter arc between its keys at a constant speed, so a turn never takes the long way around.
- `cubicSpline` is the cubic Hermite spline of glTF. Each key holds an in tangent, its value and an out tangent, in units per second, so a curve leaves and reaches each key at the speed its tangents give. A rotation becomes a unit quaternion again after each sample.
- `bezier` runs along a cubic Bezier curve through the handles of each key, as the curve editors of animation tools draw them. A handle is a time and a value as offsets from its key: the in handle comes at or before its key, and the out handle at or after it, each within the time to the key next to it, so the curve never goes back in time.

```lua
local animation3d = require('haylen.animation3d')

-- Each key gives four numbers per component: the time and value of its in handle, then of its out handle.
local ease = animation3d.newTrack({target = 'Door', property = 'angle', interpolation = 'bezier', times = {0, 2}, values = {0, 1.5}, handles = {0, 0, 0.6, 0, -0.6, 0, 0, 0}})
print(ease:sample(0.2), ease:sample(1), ease:sample(1.8))
```

Sampling is deterministic: one time always gives the same value, whatever was sampled before, and a time on a key gives the value of that key exactly. A sample finds its key from the key of the last sample in a step or two, so a clip that plays forward or backward costs no search.

## Clips

A clip holds the tracks of one animation, such as a walk or the opening of a door, and named events at moments of it, such as each footstep. Each property of a target has one track in a clip. A clip lasts until its last key or event, or for the duration it names.

A clip samples every track at once into one float buffer, the components of each track one after another in the order of the tracks. The method `clip:offset` gives the place of the first number of a track. So a clip of a hundred bones samples in one call, and the code that applies the values reads the buffer.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')

local open = animation3d.newClip({
    name = 'open',
    tracks = {
        animation3d.newTrack({target = 'Door', property = 'angle', times = {0, 1}, values = {0, 1.5}}),
        animation3d.newTrack({target = 'Door', property = 'position', type = 'vec3', times = {0, 1}, values = {0, 0, 0, 0, 0.1, 0}}),
    },
    events = {{time = 0.9, name = 'creak', data = {volume = 0.5}}},
})
local values = collections.newFloatBuffer(open.valueCount)
open:sample(0.5, values)
print(values[open:offset('Door', 'angle')])
```

## Clips of glTF files

The animations of a [glTF file](gltf.md) become clips with `animation3d.clipFromGltf` and `animation3d.clipsFromGltf`. Each channel of an animation becomes a track, with the times, values and interpolation of its sampler: translation becomes `position`, rotation `rotation`, scale `scale` and the morph weights `weights`.

A track names its node by the name of the node in the file, so a clip of one file moves the nodes of the same names in another file, such as a walk exported alone that plays on every character of the same rig. A node without a name, or whose name another node of the file shares, takes its glTF pointer instead, such as `/nodes/3`, which names it by its place in the file. The reader checks the data of every animation, and the clip checks again what it reads, so a document made in code cannot make a clip of hostile data either.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
for _, clip in ipairs(animation3d.clipsFromGltf(document)) do
    print(clip.name, clip.duration, #clip.tracks)
end
local wave = animation3d.clipFromGltf(document, document.animations[1])
```

## Players

A `ClipPlayer3D` runs one clip over time. Each `player:update(dt)` moves its time by `dt` times its speed, samples the clip into its values and reports the events it passed. Its loop mode says what happens at the ends:

- `'loop'` starts again from the other end, so a walk repeats without end.
- `'once'` stops at the end and reports `'complete'`, such as an attack.
- `'pingPong'` turns and plays the other way, such as a pendulum.

A negative speed plays backward, and the player wraps or turns at the start the same way. A paused player keeps its time and reports nothing. Setting `player.time` moves the player without passing the events between, and the next update reports the events at that time.

Events reach the listeners of `player:on('event', listener, {owner = owner})` as `{name, time, data}`, in the order the player passes them, on the frame thread inside the update. A listener ends with its owner, so nothing runs for a scene that is gone. An update far longer than the clip, such as after the app was in the background, reports the events of the last three passes it makes, as the 2D skeletons do.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')
local scene = require('haylen.scene')

local document = gltf.read('models/robot.glb')
local walk = animation3d.clipFromGltf(document, document.animations[1])
local player = animation3d.newClipPlayer(walk, {loop = 'loop', speed = 1.2})

scene.push({
    enter = function(self)
        player:on('event', function(event)
            print('Footstep of the foot ' .. event.data.foot .. '.')
        end, {owner = self})
    end,
    update = function(self, dt)
        player:update(dt)
        local hips = player:value('Hips', 'position')
    end,
})
```

## Editing clips

Clips are made and edited in Lua and in C++, and every edit returns a new clip:

- `clip:trimmed(start, end)` keeps the part between two times, moved to start at zero, with its events. The curves stay exact, since the trim adds keys at both ends that keep the shape of each curve.
- `clip:joined(other)` plays another clip after this one, joining the tracks of the same property of the same target.
- `clip:resampled(framesPerSecond)` gives every track that interpolates keys at a fixed rate, while tracks of steps keep their keys.
- `clip:optimized(tolerance)` drops the keys that a line between the kept keys brings back within the tolerance, in the units of each track and in radians for rotations.
- `clip:compressed(tolerance)` drops keys the same way and keeps values in 16-bit steps of the range of each track where that stays within the tolerance, which takes about half the memory.
- `clip:toAdditive(reference)` makes the clip relative to the pose of another clip, which a layer adds over another animation, such as a wave over a walk. Positions and other values become differences, scales ratios and rotations turns from the reference.
- `animation3d.clipFromMorphSequence(target, targetCount)` plays the morph targets of a mesh one after another, each one fully at its frame, such as the frames of a mouth that a tool baked as targets.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local all = animation3d.clipFromGltf(document, document.animations[1])
local intro = all:trimmed(0, 1.5):renamed('intro')
local light = all:resampled(30):compressed(0.001)
local wave = animation3d.clipFromGltf(document, document.animations[2]):toAdditive(all, {time = 0})
print(intro.duration, light.memorySize, all.memorySize, wave.name)
```

## Skeletons and poses

A skeleton is the hierarchy of the bones of a character. Each bone has a name, a parent and a rest pose, its position, rotation and scale relative to its parent when nothing moves it. A skeleton is shared data, so a hundred characters of one model share one skeleton, and each character keeps its own `Pose3D`, the local transform of each bone.

`animation3d.skeletonFromGltf` makes the skeleton of a skin of a glTF file. Its bones are the joints of the skin and every node above them, so a node between two joints, or the node of the armature above the hips, places the joints as the file does. Each bone takes the name of its node by the same rule as the tracks of clips, so a clip of the file, or of another file of the same rig, moves the bones of the names it targets.

A pose samples a clip with `pose:sample(clip, time)`, which writes the tracks of `position`, `rotation` and `scale` of each bone that the clip moves and leaves the other bones as they are. The other tracks of the clip, such as the morph weights of a face, belong to other parts of the engine. Then `pose:computeModelMatrices()` gives each bone its matrix in the space of the skeleton, its parent's matrix times its own transform, which attaches things to a bone, such as a sword to a hand.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/knight.glb')
local skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
local walk = animation3d.clipFromGltf(document, document.animations[1])
local pose = animation3d.newPose(skeleton)
pose:sample(walk, 0.4)
pose:computeModelMatrices()
local hand = pose:modelMatrix('RightHand'):apply({0, 0, 0})
print(skeleton.boneCount, hand)
```

## Skins and palettes

A skin names the joints that bend a mesh: bones of a skeleton, each with its inverse bind matrix, which brings a vertex of the mesh into the space of its bone at rest. The palette of a pose holds one matrix for each joint, the model matrix of its bone times its inverse bind matrix, so a joint at rest moves nothing and a joint that turns turns the vertices that follow it. A skin has no limit on its joints other than the bones of a skeleton.

`skin:palette(pose, buffer)` writes twelve floats for each joint, the first three rows of its matrix, which is the layout that a texture or a buffer of joints holds on the GPU. `skin:dualQuatPalette(pose, buffer)` writes the rotation and the translation of each joint as a dual quaternion, which blends without the loss of volume that blended matrices show at twisted joints, such as a forearm that turns at the wrist. Dual quaternions hold no scale, so their rotations come from the rotations of the pose, and a skin whose joints scale, such as a character that grows, blends matrices.

## Skin weights

Each vertex of a skinned mesh follows a few joints by weights that add up to one. A glTF file gives them in sets of four, `JOINTS_0` and `WEIGHTS_0`, then `JOINTS_1` and `WEIGHTS_1` for vertices of more joints. `animation3d.skinWeightsFromGltf` packs every set into the same number of influences for each vertex, 4 or 8, the way a vertex shader reads them: each vertex keeps its largest weights in the order of their size, the joint of the lower index first between equal weights, and they add up to one again. Four influences suit most characters and cost less, and eight keep the faces and hands of detailed characters. Joints that name no joint of the skin and weights that are negative or do not add up to one are refused with a sentence that names the vertex.

## Skinning on the CPU

`weights:skin(pose, skin, positions, skinnedPositions)` bends the positions of a mesh, three floats for each vertex in float buffers, and also its normals with the options `normals` and `skinnedNormals`. It blends the matrices of the joints of each vertex, or their dual quaternions with `dualQuaternions = true`, and spreads a large mesh over the job system. The GPU bends the meshes that draw, so skinning on the CPU serves what needs the bent shape itself, such as the bounds of a posed character, a ray cast against it or a check of the skinning of the GPU.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')
local gltf = require('haylen.gltf')

local document = gltf.read('models/knight.glb')
local skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
local skin = animation3d.skinFromGltf(document, document.skins[1], skeleton)
local weights = animation3d.skinWeightsFromGltf(document, document.meshes[1], skin, {influences = 4})
local positions = document:readAccessor(document.meshes[1].primitives[1].attributes.POSITION)
local pose = animation3d.newPose(skeleton)
pose:sample(animation3d.clipFromGltf(document, document.animations[1]), 0.4)
pose:computeModelMatrices()
local bent = collections.newFloatBuffer(#positions)
weights:skin(pose, skin, positions, bent, {dualQuaternions = true})
```

## Crowds

A crowd of characters of one model costs one call per frame with a `PoseBatch3D`. Each character of the batch loops a clip at its own time and speed, and `batch:update(dt)` moves every time, poses every character and writes all the palettes into one buffer, the characters one after another, so one texture of joints feeds an instanced draw of the whole crowd. The characters spread over the job system, and characters of one clip share the binding of the clip to the skeleton.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')
local gltf = require('haylen.gltf')
local scene = require('haylen.scene')

local document = gltf.read('models/villager.glb')
local skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
local skin = animation3d.skinFromGltf(document, document.skins[1], skeleton)
local walk = animation3d.clipFromGltf(document, document.animations[1])
local crowd = animation3d.newPoseBatch(skin)
for index = 1, 500 do
    crowd:add(walk, {time = math.random() * walk.duration, speed = 0.8 + math.random() * 0.4})
end
local palettes = collections.newFloatBuffer(crowd.count * crowd.paletteSize)

scene.push({
    update = function(self, dt)
        crowd:update(dt)
        crowd:readPalettes(palettes)
    end,
})
```

## Morph targets

A morph target is a shape of a mesh: the difference that it adds to the positions, normals and tangents of the vertices, such as a smile, a blink or a dent. The weights of a mesh blend its targets over its base shape, each target times its weight, and a track of `weights` animates them. A weight may lie below zero or above one, which reaches past a target, as glTF allows.

`animation3d.morphTargetsFromGltf` reads the targets of a primitive of a glTF mesh. Each target keeps only the vertices it moves, so a face of fifty targets that each move a few hundred of its vertices takes little memory. A target takes its name from the `targetNames` that the `extras` of the mesh list, which the tools that export glTF write, and `targets:find(name)` finds a target by it. `targets:apply(weights, positions, morphedPositions)` blends the targets on the CPU, with normals and tangents that become unit vectors again, for what needs the morphed shape itself, such as its bounds or a check of the morphing of the GPU.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')
local gltf = require('haylen.gltf')

local document = gltf.read('models/face.glb')
local primitive = document.meshes[1].primitives[1]
local targets = animation3d.morphTargetsFromGltf(document, document.meshes[1])
local positions = document:readAccessor(primitive.attributes.POSITION)
local weights = collections.newFloatBuffer(targets.targetCount)
weights[targets:find('smile')] = 0.8
local morphed = collections.newFloatBuffer(#positions)
targets:apply(weights, positions, morphed)
```

## Face expressions

Face tracking on phones and headsets reports the face of a person as the weights of 52 blend shapes with fixed names, such as `jawOpen`, `eyeBlinkLeft` and `mouthSmileRight`. Artists give the morph targets of faces the same names, so the weights that tracking reports drive the targets of a model of the same names without a table of their own. `animation3d.faceExpressions` lists the 52 names, and `targets:find` finds the target of each.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/face.glb')
local targets = animation3d.morphTargetsFromGltf(document, document.meshes[1])
local covered = 0
for _, expression in ipairs(animation3d.faceExpressions) do
    if targets:find(expression) then
        covered = covered + 1
    end
end
print(covered .. ' of ' .. #animation3d.faceExpressions .. ' expressions have a target.')
```

## Performance

Sampling, posing, palettes, skinning and morphing run in C++, and nothing allocates once their buffers exist. The command `haylen engine bench --suite animation3d` measures a clip of 64 bones, a pose with its palette, a crowd of 500 characters of 64 joints on one thread and on the job system, and 10 thousand vertices skinned with matrices and with dual quaternions.

## From C++

The C++ types live in `haylen/3d/animation/`, in the namespace `haylen::animation3d`: `Track3D`, `Clip3D`, `ClipPlayer3D`, `Skeleton3D`, `Pose3D`, `PoseSampler3D`, which binds a clip to a skeleton once for many characters, `Skin3D`, `SkinWeights3D`, `Skinning3D`, `PoseBatch3D`, `MorphTargets3D` and the enum `FaceExpression3D`. A clip is shared data, so players hold it as a `std::shared_ptr<const Clip3D>`, and its sample writes into a span of floats with a cursor for each track. Sampling allocates nothing, and an update of a player allocates nothing once its events of one update fit what earlier updates held.

```cpp
#include <memory>
#include <vector>

#include "haylen/3d/animation/ClipPlayer3D.hpp"
#include "haylen/3d/gltf/Reader.hpp"

const haylen::gltf::Document document = haylen::gltf::Reader::read(engine.getPackage(), "models/robot.glb");
auto walk = std::make_shared<const haylen::animation3d::Clip3D>(haylen::animation3d::Clip3D::fromGltf(document, 0));
haylen::animation3d::ClipPlayer3D player(walk, {.loop = haylen::animation3d::ClipPlayer3D::Loop::Loop, .speed = 1.2F});
player.eventFired.connect([](const haylen::animation3d::Clip3D::Event& event) { haylen::core::Log::info("{}", event.name); });
player.update(1.0F / 60.0F);
const std::span<const float> values = player.getValues();
```
