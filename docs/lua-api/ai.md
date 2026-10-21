# 📖 `haylen.ai`

The module `haylen.ai` holds four tools that decide what the characters of an app do. A state machine keeps a character in one named state at a time, such as idle, patrol or attack. A behavior tree makes layered decisions again on every tick. A utility selector weighs several needs and picks the best action. An influence map spreads values such as threat over a grid, so agents can choose where to go. This page is for developers who write the behavior of enemies, allies, pets and bosses.

```lua
local ai = require('haylen.ai')
```

The engine never runs these tools on its own. Call them from a scene `update` or `fixedUpdate` with the time step that you want them to use. Every function that a tool calls, such as a state function or a leaf of a tree, runs inside the method that calls it, such as `machine:update` or `tree:tick`. So these functions cannot yield a coroutine.

## State machines

A state machine is in one named state at a time. Each state is a table of up to three functions, which run when the machine enters the state, updates it and leaves it. Use state machines for enemy behavior, for player states such as idle, running and jumping, and for flows such as a boss fight with phases.

### ai.newStateMachine(states)

Creates a state machine and returns it as a [`StateMachine`](#statemachine). The table `states` maps each state name to a state table. The machine starts without a current state, until the first [`machine:change`](#machinechangename-).

The names of the states are fixed when the machine is created. The machine keeps the table `states` and looks up each function every time it runs it. So you can replace a function later, and a state whose table you removed from `states` changes without calling anything.

Every function of a state table is optional.

| Field | Runs | Arguments |
| --- | --- | --- |
| `enter` | When the machine changes to this state. | The machine, then the extra arguments of `machine:change`. |
| `update` | On every `machine:update` while this state is current. | The machine and the `dt` of `machine:update`. |
| `exit` | When the machine leaves this state for another one or for itself. | The machine. |

The argument `states` must be a table. A key that is not a string raises `bad argument #1 to 'newStateMachine' (state names must be strings)`, and an empty name raises `A state needs a name.`. A value that is not a table raises `bad argument #1 to 'newStateMachine' (each state must be a table)`.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

local goblin = {x = 0, patrolEnd = 0}

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

scene.push({
    update = function(self, dt)
        goblin.brain:update(dt)
    end,
})
```

## StateMachine

A `StateMachine`, named `haylen.StateMachine` in messages, is the machine that [`ai.newStateMachine`](#ainewstatemachinestates) returns. Reading a member that it does not have raises `The type "haylen.StateMachine" has no member "<name>".`. Writing a property that is read-only raises `The type "haylen.StateMachine" has no writable property "<name>".`.

### machine:change(name, ...)

Changes to the state `name`. These steps run in order:

1. The `exit` function of the current state, when there is one.
2. The function [`machine.onChange`](#machineonchange). Before it runs, `machine.current` already names the new state and `machine.elapsed` is back at `0`.
3. The `enter` function of the new state, with the extra arguments of `change`.

Changing to the current state leaves it and enters it again.

A change that `enter`, `exit` or `onChange` asks for waits until the running change finishes. Then it runs with its own arguments, in the order of the requests. An error raised by a state function reaches the caller of `change`, and the changes that waited in that transition are dropped. An unknown name raises `The state machine has no state named "<name>".`.

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
            print('Game over.')
        end,
    },
})

machine:change('alive')
machine:change('hurt', 2)
machine:change('hurt', 2)
print(machine.current) -- dead
```

### machine:update(dt)

Adds the number `dt` to `machine.elapsed` and calls the `update` function of the current state with the machine and `dt`. It does nothing before the first change. The engine never updates machines, so call this from a scene `update` or `fixedUpdate` with the time step that you want the machine to use.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

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

scene.push({
    update = function(self, dt)
        lamp:update(dt)
    end,
})
```

### machine:has(name)

Returns `true` when the machine has a state named `name`, and `false` otherwise.

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
print(machine.current) -- attack
```

### machine.current

Read-only name of the current state, or `nil` before the first change.

```lua
local ai = require('haylen.ai')

local door = ai.newStateMachine({open = {}, closed = {}})
door:change('closed')
if door.current == 'closed' then
    print('The door is closed.')
end
```

### machine.previous

Read-only name of the state before the current one, or `nil` until the second change. After a change to the same state, it names that state.

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

Read-only number of seconds in the current state. It adds up the `dt` values given to `machine:update`, and it returns to `0` on every change.

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

A function that runs on every change as `onChange(machine, from, to)`, after the `exit` of the old state and before the `enter` of the new one. The argument `from` is `nil` on the first change. The property can be read and written, and it is `nil` until you set it. Assign `nil` to remove the function. Assigning a value that is neither a function nor `nil` raises `bad argument #3 to 'newindex' (function expected, got <type>)`.

```lua
local ai = require('haylen.ai')

local hero = ai.newStateMachine({idle = {}, run = {}, jump = {}})

hero.onChange = function(machine, from, to)
    print(string.format('The hero went from %s to %s.', tostring(from), to))
end

hero:change('idle')
hero:change('run')
hero.onChange = nil
```

## Behavior trees

A behavior tree decides what an agent does by walking a tree of nodes on every tick. Each tick of a node ends with one of three statuses: `'success'`, `'failure'` or `'running'`, which means that the node is not done yet and wants another tick. Branch nodes, such as sequences and selectors, decide which children run. Decorators change the result of their one child. Leaves are Lua functions that test or do something with the blackboard, a table that every leaf of the tree receives.

Build a tree in two steps. First describe its nodes with the node functions below, such as `ai.sequence` and `ai.action`, which return plain tables. Then pass the description of the root to [`ai.newBehaviorTree`](#ainewbehaviortreeroot-blackboard), and tick the tree on every update. Sequences and selectors remember the child that is running and go on from it on the next tick. The root starts over from its first child after it finishes.

### Node descriptions

A node description is a plain table. Its `kind` names the node. Depending on the kind, it also holds `children`, a list of descriptions, `count`, `seconds` or `fn`, the function of a leaf. You can write descriptions by hand instead of calling the node functions. The tree reads them once, when `ai.newBehaviorTree` builds it, so later changes to a description do not reach the tree.

| Kind | Keys |
| --- | --- |
| `'sequence'`, `'selector'` | `kind`, `children` |
| `'inverter'`, `'succeeder'`, `'failer'` | `kind`, `children` with one child |
| `'parallel'` | `kind`, `children`, `count` |
| `'repeater'`, `'retry'` | `kind`, `children` with one child, `count` |
| `'cooldown'`, `'timeout'` | `kind`, `children` with one child, `seconds` |
| `'wait'` | `kind`, `seconds` |
| `'condition'`, `'action'` | `kind`, `fn` |

The `count` of a parallel node is the number of successes that it needs. A `count` or `seconds` that a description leaves out counts as `0`. Any other key, such as a misspelled `second`, makes `ai.newBehaviorTree` raise `Unknown option "<key>".`. The decorators `'inverter'`, `'succeeder'`, `'failer'`, `'repeater'`, `'retry'`, `'cooldown'` and `'timeout'` need exactly one child, or `ai.newBehaviorTree` raises `A behavior tree decorator needs exactly one child.`.

```lua
local ai = require('haylen.ai')

-- These two descriptions build the same node.
local fromFunctions = ai.cooldown(ai.action(function(board) board.shots = board.shots + 1 end), 0.5)
local byHand = {kind = 'cooldown', seconds = 0.5, children = {{kind = 'action', fn = function(board) board.shots = board.shots + 1 end}}}
print(fromFunctions.kind, byHand.kind) -- cooldown cooldown
```

### ai.sequence(children)

Returns the description of a sequence. The table `children` lists the descriptions of its children. A sequence runs its children in order. It fails as soon as a child fails, and it succeeds when every child succeeded. While a child is running, the sequence is running too. The next tick goes on from that child, without running the children before it again. A sequence without children succeeds.

A `children` that is not a table raises `bad argument #1 to 'sequence' (table expected, got <type>)`. The other node functions check their arguments in the same way.

```lua
local ai = require('haylen.ai')

-- Walks to the door and then opens it.
local enterRoom = ai.sequence({
    ai.action(function(board, dt)
        board.x = board.x + 100 * dt
        return board.x >= board.doorX and 'success' or 'running'
    end),
    ai.action(function(board)
        board.doorOpen = true
    end),
})
```

### ai.selector(children)

Returns the description of a selector, which tries its children in order until one succeeds. It succeeds as soon as a child succeeds, and it fails when every child failed. While a child is running, the selector is running too, and the next tick goes on from that child. A selector without children fails. Use it for plans with fallbacks: the first child is the best plan, and each next child is the plan to try when the ones before it fail.

```lua
local ai = require('haylen.ai')

-- Attacks a close enemy, or else chases a seen one, or else rests.
local brain = ai.selector({
    ai.sequence({
        ai.condition(function(board) return board.distance < 40 end),
        ai.action(function(board) board.attacks = board.attacks + 1 end),
    }),
    ai.sequence({
        ai.condition(function(board) return board.distance < 300 end),
        ai.action(function(board, dt) board.distance = board.distance - 80 * dt return 'running' end),
    }),
    ai.action(function(board) board.resting = true end),
})
print(brain.kind) -- selector
```

### ai.parallel(children, successes)

Returns the description of a parallel node, which ticks all its unfinished children on the same tick. It succeeds once `successes` children succeeded. It fails once so many children failed that `successes` can no longer be reached. A child that finished keeps its result and does not run again until the parallel node finishes. When the node finishes, it resets all its children.

The integer `successes` is required, and it must be between `1` and the number of children. Otherwise `ai.newBehaviorTree` raises `A parallel node needs between one success and as many successes as it has children.`.

```lua
local ai = require('haylen.ai')

-- Runs to the exit while the alarm rings, and succeeds once the run succeeds.
local escape = ai.parallel({
    ai.action(function(board, dt)
        board.x = board.x + 150 * dt
        return board.x >= board.exitX and 'success' or 'running'
    end),
    ai.action(function(board)
        board.alarm = true
        return 'running'
    end),
}, 1)
```

### ai.inverter(child)

Returns the description of a decorator that turns a success of `child` into a failure and a failure into a success. A running child keeps it running.

```lua
local ai = require('haylen.ai')

-- Succeeds while nobody saw the thief.
local unseen = ai.inverter(ai.condition(function(board) return board.seen end))
```

### ai.succeeder(child)

Returns the description of a decorator that turns every finished result of `child` into a success. A running child keeps it running. Use it for an optional step that must not stop a sequence.

```lua
local ai = require('haylen.ai')

-- Tries to pick up loot and goes home whether or not that worked.
local trip = ai.sequence({
    ai.succeeder(ai.action(function(board) return board.loot ~= nil end)),
    ai.action(function(board) board.goingHome = true end),
})
```

### ai.failer(child)

Returns the description of a decorator that turns every finished result of `child` into a failure. A running child keeps it running.

```lua
local ai = require('haylen.ai')

-- Shouts and then fails, so the selector goes on to its next plan.
local plans = ai.selector({
    ai.failer(ai.action(function(board) board.shouted = true end)),
    ai.action(function(board) board.hidden = true end),
})
```

### ai.repeater(child, count)

Returns the description of a decorator that runs `child` again after each success, once per tick. After a success it reports `'running'`, so the next tick runs the child again. It succeeds after `count` successes, and a `count` of `0` or less makes it repeat forever. A failure of the child fails the repeater and starts its count over.

```lua
local ai = require('haylen.ai')

-- Fires three shots, one per tick.
local burst = ai.repeater(ai.action(function(board) board.shots = board.shots + 1 end), 3)

-- Paces back and forth for as long as the tree runs.
local pace = ai.repeater(ai.action(function(board) board.direction = -board.direction end), 0)
```

### ai.retry(child, count)

Returns the description of a decorator that runs `child` again after each failure, once per tick. After a failure it reports `'running'`, so the next tick runs the child again. It fails after `count` failures, and a `count` of `0` or less makes it retry forever. A success of the child succeeds the retry and starts its count over.

```lua
local ai = require('haylen.ai')

-- Tries the locked door up to five times.
local open = ai.retry(ai.action(function(board) return board.keys > 0 end), 5)
```

### ai.cooldown(child, seconds)

Returns the description of a decorator that lets `child` run at most once every `seconds`. After the child finishes, with a success or a failure, the cooldown fails without running the child until `seconds` of tree time passed. Tree time is the sum of every `dt` that [`tree:tick`](#treetickdt) received. The number `seconds` is required, and [`tree:reset`](#treereset) ends every cooldown.

```lua
local ai = require('haylen.ai')

-- Throws a grenade at most every 4 seconds.
local throw = ai.cooldown(ai.action(function(board) board.grenades = board.grenades - 1 end), 4)
```

### ai.timeout(child, seconds)

Returns the description of a decorator that gives `child` at most `seconds` to finish. It counts the `dt` of each tick while the child runs, starting with the tick that started it. When the child still runs after more than `seconds`, the timeout fails and resets the child and everything under it. When the child finishes in time, the timeout reports its result and its clock starts over. The number `seconds` is required.

```lua
local ai = require('haylen.ai')

-- Gives up the search after 10 seconds.
local search = ai.timeout(ai.action(function(board, dt)
    board.searched = board.searched + dt
    return board.found and 'success' or 'running'
end), 10)
```

### ai.wait(seconds)

Returns the description of a leaf that reports `'running'` until the `dt` of its ticks add up to `seconds`. Then it succeeds, and its clock starts over. Only the ticks that reach the node count. The number `seconds` is required.

```lua
local ai = require('haylen.ai')

-- Waits one second between two shots.
local volley = ai.sequence({
    ai.action(function(board) board.shots = board.shots + 1 end),
    ai.wait(1),
    ai.action(function(board) board.shots = board.shots + 1 end),
})
```

### ai.condition(fn)

Returns the description of a leaf that calls `fn(blackboard)`. It succeeds when `fn` returns a true value, which is anything but `false` and `nil`, and it fails otherwise. A condition never reports `'running'`. The argument `fn` must be a function, or it raises `bad argument #1 to 'condition' (function expected, got <type>)`.

```lua
local ai = require('haylen.ai')

local lowHealth = ai.condition(function(board) return board.health < 25 end)
```

### ai.action(fn)

Returns the description of a leaf that calls `fn(blackboard, dt)`, where `dt` is the time step of the tick. The value that `fn` returns becomes the status of the leaf. The argument `fn` must be a function, or it raises `bad argument #1 to 'action' (function expected, got <type>)`.

| `fn` returns | Status |
| --- | --- |
| `'success'`, `'failure'` or `'running'` | That status. |
| `true` | `'success'` |
| `false` | `'failure'` |
| Nothing or `nil` | `'success'` |

Another status name raises `A behavior tree action returned the unknown status "<name>".`, and a value of another type raises `A behavior tree action must return a status name, a boolean or nothing.`. Both errors reach the caller of `tree:tick`.

```lua
local ai = require('haylen.ai')

-- Turns until it faces the target, a little more on every tick.
local aim = ai.action(function(board, dt)
    board.angle = board.angle + 2 * dt
    if board.angle >= board.targetAngle then
        return 'success'
    end
    return 'running'
end)
```

### ai.newBehaviorTree(root, blackboard)

Builds a [`BehaviorTree`](#behaviortree) from the node description `root` and returns it. The table `blackboard` is optional and defaults to a new empty table. The same description may appear in several places of a tree. Each place becomes a node of its own, with its own progress.

A description that breaks the rules raises one of these errors, and no tree is built.

- A `kind` that is missing or unknown raises `Unknown behavior tree node "<kind>". The kind of a node is "sequence", "selector", "parallel", "inverter", "succeeder", "failer", "repeater", "retry", "cooldown", "timeout", "wait", "condition" or "action".`.
- A key that the kind does not take raises `Unknown option "<key>".`, and a key that is not a string, such as children written straight into the description, raises `Option tables only accept string keys.`.
- A `count` or `seconds` of the wrong type raises `The option "<key>" of "newBehaviorTree" is invalid: <reason>.`.
- A leaf without a function raises `A behavior tree leaf needs a function in "fn", not <type>.`.
- A decorator without exactly one child raises `A behavior tree decorator needs exactly one child.`.
- A parallel node with a wrong count raises `A parallel node needs between one success and as many successes as it has children.`.
- A description that contains itself raises `A behavior tree description contains itself.`.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

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

scene.push({
    update = function(self, dt)
        guard:tick(dt)
    end,
})
```

## BehaviorTree

A `BehaviorTree`, named `haylen.BehaviorTree` in messages, is the tree that [`ai.newBehaviorTree`](#ainewbehaviortreeroot-blackboard) returns. Its leaves run on the Lua thread that ticks it. Reading a member that it does not have raises `The type "haylen.BehaviorTree" has no member "<name>".`. Writing a property that is read-only raises `The type "haylen.BehaviorTree" has no writable property "<name>".`.

### tree:tick(dt)

Adds the number `dt` to `tree.time`, ticks the root once and returns its status: `'success'`, `'failure'` or `'running'`. Every action of the tick receives the same `dt`. A leaf that ticks its own tree raises `The behavior tree is already ticking.`. A tick that raises an error leaves the tree ready for the next tick.

```lua
local ai = require('haylen.ai')

local tree = ai.newBehaviorTree(ai.sequence({
    ai.wait(1),
    ai.action(function() print('One second passed.') end),
}))
print(tree:tick(0.5), tree:tick(0.5)) -- running success
```

### tree:reset()

Forgets the running children, the counts of repeaters and retries, and the clocks of waits, timeouts and cooldowns. It keeps the blackboard and `tree.time`, and it sets `tree.status` back to `'success'`. A leaf that resets its own tree raises `The behavior tree cannot reset while it ticks. Call "reset" after "tick" returns.`.

```lua
local ai = require('haylen.ai')

local patrol = ai.newBehaviorTree(ai.sequence({ai.wait(3), ai.action(function(board) board.turns = board.turns + 1 end)}), {turns = 0})
patrol:tick(2)

-- The guard heard a noise, so the patrol starts over.
patrol:reset()
print(patrol:tick(2)) -- running
```

### Tree properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `status` | string | `'success'` | Read-only status of the last tick. It is `'success'` before the first tick and after a reset. |
| `blackboard` | table | a new table | The table that the leaves receive. You can read it and replace it. Assigning a value that is not a table raises `bad argument #3 to 'newindex' (table expected, got <type>)`. |
| `time` | number | `0` | Read-only sum of every `dt` that the tree ticked. A reset keeps it. |
| `nodeCount` | integer | | Read-only number of nodes, where every place of a shared description counts. |

```lua
local ai = require('haylen.ai')

local tree = ai.newBehaviorTree(ai.action(function(board) return board.ready end))
tree.blackboard = {ready = true}
print(tree:tick(0), tree.status, tree.nodeCount, tree.time) -- success success 1 0.0
```

## Utility AI

A utility selector picks the best of several options, such as attack, heal or flee. Each option has considerations, the facts that matter for it, such as its health or the distance to the enemy. A consideration turns one input into a factor from 0 to 1, and the score of an option is the product of its factors. So an option scores high only when all its considerations favor it.

### ai.newUtilitySelector(options)

Creates a [`UtilitySelector`](#utilityselector) from the list `options`, which holds one table per option.

| Option key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | The name that `choose` returns and that `score` finds. |
| `weight` | number | `1` | Multiplies the score, so an option can count more or less than the others. |
| `considerations` | table | required | The list of its considerations. An option with an empty list scores its weight. |

| Consideration key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | `''` | A label for debugging. |
| `input` | function | required | Called with the context of `choose` or `score`, and returns a number. |
| `minimum` | number | `0` | The input that maps to 0. |
| `maximum` | number | `1` | The input that maps to 1. A `minimum` above the `maximum` turns the input around, so a low input maps near 1. |
| `curve` | table | linear | The [response curve](#response-curves) that shapes the mapped input. |

The score of an option comes from these steps:

1. Each input maps from its range between `minimum` and `maximum` to a value from 0 to 1, and values outside the range are clamped.
2. The response curve turns that value into a factor, clamped between 0 and 1.
3. A factor of 0 makes the score of the option 0.
4. With `n` considerations, each factor `f` becomes `f + (1 - f) * (1 - 1 / n) * f`. This raises the factors toward 1 by a share that grows with `n`, so the product does not punish an option for having many considerations.
5. The product of the factors, times the weight, is the score.

An unknown key raises `Unknown option "<key>".`, and a key of the wrong type, such as a `weight` of `'high'`, raises `The option "<key>" of "newUtilitySelector" is invalid: <reason>.`. A consideration whose `minimum` equals its `maximum` raises `A consideration needs an input and a range with two different ends.`. An option or a consideration that is not a table, an option without a `considerations` table and a consideration without an `input` function raise a `bad argument` error.

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
local choice = brain:choose({health = 90, distance = 40})
print(choice) -- attack
```

### Response curves

The table `curve` of a consideration shapes the mapped input, called `x` below, into a factor. All its keys are optional.

| Curve key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `shape` | string | `'linear'` | One of the shapes below. |
| `slope` | number | `1` | How steep the curve is. A negative slope makes the curve fall instead of rise. |
| `exponent` | number | `1` | The power of a polynomial, the height of a logistic curve or how narrow the bell of a normal curve is. |
| `shift` | number | `0` | Moves the curve along `x`. |
| `offset` | number | `0` | Moves the curve up or down. |

| Shape | Factor |
| --- | --- |
| `'linear'` | `slope * (x - shift) + offset`, a straight line. It ignores `exponent`. |
| `'polynomial'` | `slope * (x - shift) ^ exponent + offset`, where an `x` below the shift counts as the shift. |
| `'logistic'` | `exponent / (1 + e ^ (-slope * (x - shift))) + offset`, an S curve. |
| `'logit'` | `slope * ln((x - shift) / (1 - x + shift)) / 5 + 0.5 + offset`, an S curve on its side. The value `x - shift` stays just inside 0 and 1, where the logarithm would be infinite. |
| `'normal'` | `slope * e ^ (-exponent * (x - shift) ^ 2) + offset`, a bell around the shift. |

The factor is clamped between 0 and 1. An unknown shape raises `The option "shape" of "newUtilitySelector" is invalid: unknown value '<shape>'.`. A curve key of the wrong type raises the same kind of message for that key.

```lua
local ai = require('haylen.ai')

-- Hunger matters little at first and a lot near the end.
local eat = ai.newUtilitySelector({
    {name = 'eat', considerations = {
        {input = function(pet) return pet.hunger end, maximum = 10, curve = {shape = 'polynomial', exponent = 3}},
    }},
})
print(eat:score('eat', {hunger = 5})) -- 0.125
```

## UtilitySelector

A `UtilitySelector`, named `haylen.UtilitySelector` in messages, is the selector that [`ai.newUtilitySelector`](#ainewutilityselectoroptions) returns. Its inputs run on the Lua thread that asks for a score. Reading a member that it does not have raises `The type "haylen.UtilitySelector" has no member "<name>".`.

### selector:choose(context, random, tolerance)

Scores every option and returns the name and the score of the chosen one, or `nil` when every option scores 0 or less. The `context` is any Lua value, such as the agent, and every input receives it.

- Without `random`, it chooses the best option, and an earlier option wins a tie.
- With a [`Random`](math.md#random) from [`m.random`](math.md#mrandomseed), it picks at random among the options that score above 0 and at least `tolerance` times the best score. Each one has a chance that grows with its score, which makes agents less predictable. The number `tolerance` defaults to `0.9` and is clamped between `0` and `1`. So `1` keeps only the best options, and `0` keeps every option that scores above 0.

An input that returns something other than a number raises `A utility input must return a number, not <type>.`. A random choice among scores that are not finite, such as with a `weight` of `math.huge`, raises `Weights must be finite and not negative.`.

```lua
local ai = require('haylen.ai')
local m = require('haylen.math')

local brain = ai.newUtilitySelector({
    {name = 'wander', considerations = {{input = function(bot) return bot.boredom end}}},
    {name = 'rest', considerations = {{input = function(bot) return bot.tired end}}},
})
local rng = m.random(4)
local name, score = brain:choose({boredom = 0.8, tired = 0.7}, rng, 0.8)
print(string.format('The bot chose %s with a score of %.2f.', name, score))
```

### selector:score(option, context)

Returns the score of one option for the context. The argument `option` is the name of the option or its position in the list, counted from 1. An unknown name raises `bad argument #1 to 'score' (unknown option)`, and a position outside the list raises `bad argument #1 to 'score' (no option with this number)`.

```lua
local ai = require('haylen.ai')

local brain = ai.newUtilitySelector({
    {name = 'eat', considerations = {{input = function(bot) return bot.hunger end, maximum = 10}}},
})
print(brain:score('eat', {hunger = 5}), brain:score(1, {hunger = 10})) -- 0.5 1.0
```

### selector.options

Read-only list of the names of the options, in the order of the list that created the selector. Every read returns a new table.

```lua
local ai = require('haylen.ai')

local brain = ai.newUtilitySelector({
    {name = 'fight', considerations = {}},
    {name = 'flee', considerations = {}},
})
for index, name in ipairs(brain.options) do
    print(index .. ' ' .. name)
end
```

## Influence maps

An influence map is a grid of numbers laid over the world. Each cell holds how much of something it has, such as the threat of enemies, the presence of allies or the pull of resources. Agents read the map to choose where to go, such as the safest cell nearby or the place where the enemies gather. Cells are counted from 0. The cell `(column, row)` covers the square that starts at `x + column * cellSize`, `y + row * cellSize`, with sides of `cellSize`.

### ai.newInfluenceMap(options)

Creates an [`InfluenceMap`](#influencemap) and returns it. Every cell starts at 0.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | required | The number of cells across, at least `1`. |
| `rows` | integer | required | The number of cells down, at least `1`. |
| `cellSize` | number | `32` | The side of every cell in world units, finite and above 0. |
| `x` | number | `0` | The world `x` where the first column starts. |
| `y` | number | `0` | The world `y` where the first row starts. |

A side below 1 or a wrong cell size raises `An influence map needs at least one cell on each side and a finite cell size above 0.`. An unknown option raises `Unknown option "<key>".`, and an option of the wrong type, such as a `columns` of `2.5`, raises `The option "<key>" of "newInfluenceMap" is invalid: <reason>.`.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 64, rows = 36, cellSize = 30})
print(threat.columns, threat.rows, threat.cellSize, threat.origin)
```

## InfluenceMap

An `InfluenceMap`, named `haylen.InfluenceMap` in messages, is the grid that [`ai.newInfluenceMap`](#ainewinfluencemapoptions) returns. Reading a member that it does not have raises `The type "haylen.InfluenceMap" has no member "<name>".`. Writing one of its properties raises `The type "haylen.InfluenceMap" has no writable property "<name>".`.

### Map properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer | | Read-only number of cells across. |
| `rows` | integer | | Read-only number of cells down. |
| `cellSize` | number | `32` | Read-only side of every cell in world units. |
| `origin` | [`Vec2`](math.md#vec2) | `Vec2(0, 0)` | Read-only world position where the cell `(0, 0)` starts, from the options `x` and `y`. |

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 20, rows = 10, cellSize = 16, x = 100, y = 50})
local width = threat.columns * threat.cellSize
local height = threat.rows * threat.cellSize
print(width, height, threat.origin) -- 320.0 160.0 Vec2(100.0, 50.0)
```

### map:stamp(x, y, strength, radius, falloff)

Adds `strength` to every cell whose center lies within `radius` of the world point `x`, `y`. A negative strength takes influence away. The string `falloff` decides how the strength fades toward the edge of the circle, with `d` the distance of a cell center from the point. A `radius` of 0 or less does nothing.

| Falloff | The cell receives |
| --- | --- |
| `'linear'` | `strength * (1 - d / radius)`. This is the default. |
| `'quadratic'` | `strength * (1 - d / radius) ^ 2`, which drops faster near the edge. |
| `'constant'` | `strength` everywhere in the circle. |

An unknown falloff raises `bad argument #5 to 'stamp' (unknown value '<falloff>')`.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 64, rows = 36, cellSize = 30})
for _, enemy in ipairs({{x = 300, y = 200}, {x = 900, y = 500}}) do
    threat:stamp(enemy.x, enemy.y, 1, 240, 'quadratic')
end
```

### map:propagate(decay, momentum)

Spreads influence one cell further across the map. Every cell moves toward the strongest influence of its eight neighbors, which is the one farthest from 0, whether it is positive or negative. That influence is reduced by `e ^ (-decay * distance)`, where the distance is 1 cell for side neighbors and about 1.41 cells for corner neighbors. The number `momentum`, from 0 to 1, is the share of its old value that a cell keeps. So `1` changes nothing, and `0` replaces every cell with the influence that reaches it. Call it once per update to let influence flow over time.

```lua
local ai = require('haylen.ai')

local scent = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
scent:stamp(100, 100, 1, 40)
for step = 1, 5 do
    scent:propagate(0.3, 0.6)
end
```

### map:scale(factor)

Multiplies every value by `factor`. Scaling by a factor below 1 on every update fades old influence over time.

```lua
local ai = require('haylen.ai')
local scene = require('haylen.scene')

local noise = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
scene.push({
    update = function(self, dt)
        noise:scale(0.95)
    end,
})
```

### map:add(other, weight)

Adds the values of the map `other`, times `weight`, to the values of this map, cell by cell. The number `weight` defaults to `1`, and a negative weight subtracts, such as allies minus enemies. Both maps need the same number of columns and rows. Otherwise it raises `Only influence maps of the same size add up.`.

```lua
local ai = require('haylen.ai')

local allies = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
local enemies = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
allies:stamp(100, 100, 1, 80)
enemies:stamp(500, 400, 1, 80)

local control = ai.newInfluenceMap({columns = 40, rows = 30, cellSize = 20})
control:add(allies)
control:add(enemies, -1)
```

### map:fill(value)

Sets every cell to `value`, such as `0` to clear the map before the next stamps.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
threat:stamp(600, 300, 1, 200)
threat:fill(0)
print(threat:get(15, 7)) -- 0.0
```

### map:get(column, row)

Returns the value of the cell at the integers `column` and `row`, both counted from 0. A cell outside the map raises `The cell (<column>, <row>) is outside the influence map, whose columns go from 0 to <last column> and rows from 0 to <last row>.`.

```lua
local ai = require('haylen.ai')

local heat = ai.newInfluenceMap({columns = 10, rows = 10, cellSize = 16})
heat:stamp(40, 56, 4, 32, 'constant')
print(heat:get(2, 3)) -- 4.0
```

### map:set(column, row, value)

Sets the value of the cell at the integers `column` and `row`, both counted from 0. A cell outside the map raises the same error as `map:get`.

```lua
local ai = require('haylen.ai')

local heat = ai.newInfluenceMap({columns = 10, rows = 10, cellSize = 16})
heat:set(2, 3, 5)
print(heat:get(2, 3)) -- 5.0
```

### map:sample(x, y)

Returns the value at the world point `x`, `y`, blended between the four nearest cell centers. Between the outer cell centers and the edge of the map, the value stays flat. A point outside the map, or a point that is not a number such as NaN, gives 0.

```lua
local ai = require('haylen.ai')

local heat = ai.newInfluenceMap({columns = 10, rows = 10, cellSize = 16})
heat:set(2, 3, 5)
heat:set(3, 3, 1)
print(heat:sample(40, 56), heat:sample(48, 56)) -- 5.0 3.0
```

### map:values()

Returns a new list of every value, row by row, starting with row 0. The cell `(column, row)` is at the index `row * map.columns + column + 1`. It suits drawing the map for debugging.

```lua
local ai = require('haylen.ai')

local heat = ai.newInfluenceMap({columns = 10, rows = 10, cellSize = 16})
heat:set(2, 3, 5)
local values = heat:values()
print(#values, values[3 * heat.columns + 2 + 1]) -- 100 5.0
```

### map:findHighest(x, y, radius)

Looks at the cells whose centers lie within `radius` of the world point `x`, `y`. It returns three numbers, the `x` and `y` of the center of the cell with the highest value and that value, or `nil` when no cell center lies within the radius. When cells tie, the first one wins, row by row from the cell `(0, 0)`. A `radius` of `math.huge` searches the whole map.

```lua
local ai = require('haylen.ai')

local food = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
food:stamp(900, 500, 1, 120)
local x, y, value = food:findHighest(640, 360, math.huge)
if x then
    print(string.format('The best food is at %.0f, %.0f.', x, y))
end
```

### map:findLowest(x, y, radius)

Works like [`map:findHighest`](#mapfindhighestx-y-radius), but returns the cell with the lowest value, such as the safest place to retreat to.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
threat:stamp(600, 300, 1, 200)
local x, y, danger = threat:findLowest(620, 320, 300)
if x then
    print(string.format('Retreat to %.0f, %.0f.', x, y))
end
```

### map:cellCenter(column, row)

Returns the world `x` and `y` of the center of the cell at `column` and `row`. It accepts cells outside the map too, and gives the centers they would have.

```lua
local ai = require('haylen.ai')

local threat = ai.newInfluenceMap({columns = 32, rows = 18, cellSize = 40})
print(threat:cellCenter(0, 0)) -- 20.0 20.0
```

## Errors

| Message | Cause |
| --- | --- |
| `bad argument #1 to 'newStateMachine' (state names must be strings)` | A key of the states table of `ai.newStateMachine` is not a string. |
| `bad argument #1 to 'newStateMachine' (each state must be a table)` | A state of `ai.newStateMachine` is not a table. |
| `A state needs a name.` | A state of `ai.newStateMachine` has an empty name. |
| `The state machine has no state named "<name>".` | The method `machine:change` names an unknown state. |
| `bad argument #3 to 'newindex' (function expected, got <type>)` | A value other than a function or `nil` was assigned to `machine.onChange`. |
| `bad argument #3 to 'newindex' (table expected, got <type>)` | A value other than a table was assigned to `tree.blackboard`. |
| `Unknown behavior tree node "<kind>". The kind of a node is "sequence", "selector", "parallel", "inverter", "succeeder", "failer", "repeater", "retry", "cooldown", "timeout", "wait", "condition" or "action".` | A node description has no `kind` or an unknown one. |
| `A behavior tree leaf needs a function in "fn", not <type>.` | A condition or action description has no function. |
| `A behavior tree decorator needs exactly one child.` | A decorator description has no child or several. |
| `A parallel node needs between one success and as many successes as it has children.` | The `successes` of `ai.parallel`, or the `count` of a parallel description, is below 1 or above the number of children. |
| `A behavior tree description contains itself.` | A description holds itself among its children, at any depth. |
| `A behavior tree action returned the unknown status "<name>".` | An action returned a string other than the three statuses. |
| `A behavior tree action must return a status name, a boolean or nothing.` | An action returned a value of another type. |
| `The behavior tree is already ticking.` | A leaf called `tick` on its own tree. |
| `The behavior tree cannot reset while it ticks. Call "reset" after "tick" returns.` | A leaf called `reset` on its own tree. |
| `A consideration needs an input and a range with two different ends.` | A consideration has the same `minimum` and `maximum`. |
| `A utility input must return a number, not <type>.` | An input returned something other than a number. |
| `Weights must be finite and not negative.` | A random choice met a score that is not finite. |
| `bad argument #1 to 'score' (unknown option)` | The method `selector:score` names an unknown option. |
| `bad argument #1 to 'score' (no option with this number)` | The position given to `selector:score` is outside the list. |
| `An influence map needs at least one cell on each side and a finite cell size above 0.` | The `columns`, `rows` or `cellSize` of `ai.newInfluenceMap` are out of range. |
| `bad argument #5 to 'stamp' (unknown value '<falloff>')` | The falloff of `map:stamp` is unknown. |
| `Only influence maps of the same size add up.` | The maps of `map:add` have different numbers of columns or rows. |
| `The cell (<column>, <row>) is outside the influence map, whose columns go from 0 to <last column> and rows from 0 to <last row>.` | The methods `map:get` or `map:set` name a cell outside the map. |
| `The option "<key>" of "<function>" is invalid: <reason>.` | A key of a description, an option, a consideration, a curve or the options of `ai.newInfluenceMap` has the wrong type, or a curve has an unknown shape. |
| `Unknown option "<key>".` | A description, an option, a consideration, a curve or the options of `ai.newInfluenceMap` hold a key that they do not take. |
| `Option tables only accept string keys.` | One of those tables holds a key that is not a string. |
| `The type "<type>" has no member "<name>".` | The code read a member that the type does not have. |
| `The type "<type>" has no writable property "<name>".` | The code wrote a property that is read-only. |
