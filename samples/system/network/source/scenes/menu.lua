-- The menu of the sample: one button per test next to its description, in a list that scrolls, with the focus on a button for gamepads and TV remotes.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local sample = require('sample')
local tests = require('tests')

local Menu = haylen.class('Menu', scene.Scene)

function Menu:init()
    self.selected = tests[1].id
end

function Menu:enter()
    local rows = {}
    for index, entry in ipairs(tests) do
        rows[index] = ui.row{gap = 24,
            ui.button{id = entry.id, text = entry.title, width = 440, onClick = function()
                self.selected = entry.id
                sample.open(entry)
            end},
            ui.label{text = entry.description, color = 'textMuted', grow = 1},
        }
    end

    self.document = ui.mount(ui.column{
        padding = {32, 48},
        gap = 24,
        ui.pageHeader{title = haylen.config.name, caption = 'Pick a test. Back, Escape or the B button returns to this menu.'},
        ui.scroll{height = 0, grow = 1, ui.column{gap = 12, padding = {0, 24, 0, 0}, children = rows}},
    }, {owner = self})
    self.document:command(self.selected, 'focus')
end

function Menu:pause()
    self.document.visible = false
end

-- Coming back from a test puts the focus on the button that opened it.
function Menu:resume()
    self.document.visible = true
    self.document:command(self.selected, 'focus')
end

return Menu
