-- The part of the automatic run that one machine takes when several run the project side by side. The variable "HAYLEN_TESTS_SHARD" of the headless player names it as "<index>/<count>", such as "2/4". Whole categories go to the shards, the category with the most tests first, each to the shard with the fewest tests so far, so the shards take about the same time and together run every test once.
local shard = {}

shard.variable = 'HAYLEN_TESTS_SHARD'

-- Returns the index and the count of the shard that the variable names, or nothing when the run takes every test.
function shard.read()
    local text = os.getenv(shard.variable)
    if text == nil or text == '' then
        return nil
    end
    local index, count = text:match('^(%d+)/(%d+)$')
    index, count = tonumber(index), tonumber(count)
    if index == nil or index < 1 or index > count then
        error(string.format('The variable "%s" holds "%s", which names no shard. Give it as "<index>/<count>", such as "2/4".', shard.variable, text), 0)
    end
    return index, count
end

-- Keeps the tests of the queue whose categories go to the shard, in the order of the queue.
function shard.select(queue, index, count)
    local categories = {}
    local sizes = {}
    for _, test in ipairs(queue) do
        local category = test.category
        if not sizes[category] then
            categories[#categories + 1] = category
            sizes[category] = 0
        end
        sizes[category] = sizes[category] + 1
    end

    -- The sort breaks ties by the place of a category in the queue, so every machine computes the same shards.
    local places = {}
    for place, category in ipairs(categories) do
        places[category] = place
    end
    table.sort(categories, function(first, second)
        if sizes[first] ~= sizes[second] then
            return sizes[first] > sizes[second]
        end
        return places[first] < places[second]
    end)

    local loads = {}
    for part = 1, count do
        loads[part] = 0
    end
    local owners = {}
    for _, category in ipairs(categories) do
        local lightest = 1
        for part = 2, count do
            if loads[part] < loads[lightest] then
                lightest = part
            end
        end
        owners[category] = lightest
        loads[lightest] = loads[lightest] + sizes[category]
    end

    local selected = {}
    for _, test in ipairs(queue) do
        if owners[test.category] == index then
            selected[#selected + 1] = test
        end
    end
    return selected
end

return shard
