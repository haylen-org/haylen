# haylen.ai

`haylen.ai` provides finite state machines, behavior trees with a blackboard, utility AI with response curves and influence maps. A machine is in one named state at a time, and each state has optional functions that run when the state is entered, updated and left. Use state machines for enemy behavior, player states such as idle, running and jumping, and flows such as a boss fight with phases. Use behavior trees for layered decisions that react each tick, utility selectors for agents that weigh several needs, and influence maps to read the threat or pull of every spot of a map.

```lua
local ai = require('haylen.ai')
```

## Functions

### ai.newStateMachine(states)

Creates a state machine and returns it as a `StateMachine` object. `states` maps state names to state tables. The set of state names is fixed when the machine is created. The functions inside a state table are looked up every time they run, so they can be replaced later. The machine starts without a current state until the first `machine:change`.

Every function is optional.

| Field | Called | Arguments |
| --- | --- | --- |
| `enter` | When the machine changes to this state. | The machine, then the extra arguments given to `machine:change`. |
| `update` | On every `machine:update` while this state is current. | The machine and the delta time given to `machine:update`. |
| `exit` | When the machine changes from this state to another one or to itself. | The machine. |

`states` must be a table. A key that is not a string raises `bad argument #1 to 'newStateMachine' (state names must be strings)`, and a value that is not a table raises `bad argument #1 to 'newStateMachine' (each state must be a table)`.

```lua
local ai = require('haylen.ai')

local goblin = {x = 0, target = nil}

goblin.brain = ai.newStateMachine({
    idle = {
        update = function(machine, dt)
            if machine.elapsed > 2 then
                machine:change('patrol', 200)
            end
        end,
    },
    patrol = {
        enter = function(machine, distance)
            goblin.patrolEnd = goblin.x + distance
        end,
        update = function(machine, dt)
            goblin.x = goblin.x + 60 * dt
            if goblin.x >= goblin.patrolEnd then
                machine:change('idle')
            end
        end,
    },
})

goblin.brain:change('idle')

require('haylen.scene').push({
    update = function(self, dt)
        goblin.brain:update(dt)
    end,
})
```

## StateMachine

### machine:change(name, ...)

Changes to the state `name`. The current state's `exit` runs first, then `machine.onChange`, then the new state's `enter` with the extra arguments. `machine.elapsed` restarts at zero. Changing to the current state leaves it and enters it again.

A change requested from inside `enter`, `exit` or `onChange` waits until the running change finishes and then runs with its own arguments, in request order. An error raised by a state function reaches the caller of `change`, and changes queued during that transition are dropped. An unknown name raises `The state machine has no state named <name>.`

```lua
local ai = require('haylen.ai')

local player = {hp = 3}

local machine = ai.newStateMachine({
    alive = {},
    hurt = {
        enter = function(machine, damage)
            player.hp = player.hp - damage
            if player.hp <= 0 then
                machine:change('dead')
            end
        end,
    },
    dead = {
        enter = function(machine)
            print('game over')
        end,
    },
})

machine:change('alive')
machine:change('hurt', 2)
machine:change('hurt', 2)
print(machine.current) -- dead
```

### machine:update(dt)

Adds `dt` to `machine.elapsed` and calls the current state's `update` function. It does nothing before the first change. Machines are not updated by the engine, so call this from a scene `update` or `fixedUpdate` with the time step you want the machine to use.

```lua
local ai = require('haylen.ai')

local lamp = ai.newStateMachine({
    on = {
        update = function(machine, dt)
            if machine.elapsed >= 0.5 then machine:change('off') end
        end,
    },
    off = {
        update = function(machine, dt)
            if machine.elapsed >= 0.5 then machine:change('on') end
        end,
    },
})
lamp:change('on')

require('haylen.scene').push({
    update = function(self, dt)
        lamp:update(dt)
    end,
})
```

### machine:has(name)

Returns `true` when the machine has a state called `name`.

```lua
local ai = require('haylen.ai')

local machine = ai.newStateMachine({idle = {}, attack = {}})

local function command(name)
    if machine:has(name) then
        machine:change(name)
    end
end

command('attack')
command('fly')
```

### machine.current

Read-only name of the current state, or `nil` before the first change.

```lua
local ai = require('haylen.ai')

local door = ai.newStateMachine({open = {}, closed = {}})
door:change('closed')
if door.current == 'closed' then
    print('the door is closed')
end
```

