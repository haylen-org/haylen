-- How the tests of the category time and show what they send and receive: the wall clock, sizes, bodies and binary data.
local datetime = require('datetime')
local json = require('json')

local readout = {}

-- The wall clock in milliseconds, which times requests and round trips.
function readout.millis()
    return datetime.now():millis()
end

function readout.clock()
    return os.date('%H:%M:%S')
end

-- Formats a byte count for people, such as 512 bytes or 3.4 KB.
function readout.bytes(count)
    if count < 1024 then
        return string.format('%d bytes', count)
    end
    if count < 1024 * 1024 then
        return string.format('%.1f KB', count / 1024)
    end
    return string.format('%.2f MB', count / (1024 * 1024))
end

-- Shows a body as indented JSON when it is JSON, and as it came otherwise.
function readout.body(text)
    local ok, value = pcall(json.decode, text)
    if ok and type(value) == 'table' then
        return json.encode(value, {pretty = true})
    end
    return text
end

-- Formats binary data as hexadecimal bytes.
function readout.hex(data)
    return (data:gsub('.', function(byte)
        return string.format('%02X ', byte:byte())
    end))
end

return readout
