# 📖 `haylen.animation3d`

The module `haylen.animation3d` animates 3D models and scenes. It makes tracks, the keys of one property of a node, a bone or a component over time, gathers them into clips with their events, reads the clips of glTF files, edits clips into new ones and plays them with clip players. It also makes the skeletons, poses, skins and skin weights of characters, bends their vertices on the CPU, poses crowds in pose batches and blends the morph targets of meshes. Times are in seconds, positions in meters and angles in radians. The [3D animation guide](../3d/animation.md) explains how the parts fit together.

```lua
local animation3d = require('haylen.animation3d')
```

## Functions

### animation3d.newTrack(description)

Returns a [`Track3D`](#track3d) of the keys of the table `description`, whose unknown keys raise `Unknown option "<key>".`:

| Key | Default | Meaning |
| --- | --- | --- |
| `target` | `''` | The node or the bone that the track moves, by its name. |
| `property` | required | What moves: `'position'`, `'rotation'`, `'scale'` and `'weights'` for the transform and the morph weights, or the path of any other property, such as `'light.intensity'`. |
| `type` | `'number'` | The type of the values, one of the [enum names](#enum-names). |
| `interpolation` | `'linear'` | How values run between keys, one of the [enum names](#enum-names). Booleans and strings take `'step'`. |
| `times` | required | The time of each key, a list of numbers or a `FloatBuffer` of [`haylen.collections`](collections.md), each of zero seconds or more and none before the one before it. |
| `values` | none | The components of each key in order, a list of numbers or a `FloatBuffer`. A `'cubicSpline'` track holds the in tangent, the value and the out tangent of each key in a row, with tangents in units per second. A `'weights'` track holds as many weights for each key as its values give. A `'quat'` track holds unit quaternions `x, y, z, w` and a `'color'` track linear colors `r, g, b, a`. |
| `handles` | none | For a `'bezier'` track, four numbers for each component of each key: the time and the value of its in handle and of its out handle as offsets from the key. The in handle comes at or before its key and the out handle at or after it, each within the time to the key next to it. |
| `strings` | none | For a `'string'` track, the string of each key. |

```lua
local animation3d = require('haylen.animation3d')

local glow = animation3d.newTrack({target = 'Lamp', property = 'light.intensity', times = {0, 0.5, 1}, values = {1, 4, 1}})
local mood = animation3d.newTrack({target = 'Hero', property = 'mood', type = 'string', interpolation = 'step', times = {0, 2}, strings = {'calm', 'angry'}})
print(glow:sample(0.25), mood:sample(3)) -- 2.5 angry
```

### animation3d.newClip(description)

Returns a [`Clip3D`](#clip3d) of the table `description`, whose unknown keys raise `Unknown option "<key>".`:

| Key | Default | Meaning |
| --- | --- | --- |
| `name` | `''` | The name of the clip. |
| `tracks` | none | A list of [`Track3D`](#track3d) values, at most one for each property of a target. |
| `events` | none | A list of events, each `{time, name, data}`, where `data` is any value that JSON holds and that players hand to their listeners. |
| `duration` | the last key or event | How long the clip lasts, at least until its last key and event. |

```lua
local animation3d = require('haylen.animation3d')

local open = animation3d.newClip({
    name = 'open',
    tracks = {animation3d.newTrack({target = 'Door', property = 'angle', times = {0, 1}, values = {0, 1.5}})},
    events = {{time = 0.9, name = 'creak', data = {volume = 0.5}}},
})
print(open.duration, open.valueCount) -- 1.0 1
```

### animation3d.clipFromGltf(document, animation)

Returns the [`Clip3D`](#clip3d) of an animation table of a `Document` of [`haylen.gltf`](gltf.md). Each channel that targets a node becomes a track, with `position`, `rotation`, `scale` or `weights` and the interpolation of its sampler. A track names its node by its name when no other node of the file takes it, and otherwise by the glTF pointer of the node, such as `/nodes/3`. A table that is not an animation of the document raises `The argument 2 is not a table of the list "animations" of this document.`.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local wave = animation3d.clipFromGltf(document, document.animations[1])
print(wave.name, wave.tracks[1].target, wave.tracks[1].property)
```

### animation3d.clipsFromGltf(document)

Returns the list of the clips of every animation of a `Document`, in the order of the file.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

for _, clip in ipairs(animation3d.clipsFromGltf(gltf.read('models/robot.glb'))) do
    print(clip.name, clip.duration)
end
```

### animation3d.clipFromMorphSequence(target, targetCount, options)

Returns a [`Clip3D`](#clip3d) with one `weights` track of `target` that shows each of `targetCount` morph targets fully at its frame and fades into the next. The optional table `options` takes `name` (default `''`), `framesPerSecond` (default `30`) and `loop` (default `true`), which fades the last target back into the first, so a looping player wraps without a jump. Unknown keys raise `Unknown option "<key>".`.

```lua
local animation3d = require('haylen.animation3d')

local talk = animation3d.clipFromMorphSequence('Mouth', 8, {name = 'talk', framesPerSecond = 12})
print(talk.duration) -- 0.66666668653488
```

### animation3d.newClipPlayer(clip, options)

Returns a [`ClipPlayer3D`](#clipplayer3d) of a clip. The optional table `options` takes `loop` (default `'loop'`), `speed` (default `1`), `paused` (default `false`) and `time`, the time to start at, which is the start of the clip, or its end while the speed is negative, without one. Unknown keys raise `Unknown option "<key>".`.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local player = animation3d.newClipPlayer(animation3d.clipFromGltf(document, document.animations[1]), {loop = 'pingPong', speed = 0.5})
```

### animation3d.newSkeleton(bones)

Returns a [`Skeleton3D`](#skeleton3d) of a list of bones, each `{name, parent, position, rotation, scale}`, whose unknown keys raise `Unknown option "<key>".`. The `name` of each bone is its own, the `parent` is the place of a bone before it in the list, counted from 1, or nil for a root, and the rest pose takes `position` (default `{0, 0, 0}`), `rotation` (default no turn) and `scale` (default `{1, 1, 1}`). A skeleton holds from 1 to 65536 bones.

```lua
local animation3d = require('haylen.animation3d')

local arm = animation3d.newSkeleton({{name = 'Shoulder'}, {name = 'Elbow', parent = 1, position = {0.3, 0, 0}}, {name = 'Wrist', parent = 2, position = {0.25, 0, 0}}})
print(arm.boneCount) -- 3
```

### animation3d.skeletonFromGltf(document, skin)

Returns the [`Skeleton3D`](#skeleton3d) of a skin table of a `Document` of [`haylen.gltf`](gltf.md): the joints of the skin and every node above them, parents first, named after their nodes by the rule of [`animation3d.clipFromGltf`](#animation3dclipfromgltfdocument-animation), with the rest poses of the nodes.

### animation3d.newPose(skeleton)

Returns a [`Pose3D`](#pose3d) of a skeleton at its rest pose.

### animation3d.newSkin(skeleton, joints)

Returns a [`Skin3D`](#skin3d) of a skeleton with a list of joints, each `{bone, inverseBindMatrix}`, whose unknown keys raise `Unknown option "<key>".`. The `bone` is the place of a bone of the skeleton counted from 1, and the `inverseBindMatrix` a `Mat4`, the identity by default.

### animation3d.skinFromGltf(document, skin, skeleton)

Returns the [`Skin3D`](#skin3d) of a skin table of a `Document` for the skeleton that `animation3d.skeletonFromGltf` made of it, with an identity for each joint when the skin has no inverse bind matrices.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/knight.glb')
local skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
local skin = animation3d.skinFromGltf(document, document.skins[1], skeleton)
print(skeleton.boneCount, skin.jointCount)
```

### animation3d.skinWeightsFromGltf(document, mesh, skin, options)

Returns the [`SkinWeights3D`](#skinweights3d) of a primitive of a mesh table of a `Document`, which the skin bends. The optional table `options` takes `primitive`, the place of the primitive in the mesh counted from 1 (default `1`), and `influences`, `4` (the default) or `8`, and other keys raise `Unknown option "<key>".`. Each vertex keeps its largest weights, the joint of the lower index first between equal weights, and they add up to one again.

### animation3d.newPoseBatch(skin)

Returns an empty [`PoseBatch3D`](#posebatch3d) of the characters of a skin.

### animation3d.newMorphTargets(vertexCount, targets)

Returns [`MorphTargets3D`](#morphtargets3d) of a mesh of `vertexCount` vertices from a list of targets, each `{name, vertices, positions, normals, tangents}`, whose unknown keys raise `Unknown option "<key>".`. The `vertices` are the vertices that the target moves, counted from 1 and growing, and `positions`, `normals` and `tangents`, each optional, hold three numbers for each of them, in a list or a `FloatBuffer`: the difference that the target adds at a weight of one. Each named target needs a name of its own.

### animation3d.morphTargetsFromGltf(document, mesh, options)

Returns the [`MorphTargets3D`](#morphtargets3d) of a primitive of a mesh table of a `Document`, each keeping the vertices it moves, named by the `targetNames` of the `extras` of the mesh when they name every target. The optional table `options` takes `primitive`, counted from 1 (default `1`), and other keys raise `Unknown option "<key>".`.

### animation3d.faceExpressions

The list of the names of the 52 face blend shapes that face tracking reports, from `'eyeBlinkLeft'` to `'tongueOut'`, which [`targets:find`](#targetsfindname) finds the targets of.

## Track3D

A `Track3D` holds the keys of one property of one target. It never changes once made.

| Property | Type | Meaning |
| --- | --- | --- |
| `target` | string | The node or bone it moves. |
| `property` | string | The property it moves. |
| `type` | string | The type of its values. |
| `interpolation` | string | How values run between its keys. |
| `storage` | string | `'float'`, or `'quantized'` for values in 16-bit steps. |
| `keyCount` | integer | The number of keys. |
| `componentCount` | integer | The numbers that a sample writes, one for a string track. |
| `startTime` and `endTime` | number | The times of the first and the last key. |
| `quantizationError` | number | The most that quantizing moved a value, `0` for floats. |
| `memorySize` | integer | The bytes that its keys take. |

### track:sample(time)

Returns the value at a time: a number, a boolean, a string, a `Vec2`, a `Vec3`, a `Vec4`, a `Quat`, a `Color` or a list of weights. A time before the first key gives the first value and a time after the last key the last value. A cubic spline of rotations that passes through a quaternion of length zero raises `The "<property>" track of "<target>" passes through a quaternion of length zero at <time> seconds, so it holds no rotation there. Make the tangents of its keys smaller.`.

```lua
local animation3d = require('haylen.animation3d')

local track = animation3d.newTrack({target = 'Face', property = 'weights', type = 'weights', times = {0, 1}, values = {0, 1, 1, 0}})
local weights = track:sample(0.25)
print(weights[1], weights[2]) -- 0.25 0.75
```

### track:times(), track:values() and track:handles()

Return the times, the values and the handles of the keys as new `FloatBuffer` values, in the layout of `animation3d.newTrack`. Quantized values come back as the floats they stand for.

### track:strings()

Returns the list of the strings of a string track.

## Clip3D

A `Clip3D` holds the tracks and events of one animation. It never changes once made, and every edit returns a new clip.

| Property | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the clip. |
| `duration` | number | How long it lasts in seconds. |
| `valueCount` | integer | The numbers that a sample writes, the components of every track one after another. |
| `memorySize` | integer | The bytes that the keys of its tracks take. |
| `tracks` | list | Its [`Track3D`](#track3d) values, in order. |
| `events` | list | Its events, each `{time, name, data}`, in the order of their times. |

### clip:track(target, property)

Returns the track of a property of a target, or nil.

### clip:offset(target, property)

Returns the place of the first number of a track in what `clip:sample` writes, counted from one, or nil for a track that the clip lacks.

### clip:sample(time, buffer, first)

Writes the value of every track at a time into a `FloatBuffer`, from its value `first` on, `1` by default. A string track writes the index of its key from zero, and a boolean track `0` or `1`. A buffer without room raises `A sample of the clip "<name>" takes <count> values and <tracks> cursors, while <size> values and <tracks> cursors were given.`.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local clip = animation3d.clipFromGltf(document, document.animations[1])
local values = collections.newFloatBuffer(clip.valueCount)
clip:sample(0.5, values)
local hips = clip:offset('Hips', 'rotation')
print(values[hips], values[hips + 3])
```

### clip:renamed(name)

Returns the same clip with another name.

### clip:trimmed(start, end)

Returns the part from `start` to `end`, moved to start at zero, with its events. Ends outside the clip or out of order raise `A trim of the clip "<name>" runs within its <duration> seconds from a start to an end at or after it, while it was given <start> and <end>.`.

### clip:joined(other)

Returns this clip followed by `other`. Tracks of the same property of the same target join, and a track of one clip alone holds its value through the other. Two such tracks of different types, interpolations or counts of components raise `The "<property>" track of "<target>" joins only a track of the same target, property, type, interpolation and number of components, while the "<property>" track of "<target>" differs.`.

### clip:resampled(framesPerSecond)

Returns the clip with keys at a fixed rate for every track that interpolates, the last key at the end exactly. Tracks of steps keep their keys. A rate of zero or less raises `A resample takes a positive number of frames per second, while it was given <rate>.`, and a rate that would make more than 16777216 keys in a track raises `A resample of the "<property>" track of "<target>" at <rate> frames per second would make more than 16777216 keys.`.

### clip:optimized(tolerance)

Returns the clip with the fewest keys of each step or linear track that bring back every key within `tolerance`, in the units of the track and in radians for rotations. A cubic spline or Bezier track keeps its keys unless every key holds the same value with flat tangents or handles, which leaves one key. A negative tolerance raises `An optimization takes a tolerance of zero or more, while it was given <tolerance>.`.

### clip:compressed(tolerance)

Returns the clip optimized within half the tolerance, with the values of each track that interpolates numbers by steps, lines or cubic splines in 16-bit steps of its range where they stay within the other half. Tracks of booleans, strings and Bezier curves, and tracks whose range is too large for steps that fine, keep their floats.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local clip = animation3d.clipFromGltf(document, document.animations[1]):resampled(30)
local small = clip:compressed(0.001)
print(clip.memorySize, small.memorySize, small.tracks[1].storage)
```

### clip:toAdditive(reference, options)

Returns the clip relative to the pose of `reference` at the time `time` of the optional table `options`, `0` by default, whose other keys raise `Unknown option "<key>".`. Positions and other values become their difference to the reference, scales their ratio and rotations the turn from the reference, while tracks of booleans and strings stay as they are. A track that the reference lacks raises `The reference clip "<name>" has no "<property>" track of "<target>", which the additive clip of "<name>" needs.`, and a reference scale of zero raises `The "scale" track of "<target>" cannot become relative to a reference scale of zero.`.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')

local document = gltf.read('models/robot.glb')
local idle = animation3d.clipFromGltf(document, document.animations[1])
local wave = animation3d.clipFromGltf(document, document.animations[2]):toAdditive(idle)
```

## ClipPlayer3D

A `ClipPlayer3D` plays one clip. Each update moves its time by the seconds times its speed, samples the clip and then reports the events it passed in the order it passed them.

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `clip` | Clip3D | read and write | The clip it plays. A new clip plays from its start, or from its end while the speed is negative. |
| `time` | number | read and write | The time in the clip. Setting it moves there without passing the events between, and the next update reports the events at that time. A time outside the clip raises `The time <time> lies outside the clip "<name>", which lasts <duration> seconds.`. |
| `speed` | number | read and write | How fast it plays, negative to play backward. A speed that is not finite raises `The speed of a clip player is a finite number, while it was given <speed>.`. |
| `paused` | boolean | read and write | Whether updates leave it where it stands. |
| `loop` | string | read and write | `'loop'`, `'once'` or `'pingPong'`. |
| `finished` | boolean | read | Whether a player of `'once'` reached the end it plays toward. |

### player:update(dt)

Moves the player by `dt` seconds times its speed, samples its clip and reports what it passed. A player of `'loop'` starts again from the other end, a player of `'once'` stops at the end and reports `'complete'`, and a player of `'pingPong'` turns. An update far longer than the clip reports the events of the last three passes it makes. A negative or infinite step raises `A clip player updates by a finite time step of zero or more seconds, while it was given <dt>.`, and a listener that updates the player it listens to raises `A clip player cannot update from a listener of its own events.`. An error of a listener reaches the caller of `update`.

### player:value(target, property)

Returns the value of the track of a property of a target as of the last update, in the type that `track:sample` returns, or nil for a track that the clip lacks.

### player:readValues(buffer, first)

Writes the values of every track as of the last update into a `FloatBuffer`, laid out as `clip:sample` writes them, from its value `first` on, `1` by default.

### player:on(event, listener, options)

Listens to `'event'`, which calls the listener with the table `{name, time, data}` of every event the player passes, or to `'complete'`, which calls it once a player of `'once'` reaches its end, and returns a `Connection`. The optional table `options` takes `owner`, and without one the listener takes the default owner. Another event raises `Unknown clip player event "<event>". Clip players report "event" and "complete".`.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')
local scene = require('haylen.scene')

local document = gltf.read('models/robot.glb')
local player = animation3d.newClipPlayer(animation3d.clipFromGltf(document, document.animations[1]), {loop = 'once'})

scene.push({
    enter = function(self)
        player:on('event', function(event) print(event.name, event.time) end, {owner = self})
        player:on('complete', function() print('The clip ended.') end, {owner = self})
    end,
    update = function(self, dt)
        player:update(dt)
    end,
})
```

## Skeleton3D

A `Skeleton3D` holds the bones of a character. It never changes once made, and many poses and skins share it.

| Property | Type | Meaning |
| --- | --- | --- |
| `boneCount` | integer | The number of bones. |
| `bones` | list | The bones, each `{name, parent, position, rotation, scale}`, with `parent` counted from 1 and nil for a root. |

### skeleton:findBone(name)

Returns the place of the bone of a name, counted from 1, or nil.

## Pose3D

A `Pose3D` holds the local transform of each bone of one character, which clips write, and the matrix of each bone in the space of the skeleton, which skins read. Its methods name a bone by its name or by its place counted from 1, and a bone the skeleton lacks raises `The skeleton has no bone "<name>".` or `The skeleton has <count> bones, counted from 1, so it has no bone <place>.`.

| Property | Type | Meaning |
| --- | --- | --- |
| `skeleton` | Skeleton3D | The skeleton it poses. |

### pose:sample(clip, time)

Writes the `position`, `rotation` and `scale` of each bone that the clip moves at a time, and leaves the other bones as they are. A track of a bone of another type raises `The "<property>" track of "<bone>" of the clip "<name>" holds <type> values, while the <property> of a bone takes <type> values.`.

### pose:setRest()

Puts every bone back at its rest pose.

### pose:transform(bone) and pose:setTransform(bone, transform)

Read and write the local transform of a bone as a `Transform3D` of [`haylen.math`](math.md).

### pose:computeModelMatrices()

Computes the matrix of every bone in the space of the skeleton from the local transforms.

### pose:modelMatrix(bone)

Returns the `Mat4` of a bone in the space of the skeleton as of the last `pose:computeModelMatrices`.

```lua
local animation3d = require('haylen.animation3d')
local gltf = require('haylen.gltf')
local m = require('haylen.math')

local document = gltf.read('models/knight.glb')
local pose = animation3d.newPose(animation3d.skeletonFromGltf(document, document.skins[1]))
pose:sample(animation3d.clipFromGltf(document, document.animations[1]), 0.4)
pose:setTransform('Head', m.transform3d({0, 0.2, 0}, m.fromAxisAngle(m.up(), 0.5)))
pose:computeModelMatrices()
print(pose:modelMatrix('Head'):apply({0, 0, 0}))
```

## Skin3D

A `Skin3D` holds the joints that bend a mesh, bones of a skeleton with their inverse bind matrices.

| Property | Type | Meaning |
| --- | --- | --- |
| `jointCount` | integer | The number of joints. |
| `joints` | list | The joints, each `{bone, inverseBindMatrix}`, with `bone` counted from 1. |
| `skeleton` | Skeleton3D | The skeleton whose bones bend it. |

### skin:palette(pose, buffer, first)

Writes the palette of a pose of the skeleton of the skin into a `FloatBuffer`, from its value `first` on, `1` by default: twelve floats for each joint, the first three rows of the model matrix of its bone times its inverse bind matrix, as of the last `pose:computeModelMatrices`. A pose of another skeleton raises `The pose belongs to another skeleton than the skin.`, and a buffer without room raises `The palette of <count> joints takes <values> values, while the buffer holds <size> from its first one.`.

### skin:dualQuatPalette(pose, buffer, first)

Writes the palette of a pose as dual quaternions, eight floats for each joint: the real part and then the dual part, each `x, y, z, w`. The rotation of each joint comes from the rotations of the pose, since dual quaternions hold no scale. A joint whose inverse bind matrix scales an axis to zero raises `The inverse bind matrix of joint <index> of the skin scales an axis to zero, so it holds no rotation for dual quaternion skinning. Skin it with matrices.`.

## SkinWeights3D

`SkinWeights3D` hold the joints and weights of the vertices of a primitive, the same number of influences for each vertex.

| Property | Type | Meaning |
| --- | --- | --- |
| `influenceCount` | integer | The influences of each vertex, 4 or 8. |
| `vertexCount` | integer | The number of vertices. |
| `jointCount` | integer | The joints of the skin that the joints of the vertices name. |

### weights:readJoints(buffer, first) and weights:readWeights(buffer, first)

Write the joints of every vertex, counted from zero as a shader reads them, or their weights into a `FloatBuffer`, the influences of each vertex one after another.

### weights:skin(pose, skin, positions, skinnedPositions, options)

Bends the positions of the vertices, three floats for each vertex in the float buffer `positions`, into the float buffer `skinnedPositions`, by the palette of a pose as of its last `pose:computeModelMatrices`. The optional table `options` takes `normals` and `skinnedNormals`, float buffers of the normals and of the bent normals, and `dualQuaternions`, which blends dual quaternions instead of matrices (default `false`), and other keys raise `Unknown option "<key>".`. A normal that a joint scaled to zero folds stays zero. A large mesh spreads over the job system.

## PoseBatch3D

A `PoseBatch3D` poses many characters of one skin, each looping a clip at its own time and speed. Its characters are numbered from 1, and removing a character moves the characters after it one place down.

| Property | Type | Meaning |
| --- | --- | --- |
| `count` | integer | The number of characters. |
| `paletteSize` | integer | The floats of the palette of one character, twelve for each joint. |

### batch:add(clip, options)

Adds a character that loops a clip and returns its place. The optional table `options` takes `time` (default `0`) and `speed` (default `1`, negative to play backward), and other keys raise `Unknown option "<key>".`.

### batch:remove(character), batch:setClip(character, clip), batch:setTime(character, time), batch:setSpeed(character, speed) and batch:time(character)

Remove a character, give it another clip, set its time or its speed, and read its time. A place past the characters raises `The pose batch holds <count> characters, so it has no character <index>.`, with the index counted from zero.

### batch:setTimes(buffer, first)

Sets the time of every character, one number each from the value `first` of a `FloatBuffer` on, `1` by default.

### batch:update(dt)

Moves the time of every character by `dt` seconds times its speed, wrapped into its clip, poses it and writes its palette, spreading the characters over the job system.

### batch:readPalettes(buffer, first)

Writes the palettes of every character as of the last update into a `FloatBuffer`, the joints of each character one after another, twelve floats each.

```lua
local animation3d = require('haylen.animation3d')
local collections = require('haylen.collections')
local gltf = require('haylen.gltf')

local document = gltf.read('models/villager.glb')
local skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
local crowd = animation3d.newPoseBatch(animation3d.skinFromGltf(document, document.skins[1], skeleton))
local walk = animation3d.clipFromGltf(document, document.animations[1])
for index = 1, 100 do
    crowd:add(walk, {time = index * 0.1})
end
crowd:update(1 / 60)
local palettes = collections.newFloatBuffer(crowd.count * crowd.paletteSize)
crowd:readPalettes(palettes)
```

## MorphTargets3D

`MorphTargets3D` hold the morph targets of a mesh, each with the vertices it moves.

| Property | Type | Meaning |
| --- | --- | --- |
| `targetCount` | integer | The number of targets. |
| `vertexCount` | integer | The vertices of the mesh. |
| `names` | list | The name of each target, an empty string for a target without one. |
| `memorySize` | integer | The bytes that the vertices and differences of the targets take. |

### targets:find(name)

Returns the place of the target of a name, counted from 1, or nil, such as the target of a face expression by its name.

### targets:apply(weights, positions, morphedPositions, options)

Writes the base positions of a `FloatBuffer`, three floats for each vertex, plus each target times its weight, into `morphedPositions`. The `weights` are one number for each target in a list or a `FloatBuffer`, and may lie below zero or above one. The optional table `options` takes `normals` with `morphedNormals` and `tangents` with `morphedTangents`, float buffers of three floats for each vertex, which become unit vectors again, and other keys raise `Unknown option "<key>".`. Weights of another count or that are not finite raise `The morph targets take <count> finite weights, one for each target, while <given> weights were given.`.

## Enum names

| Field | Names |
| --- | --- |
| `type` of a track | `'number'`, `'vec2'`, `'vec3'`, `'vec4'`, `'quat'`, `'color'`, `'weights'`, `'boolean'`, `'string'` |
| `interpolation` | `'step'`, `'linear'`, `'cubicSpline'`, `'bezier'` |
| `storage` | `'float'`, `'quantized'` |
| `loop` | `'loop'`, `'once'`, `'pingPong'` |

## Errors

Keys that break the rules of a track raise a sentence that names the track and the rule, such as `The "rotation" track of "Hips" holds a quaternion of length 2 at key 0, while rotations are unit quaternions.`. These errors also happen:

| Message | Cause |
| --- | --- |
| `Unknown option "<key>".` | A table holds a key that its function does not take. |
| `A track needs the property it animates, such as "position".` | A track has no `property`. |
| `The "<property>" track of "<target>" has no keys, while a track needs at least one key.` | A track has no times. |
| `The "<property>" track of "<target>" has key <key> at <time> seconds, before the key before it, while the times of a track never go back.` | The times of a track go back. |
| `The "<property>" track of "<target>" holds <count> values, while its <keys> keys of <components> components need <needed>.` | The values do not fill the keys. |
| `The "<property>" track of "<target>" holds <type> values, which change only by steps, so its interpolation is "step".` | A boolean or string track interpolates. |
| `The "<property>" track of "<target>" has a handle of key <key> that reaches past the key next to it, so the curve would go back in time.` | A Bezier handle lies past the next key. |
| `The <list> are a list of numbers or a float buffer.` | `times`, `values` or `handles` is another value. |
| `The item <place> of the option "tracks" is not a track.` | The `tracks` of a clip hold another value. |
| `The clip "<name>" holds the "<property>" track of "<target>" twice, while a clip animates each property of a target once.` | Two tracks of a clip move the same property. |
| `The clip "<name>" lasts <duration> seconds, while its keys and events run until <end> seconds.` | The `duration` of a clip is shorter than its keys or events. |
| `The clip "<name>" has an event without a name at <time> seconds. Give every event a name.` | An event has no name. |
| `The animation <index> of the glTF file "<path>" does not make a clip, since <problem>. Export the model again from the tool that made it, so it follows glTF 2.0.` | A document made in code holds an animation that breaks a rule of glTF. |
| `Every bone of a skeleton needs a name of its own, while the name "<name>" of bone <index> is empty or taken.` | Two bones share a name, or a bone has none. |
| `The bone "<name>" names the parent <index>, while the parent of a bone comes before it.` | A bone names a parent that comes after it. |
| `Joint <index> of a skin names bone <bone> of a skeleton of <count> bones.` | A joint of a skin names a bone the skeleton lacks. |
| `Vertex <index> of the skin weights names joint <joint> of a skin with <count> joints.` | A vertex names a joint the skin lacks. |
| `The weights of vertex <index> of the skin weights add up to <sum> and not to 1.` | The weights of a vertex do not add up to one. |
| `A vertex takes 4 or 8 influences, while <count> were asked for.` | `skinWeightsFromGltf` got other influences. |
| `The morph target "<name>" names vertex <vertex> at place <place>, while the vertices of a target grow and stay below the <count> vertices of the mesh.` | The vertices of a target are out of order or past the mesh. |
| `The morph target "<name>" holds <count> differences of an attribute for its <vertices> vertices.` | The differences of a target do not match its vertices. |
| `The morph target "<name>" has the name of another target, while each target needs a name of its own.` | Two targets share a name. |
| `The options "normals" and "morphedNormals" come together, and so do "tangents" and "morphedTangents".` | `targets:apply` got one of a pair. |