### machine.previous

Read-only name of the state before the current one, or `nil` until the second change.

```lua
local ai = require('haylen.ai')

local menu = ai.newStateMachine({main = {}, options = {}, credits = {}})
menu:change('main')
menu:change('options')

local function back()
    if menu.previous then
        menu:change(menu.previous)
    end
end

back()
print(menu.current) -- main
```

### machine.elapsed

Read-only number of seconds spent in the current state, counted from the `dt` values given to `machine:update`.

```lua
local ai = require('haylen.ai')

local turret = ai.newStateMachine({
    charging = {
        update = function(machine, dt)
            if machine.elapsed >= 1.5 then
                machine:change('firing')
            end
        end,
    },
    firing = {},
})
turret:change('charging')
turret:update(1)
turret:update(1)
print(turret.current) -- firing
```

### machine.onChange

Readable and writable function called on every change as `onChange(machine, from, to)`, after the old state's `exit` and before the new state's `enter`. `from` is `nil` on the first change. Assign `nil` to remove it. Assigning a value that is neither a function nor `nil` raises an error.

```lua
local ai = require('haylen.ai')

local hero = ai.newStateMachine({idle = {}, run = {}, jump = {}})

hero.onChange = function(machine, from, to)
    print(string.format('hero: %s -> %s', tostring(from), to))
end

hero:change('idle')
hero:change('run')
hero.onChange = nil
```

## Behavior trees

A behavior tree is built from a description of nodes and ticked once per update. Every tick returns `'success'`, `'failure'` or `'running'`. Sequences and selectors remember their running child and go on from it on the next tick, and the root starts over after it finishes. Leaves are Lua functions of the blackboard, a table the leaves share.

### Node descriptions

The node functions return plain description tables that `ai.newBehaviorTree` reads.

| Function | Meaning |
| --- | --- |
| `ai.sequence(children)` | Runs the children in order and fails as soon as one fails. |
| `ai.selector(children)` | Runs the children in order and succeeds as soon as one succeeds. |
| `ai.parallel(children, successes)` | Ticks every unfinished child on each tick, succeeds once `successes` children succeeded and fails once too many failed to reach that. |
| `ai.inverter(child)` | Turns success into failure and failure into success. |
| `ai.succeeder(child)`, `ai.failer(child)` | Turn a finished child into a success or a failure. |
| `ai.repeater(child, count)` | Runs the child again after each success, once per tick, and succeeds after `count` successes, or never without a count. A failure of the child fails it. |
| `ai.retry(child, count)` | Runs the child again after each failure, once per tick, and fails after `count` failures, or never without a count. A success of the child succeeds it. |
| `ai.cooldown(child, seconds)` | Fails without running the child until `seconds` of tree time passed since the child last finished. |
| `ai.timeout(child, seconds)` | Fails and resets the child when it keeps running for more than `seconds`. |
| `ai.wait(seconds)` | Runs for `seconds` and then succeeds. |
| `ai.condition(fn)` | Calls `fn(blackboard)` and succeeds when it returns a true value. |
| `ai.action(fn)` | Calls `fn(blackboard, dt)`, which returns `'success'`, `'failure'` or `'running'`, or `true` for success and `false` for failure. Returning nothing counts as success, and another status name raises `A behavior tree action returned the unknown status '<name>'.` |

```lua
local ai = require('haylen.ai')

local brain = ai.selector({
    ai.sequence({
        ai.condition(function(board) return board.target ~= nil end),
        ai.cooldown(ai.action(function(board) board.shots = board.shots + 1 end), 0.5),
    }),
    ai.action(function(board, dt)
        board.angle = board.angle + dt
        return 'running'
    end),
})
print(brain.kind) -- selector
```

### ai.newBehaviorTree(root, blackboard)

Creates a `BehaviorTree` from a node description and an optional blackboard table, which defaults to a new table. A malformed description raises an error such as `Unknown behavior tree node '<kind>'.` or `A parallel node needs between one success and as many successes as it has children.` The same description may appear in several places of a tree, but a description that contains itself raises `A behavior tree description contains itself.`

