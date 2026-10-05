-- The module that the test "DEV-001" shows. Change the text and save the file while the test runs: the greeting changes at once, and the counter of the test keeps counting.
local greeting = {}

function greeting.text(seconds)
    return string.format('Hello from the first version, after %d seconds.', math.floor(seconds))
end

function greeting.color()
    return '#FF8FB0FF'
end

return greeting
