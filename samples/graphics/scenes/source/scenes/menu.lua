-- The menu of the sample: one button per test with its description, in a list that scrolls.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')
local tests = require('tests')

local Menu = haylen.class('Menu', scene.Scene)

-- Takes the id of the test to focus, the one the player comes back from.
function Menu:init(selected)
    self.selected = selected or tests[1].id
end

function Menu:enter()
    -- The menu is the root screen, where the back button of a TV or an Android device leaves the app.
    window.setBackLeavesApp(true)
    local rows = {}
    for index, entry in ipairs(tests) do
        rows[index] = ui.row{
            gap = 24,
            ui.button{id = entry.id, text = entry.title, width = 440, onClick = function()
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

return Menu
