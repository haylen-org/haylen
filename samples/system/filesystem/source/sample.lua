-- What every test of the sample shares: the page with the Back button, the title, the description and the hint line, the way to and from the menu, and the formatting of sizes, times and bytes.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

function sample.open(entry)
    if not scene.transitioning() then
        scene.push(require(entry.module)(entry), sample.transition)
    end
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(sample.transition)
    end
end

-- Formats a byte count for people, such as 512 bytes or 3.4 KB.
function sample.bytes(count)
    if count < 1024 then
        return string.format('%d bytes', count)
    end
    if count < 1024 * 1024 then
        return string.format('%.1f KB', count / 1024)
    end
    return string.format('%.1f MB', count / (1024 * 1024))
end

-- Formats Unix seconds as a date and time of the player's clock.
function sample.time(seconds)
    return os.date('%Y-%m-%d %H:%M:%S', seconds)
end

-- Formats the first bytes of a string as rows of sixteen hexadecimal bytes.
function sample.hex(data, limit)
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
        rows[#rows + 1] = string.format('%s more', sample.bytes(#data - last))
    end
    return table.concat(rows, '\n')
end

-- Shows data as text when it is valid UTF-8 without control characters other than line breaks and tabs, and as hexadecimal bytes otherwise.
function sample.preview(data, limit)
    local text = data:sub(1, limit or 2048)
    if utf8.len(text) ~= nil and not text:find('[%z\1-\8\11\12\14-\31]') then
        return text .. (#data > #text and '\n…' or '')
    end
    return sample.hex(data, 256)
end

-- The base of every test scene. A test sets its hints and the control that takes the focus, and returns the nodes of its page from content.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'back'

function Test:init(entry)
    self.entry = entry
end

function Test:content()
    return {}
end

-- Mounts the page, which the scene owns, so it goes away when the scene unloads. The Back button, Escape, the east gamepad button and the Menu button of a TV remote return to the menu.
function Test:enter()
    window.setBackLeavesApp(false)
    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 16,
        onCancel = sample.back,
        ui.row{gap = 24,
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.entry.title, font = 'heading'},
                ui.label{text = self.entry.description, color = 'textMuted'},
            },
        },
        ui.row{height = 0, grow = 1, gap = 24, children = self:content()},
        ui.label{text = self.hints, font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
end

function Test:exit()
    window.setBackLeavesApp(true)
end

function Test:show(id, properties)
    self.document:set(id, properties)
end

return sample