```lua
local ai = require('haylen.ai')

local guard = ai.newBehaviorTree(ai.selector({
    ai.sequence({
        ai.condition(function(board) return board.enemy ~= nil end),
        ai.action(function(board, dt)
            board.x = board.x + 80 * dt
            return board.x >= board.enemy and 'success' or 'running'
        end),
    }),
    ai.action(function(board) board.x = board.x - 1 end),
}), {x = 0})

require('haylen.scene').push({
    update = function(self, dt)
        guard:tick(dt)
    end,
})
```

### tree:tick(dt), tree:reset()

`tick` advances the tree time by `dt`, ticks the root and returns its status. A leaf that ticks its own tree raises `The behavior tree is already ticking.` `reset` forgets the running children, counters and timers, keeping the blackboard.

```lua
local ai = require('haylen.ai')

local tree = ai.newBehaviorTree(ai.sequence({ai.wait(1), ai.action(function() print('done') end)}))
print(tree:tick(0.5), tree:tick(0.5)) -- running success
tree:reset()
```

### Tree properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `status` | string | read | Status of the last tick. |
| `blackboard` | table | read and write | Table the leaves receive. |
| `time` | number | read | Sum of every `dt` ticked. |
| `nodeCount` | integer | read | Number of nodes. |

```lua
local ai = require('haylen.ai')

local tree = ai.newBehaviorTree(ai.action(function(board) return board.ready end))
tree.blackboard = {ready = true}
print(tree:tick(0), tree.status, tree.nodeCount, tree.time) -- success success 1 0.0
```

## Utility AI

A `UtilitySelector` scores each option by the product of its considerations and picks the best one, such as attacking when the enemy is close and healthy or fleeing when health runs low. Each consideration reads an input from a context, maps it from its range to 0 to 1 and shapes it with a response curve. Each factor is then raised toward 1 by a share that grows with the number of considerations, so options with many considerations are not punished by the product, and the weight of the option scales it.

### ai.newUtilitySelector(options)

Creates a selector from a list of options. Unknown keys raise `Unknown option '<key>'.`, and a consideration whose range has two equal ends raises `A consideration needs an input and a range with two different ends.`

| Option key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | Name that `choose` returns. |
| `weight` | number | `1` | Multiplies the score. |
| `considerations` | table | required | List of considerations. |

| Consideration key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | Label for debugging. |
| `input` | function | required | Called with the context and returns a number. |
| `minimum`, `maximum` | number | `0`, `1` | Range of the input that maps to 0 and 1. |
| `curve` | table | linear | Response curve `{shape, slope, exponent, shift, offset}`. |

Curves follow the infinite axis utility system, with `x` the mapped input: `'linear'` gives `slope * (x - shift) + offset` and ignores the exponent, `'polynomial'` gives `slope * (x - shift) ^ exponent + offset` with inputs below the shift counted as the shift, `'logistic'` gives `exponent / (1 + e ^ (-slope * (x - shift))) + offset`, `'logit'` gives `slope * ln((x - shift) / (1 - x + shift)) / 5 + 0.5 + offset`, and `'normal'` gives `slope * e ^ (-exponent * (x - shift) ^ 2) + offset`. Results are clamped between 0 and 1, and slope, exponent, shift and offset default to `1`, `1`, `0` and `0`.

```lua
local ai = require('haylen.ai')

local brain = ai.newUtilitySelector({
    {name = 'attack', considerations = {
        {name = 'health', input = function(bot) return bot.health end, maximum = 100},
        {name = 'close', input = function(bot) return bot.distance end, maximum = 300, curve = {slope = -1, offset = 1}},
    }},
    {name = 'heal', weight = 0.8, considerations = {
        {name = 'hurt', input = function(bot) return bot.health end, maximum = 100, curve = {shape = 'logistic', slope = -12, shift = 0.4}},
    }},
})
print(brain:choose({health = 90, distance = 40})) -- attack
```

### selector:choose(context, random, tolerance)

Returns the name and score of the best option for the context, or `nil` when every option scores zero. With a `Random`, it picks at random, weighted by score, among the options that score above zero and at least `tolerance` times the best score, which makes agents less predictable. `tolerance` defaults to `0.9` and is clamped between `0` and `1`, so `1` keeps only the best options and `0` keeps every option that scores above zero.

```lua
local ai = require('haylen.ai')
local m = require('haylen.math')

local brain = ai.newUtilitySelector({
    {name = 'wander', considerations = {{input = function(bot) return bot.boredom end}}},
    {name = 'rest', considerations = {{input = function(bot) return bot.tired end}}},
})
local rng = m.random(4)
print(brain:choose({boredom = 0.8, tired = 0.7}, rng, 0.8))
```

