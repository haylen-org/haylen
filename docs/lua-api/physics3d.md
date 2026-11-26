# 📖 `haylen.physics3d`

The module `haylen.physics3d` makes 3D physics worlds in meters, which Jolt Physics simulates unless a plugin selects another backend. This page lists the functions of the module and every member of its worlds. The [3D physics guide](../3d/physics.md) explains worlds, steps, threads, limits and backends.

```lua
local physics3d = require('haylen.physics3d')
```

## Units

Every distance and position is in meters, every speed in meters per second, every time in seconds and gravity in meters per second squared, with `y` up, as the [units of 3D](../3d/coordinates.md#units) list. The memory of a step is in megabytes.

## Functions

| Function | Returns |
| --- | --- |
| [`physics3d.newWorld(options)`](#physics3dnewworldoptions) | A [`PhysicsWorld3D`](#physicsworld3d) |
| [`physics3d.backends()`](#physics3dbackends) | The names of the registered backends |

### physics3d.newWorld(options)

Creates an empty world and returns it. Its settings start from the `physics3d` section of `haylen.json`, and the table `options`, which is optional, changes any of them for this world. An unknown key raises `Unknown option "<key>".`, and a value out of its range raises a sentence that names it, such as `The value of "velocitySteps" must be from 2 to 64.`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec3 or `{x, y, z}` | `{0, -m.standardGravity, 0}` | Gravity in meters per second squared, which pulls every body that gravity moves. |
| `collisionSteps` | integer | `1` | The equal parts of each step, from 1 to 64. Each part finds the collisions and solves them again, which a world of fast bodies needs at a low step rate. |
| `velocitySteps` | integer | `10` | The passes of the velocity solver in each part, from 2 to 64. More passes stack bodies and hold joints more firmly. |
| `positionSteps` | integer | `2` | The passes that push overlapping bodies apart in each part, from 0 to 64. |
| `sleepEnabled` | boolean | `true` | Whether bodies that rest fall asleep, which costs nothing until something wakes them. |
| `timeBeforeSleep` | number | `0.5` | The seconds a body rests before it sleeps. |
| `sleepVelocity` | number | `0.03` | The speed in meters per second under which every point of a body counts as resting. |
| `speculativeContactDistance` | number | `0.02` | The distance in meters at which a contact starts before the bodies touch, which stops fast bodies before they overlap. |
| `penetrationSlop` | number | `0.02` | The overlap in meters that the solver leaves, which keeps resting bodies from jittering. |
| `restitutionThreshold` | number | `1` | The speed in meters per second under which a hit does not bounce. |
| `maxBodies` | integer | `10240` | The most bodies the world holds, from 1 to 8388607. |
| `maxBodyPairs` | integer | `65536` | The most pairs of bodies whose bounds touch in one step, from 1 to 8388607. |
| `maxContactConstraints` | integer | `20480` | The most contacts the world solves in one step, from 1 to 8388607. |
| `threads` | integer | The workers of the engine, up to 4 | The threads that a step runs on, from 1 to 64, while enough bodies are awake. |
| `temporaryMemory` | integer | `16` | The megabytes that one step works in, from 1 to 1024. |
| `owner` | table or userdata | The default owner | What the world belongs to. The world ends with its owner. |

```lua
local physics3d = require('haylen.physics3d')

local moon = physics3d.newWorld({gravity = {0, -1.62, 0}, velocitySteps = 12, owner = self})
```

### physics3d.backends()

Returns the names of the registered backends in alphabetical order, such as `{'jolt'}`. The built-in backend is `jolt`, and a C++ plugin may register and select another one, as the [guide](../3d/physics.md#backends) explains.

```lua
print(table.concat(require('haylen.physics3d').backends(), ', ')) -- jolt
```

## PhysicsWorld3D

A `PhysicsWorld3D`, named `haylen.PhysicsWorld3D` in messages, holds the bodies of a 3D simulation and moves them when the app steps it. Every member of a destroyed world, but `destroyed` and `destroy`, raises `The 3D physics world was destroyed.`.

### world:step(seconds)

Advances the world by the time in seconds, which is zero or more. Step a world in the `fixedUpdate` of a scene with the time it receives, so the simulation keeps the same step whatever the frame rate. A negative time, or one that is not a number, raises `A step of a 3D physics world takes a time of zero seconds or more.`.

```lua
function scene:fixedUpdate(seconds)
    self.world:step(seconds)
end
```

### world:destroy()

Ends the world at once with everything it holds, without waiting for the garbage collector. Destroying a destroyed world does nothing.

### world:stats()

Returns a table that tells what the world holds and what its last step cost.

| Field | Type | Meaning |
| --- | --- | --- |
| `bodies` | integer | The bodies of the world. |
| `activeBodies` | integer | The bodies that are awake. |
| `stepThreads` | integer | The threads that the last step ran on, or 0 before the first step. |
| `stepMilliseconds` | number | The milliseconds that the last step took. |
| `temporaryMemoryPeak` | integer | The most bytes of the temporary memory that any step used. |

### world:stateHash()

Returns an integer that hashes the whole state that the steps advance, bit for bit. Two worlds that receive the same bodies and the same steps on the same platform and build give the same hash, which checks that a replay or a lockstep game stays in step.

```lua
local hash = world:stateHash()
```

### World properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `gravity` | Vec3 | read and write | The option `gravity`. |
| `collisionSteps` | integer | read and write | The option `collisionSteps`. |
| `velocitySteps` | integer | read and write | The option `velocitySteps`. |
| `positionSteps` | integer | read and write | The option `positionSteps`. |
| `sleepEnabled` | boolean | read and write | The option `sleepEnabled`. |
| `timeBeforeSleep` | number | read and write | The option `timeBeforeSleep`. |
| `sleepVelocity` | number | read and write | The option `sleepVelocity`. |
| `speculativeContactDistance` | number | read and write | The option `speculativeContactDistance`. |
| `penetrationSlop` | number | read and write | The option `penetrationSlop`. |
| `restitutionThreshold` | number | read and write | The option `restitutionThreshold`. |
| `maxBodies` | integer | read | The option `maxBodies`, which stays for the life of the world. |
| `maxBodyPairs` | integer | read | The option `maxBodyPairs`, which stays for the life of the world. |
| `maxContactConstraints` | integer | read | The option `maxContactConstraints`, which stays for the life of the world. |
| `threads` | integer | read | The option `threads`, which stays for the life of the world. |
| `temporaryMemory` | integer | read | The option `temporaryMemory`, which stays for the life of the world. |
| `backend` | string | read | The name of the backend that simulates the world. |
| `bodyCount` | integer | read | The bodies of the world. |
| `activeBodyCount` | integer | read | The bodies that are awake. |
| `destroyed` | boolean | read | Whether the world was destroyed. |

A value written out of its range raises the same sentence as the option, and the world keeps the value it had.

```lua
world.gravity = {0, -3.71, 0}
world.velocitySteps = 14
```
