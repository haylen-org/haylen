-- The callbacks of the test "DEV-003". Change "first" to "second" in every function and save the file while the test runs: the module function and the call through the table run the new code at once, while the closure that "ticks.makeClosure" made before the save keeps its old body.
local ticks = {}

-- A function the test hands to a timer as it is, which writes into the table that the test puts in "ticks.target".
function ticks.record()
    ticks.target.moduleFunction = 'first'
end

-- A function the timer reaches through the module table on every call.
function ticks.label()
    return 'first'
end

-- Makes a closure once, when the test enters, which the timer keeps.
function ticks.makeClosure(target)
    return function()
        target.closure = 'first'
    end
end

return ticks