### selector:score(option, context), selector.options

`score` returns the score of one option, by name or by position from 1, and an unknown name raises `unknown option`. `options` lists the option names.

```lua
local ai = require('haylen.ai')

local brain = ai.newUtilitySelector({
    {name = 'eat', considerations = {{input = function(bot) return bot.hunger end, maximum = 10}}},
})
print(brain:score('eat', {hunger = 5}), brain.options[1]) -- 0.5 eat
```

## Influence maps

An `InfluenceMap` is a grid of influence over the world, such as the threat of enemies, the presence of allies or the pull of resources, which agents read to choose where to go. Cell `(column, row)` covers the square from `{x, y} + {column, row} * cellSize`, counting cells from 0.

### ai.newInfluenceMap(options)

Creates a map from `{columns, rows, cellSize = 32, x = 0, y = 0}`. A side below 1 or a cell size that is not positive raises `An influence map needs at least one cell on each side and a positive cell size.`

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 64, rows = 36, cellSize = 30})
print(threat.columns, threat.rows, threat.cellSize, threat.origin)
```

### Map properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | read | Number of cells across. |
| `rows` | integer | read | Number of cells down. |
| `cellSize` | number | read | Side of every cell in world units. |
| `origin` | Vec2 | read | World position where cell `(0, 0)` starts, from the `x` and `y` options. |

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 20, rows = 10, cellSize = 16, x = 100, y = 50})
local width = threat.columns * threat.cellSize
local height = threat.rows * threat.cellSize
print(width, height, threat.origin) -- 320.0 160.0 Vec2(100.0, 50.0)
```

### map:stamp(x, y, strength, radius, falloff)

Adds `strength` to the cells whose centers lie within `radius` of the point, fading toward the edge with `'linear'` (the default), `'quadratic'` or `'constant'`.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 64, rows = 36, cellSize = 30})
for _, enemy in ipairs({{x = 300, y = 200}, {x = 900, y = 500}}) do
    threat:stamp(enemy.x, enemy.y, 1, 240, 'quadratic')
end
```

### map:propagate(decay, momentum), map:scale(factor), map:add(other, weight), map:fill(value)

`propagate` spreads influence across the map: every cell moves toward the strongest influence of its eight neighbors, reduced by `e ^ (-decay * distance in cells)`, and `momentum` is the share of its old value a cell keeps. `scale` multiplies every value, which fades old influence over time. `add` adds another map of the same size times `weight`, which defaults to `1`, and maps of other sizes raise `Only influence maps of the same size add up.` `fill` sets every value.

```lua
local ai = require('haylen.ai')

local allies = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
local enemies = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
allies:stamp(100, 100, 1, 80)
enemies:stamp(500, 400, 1, 80)
for i = 1, 5 do
    allies:propagate(0.3, 0.6)
    enemies:propagate(0.3, 0.6)
end
local tension = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
tension:add(allies)
tension:add(enemies, -1)
tension:scale(0.5)
```

### map:get(column, row), map:set(column, row, value), map:sample(x, y), map:values()

`get` and `set` read and write one cell and raise `The cell is outside the influence map.` outside it. `sample` returns the value at a world position, interpolated between cell centers, and 0 outside the map or for a NaN position. `values` returns every value row by row.

```lua
local ai = require('haylen.ai')

local heat = ai.newInfluenceMap({columns = 10, rows = 10, cellSize = 16})
heat:set(2, 3, 5)
print(heat:get(2, 3), heat:sample(40, 56), #heat:values()) -- 5.0 5.0 100
```

### map:findHighest(x, y, radius), map:findLowest(x, y, radius), map:cellCenter(column, row)

`findHighest` and `findLowest` return the center and value of the cell with the highest or lowest value among the cells whose centers lie within `radius` of the point, as three numbers, or `nil` when no cell does. A `radius` of `math.huge` searches the whole map. `cellCenter` returns the center of a cell.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
threat:stamp(600, 300, 1, 200)
local x, y, danger = threat:findLowest(620, 320, 300)
print('retreat to', x, y, danger)
print(threat:cellCenter(0, 0)) -- 20.0 20.0
```
