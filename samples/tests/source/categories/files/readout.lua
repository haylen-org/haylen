-- How the tests of the category show what they read and write: sizes, times, bytes as hexadecimal and previews of files.
local readout = {}

-- Formats a byte count for people, such as 512 bytes or 3.4 KB.
function readout.bytes(count)
    if count < 1024 then
        return string.format('%d bytes', count)
    end
    if count < 1024 * 1024 then
        return string.format('%.1f KB', count / 1024)
    end
    return string.format('%.1f MB', count / (1024 * 1024))
end

-- Formats Unix seconds as a date and time of the clock of the device.
function readout.time(seconds)
    return os.date('%Y-%m-%d %H:%M:%S', seconds)
end

-- Formats the first bytes of a string as rows of sixteen hexadecimal bytes.
function readout.hex(data, limit)
    local rows = {}
    local last = math.min(#data, limit or 256)
    for offset = 1, last, 16 do
        local codes = {data:byte(offset, math.min(offset + 15, last))}
        local cells = {}
        for index, code in ipairs(codes) do
            cells[index] = string.format('%02X', code)
        end
        rows[#rows + 1] = string.format('%04X  %s', offset - 1, table.concat(cells, ' '))
    end
    if #data > last then
        rows[#rows + 1] = string.format('And %s more', readout.bytes(#data - last))
    end
    return table.concat(rows, '\n')
end

-- Shows data as text when it is valid UTF-8 without control characters other than line breaks and tabs, and as hexadecimal bytes otherwise.
function readout.preview(data, limit)
    local text = data:sub(1, limit or 2048)
    if utf8.len(text) ~= nil and not text:find('[%z\1-\8\11\12\14-\31]') then
        return text .. (#data > #text and '\n…' or '')
    end
    return readout.hex(data, 256)
end

return readout
