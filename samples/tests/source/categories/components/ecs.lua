-- ECS: a small entity component system, where an entity is a number, a component is a table of data stored by its name, and a system is a function that runs every frame over the entities that have all the components it asks for.
-- The module needs only Lua, so it copies as it is into another project. Create a world with `Ecs.new()`, spawn entities with `world:spawn({position = {x = 0, y = 0}, velocity = {x = 10, y = 0}})`, add systems with `world:system(name, {'position', 'velocity'}, function(entity, position, velocity, dt) end)`, and call `world:update(dt)` every frame.
-- Each component lives in a packed pool, a system walks the smallest pool of the components it asks for, and entities destroyed while systems run go away once the frame ends, so systems never see half of an entity.
local Ecs = {}
Ecs.__index = Ecs

function Ecs.new()
    return setmetatable({pools = {}, systems = {}, nextId = 0, alive = {}, count = 0, doomed = {}, doomedCount = 0, updating = false}, Ecs)
end

-- Returns the pool of a component, made on first use: the entities that have it in a packed list, where each sits, and its data.
function Ecs:pool(name)
    local pool = self.pools[name]
    if not pool then
        pool = {entities = {}, index = {}, data = {}, count = 0}
        self.pools[name] = pool
    end
    return pool
end

-- Makes an entity with the components of `components`, a table of names to data, and returns its number.
function Ecs:spawn(components)
    self.nextId = self.nextId + 1
    local entity = self.nextId
    self.alive[entity] = true
    self.count = self.count + 1
    for name, data in pairs(components or {}) do
        self:add(entity, name, data)
    end
    return entity
end

function Ecs:add(entity, name, data)
    if not self.alive[entity] then
        error('The entity ' .. tostring(entity) .. ' does not exist.', 2)
    end
    local pool = self:pool(name)
    local slot = pool.index[entity]
    if slot then
        pool.data[slot] = data
        return
    end
    pool.count = pool.count + 1
    pool.entities[pool.count], pool.data[pool.count], pool.index[entity] = entity, data, pool.count
end

-- Removes a component from an entity, moving the last of the pool into its slot so the pool stays packed.
function Ecs:remove(entity, name)
    local pool = self.pools[name]
    local slot = pool and pool.index[entity]
    if not slot then
        return
    end
    local last = pool.entities[pool.count]
    pool.entities[slot], pool.data[slot], pool.index[last] = last, pool.data[pool.count], slot
    pool.entities[pool.count], pool.data[pool.count], pool.index[entity] = nil, nil, nil
    pool.count = pool.count - 1
end

function Ecs:get(entity, name)
    local pool = self.pools[name]
    local slot = pool and pool.index[entity]
    return slot and pool.data[slot]
end

function Ecs:has(entity, name)
    local pool = self.pools[name]
    return pool ~= nil and pool.index[entity] ~= nil
end

-- Destroys an entity with all its components, at once outside of an update and when the frame ends inside one.
function Ecs:destroy(entity)
    if not self.alive[entity] then
        return
    end
    if self.updating then
        self.doomedCount = self.doomedCount + 1
        self.doomed[self.doomedCount] = entity
        return
    end
    for name in pairs(self.pools) do
        self:remove(entity, name)
    end
    self.alive[entity] = nil
    self.count = self.count - 1
end

-- Adds a system that runs `run(entity, component..., dt)` over every entity with all the components named in `needs`, up to four, in the order systems were added.
function Ecs:system(name, needs, run)
    if #needs < 1 or #needs > 4 then
        error('A system needs from one to four components.', 2)
    end
    local pools = {}
    for index, component in ipairs(needs) do
        pools[index] = self:pool(component)
    end
    local system = {name = name, needs = needs, pools = pools, run = run, enabled = true, visited = 0, values = {}}
    self.systems[#self.systems + 1] = system
    return system
end

function Ecs:findSystem(name)
    for _, system in ipairs(self.systems) do
        if system.name == name then
            return system
        end
    end
end

-- Runs one system over its entities, walking the smallest of the pools it needs.
function Ecs:runSystem(system, dt)
    local pools, smallest = system.pools, nil
    for index = 1, #pools do
        if not smallest or pools[index].count < smallest.count then
            smallest = pools[index]
        end
    end

    local values, needed, visited = system.values, #pools, 0
    -- The walk goes backwards over a snapshot of the count, so a component removed by the system itself never skips an entity.
    for slot = smallest.count, 1, -1 do
        local entity = smallest.entities[slot]
        local complete = entity ~= nil
        for index = 1, needed do
            local pool = pools[index]
            local at = complete and pool.index[entity]
            if not at then
                complete = false
                break
            end
            values[index] = pool.data[at]
        end
        if complete then
            visited = visited + 1
            -- The time comes right after the components the system asked for.
            if needed == 1 then
                system.run(entity, values[1], dt)
            elseif needed == 2 then
                system.run(entity, values[1], values[2], dt)
            elseif needed == 3 then
                system.run(entity, values[1], values[2], values[3], dt)
            else
                system.run(entity, values[1], values[2], values[3], values[4], dt)
            end
        end
    end
    system.visited = visited
end

function Ecs:update(dt)
    self.updating = true
    for _, system in ipairs(self.systems) do
        if system.enabled then
            self:runSystem(system, dt)
        end
    end
    self.updating = false

    for index = 1, self.doomedCount do
        self:destroy(self.doomed[index])
        self.doomed[index] = nil
    end
    self.doomedCount = 0
end

-- Answers how many entities have a component.
function Ecs:countOf(name)
    local pool = self.pools[name]
    return pool and pool.count or 0
end

return Ecs
