-- The log of the console, trace, crash and inspector tests: a card that fills the room beside their panel with the newest entries first, each a line in the accent color with its result under it, or a sentence that says what fills it while it is empty.
local haylen = require('haylen')
local ui = require('haylen.ui')

local code = require('harness.code')

local LogCard = haylen.class('LogCard')

LogCard.limit = 12

-- Takes the test that owns the card, the id of its text, its title, the sentence it shows while empty and its font role.
function LogCard:init(test, id, title, empty, font)
    self.test, self.id, self.title, self.empty, self.font = test, id, title, empty, font or 'body'
    self.entries = {}
end

function LogCard:node()
    return ui.card{grow = 1, width = 480, gap = 12, align = 'stretch',
        ui.row{gap = 12, alignItems = 'center',
            ui.label{text = self.title, font = 'heading', grow = 1},
            ui.label{text = 'Newest first', font = 'caption', color = 'textMuted'},
        },
        ui.scroll{grow = 1, height = 0, ui.richText{id = self.id, text = self:markup(), font = self.font}},
    }
end

-- Adds an entry with its line and the result under it, in the tone `danger` for a failure.
function LogCard:add(line, result, tone)
    table.insert(self.entries, 1, {line = line, result = result, tone = tone})
    self.entries[LogCard.limit + 1] = nil
    self.test:set(self.id, {text = self:markup()})
end

function LogCard:markup()
    if #self.entries == 0 then
        return string.format('[color=%s]%s[/color]', ui.themeColor('textMuted'):toHex(), code.escape(self.empty))
    end
    local accent, muted, danger = ui.themeColor('accentText'):toHex(), ui.themeColor('textMuted'):toHex(), ui.themeColor('dangerText'):toHex()
    local parts = {}
    for _, entry in ipairs(self.entries) do
        local result = entry.result and string.format('\n[color=%s]%s[/color]', entry.tone == 'danger' and danger or muted, code.escape(entry.result)) or ''
        parts[#parts + 1] = string.format('[color=%s]%s[/color]%s', accent, code.escape(entry.line), result)
    end
    return table.concat(parts, '\n\n')
end

return LogCard
