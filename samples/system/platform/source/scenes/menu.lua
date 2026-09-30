-- The menu of the sample: one button per test next to its description, with the focus on a button for gamepads and TV remotes.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')
local tests = require('tests')

local Menu = haylen.class('Menu', scene.Scene)

function Menu:init()
    self.selected = tests[1].id
end

function Menu:enter()
    local rows = {}
    for _, test in ipairs(tests) do
        rows[#rows + 1] = ui.row{gap = 24,
            ui.button{id = test.id, text = test.title, width = 460, onClick = function()
                self.selected = test.id
                sample.open(test)
            end},
            ui.label{text = test.description, color = 'textMuted', grow = 1},
        }
    end

    self.document = ui.mount(ui.column{
        padding = {32, 48},
        gap = 24,
        ui.pageHeader{title = haylen.config.name, caption = 'The bridge to native code with native events and a handler of this app on every platform, what the window reports and what haylen.system tells about the device, one test per scene.'},
        ui.scroll{grow = 1, ui.column{gap = 12, padding = {0, 24, 0, 0}, children = rows}},
    }, {owner = self})
    self.document:command(self.selected, 'focus')
end

function Menu:pause()
    self.document.visible = false
end

-- Coming back from a test puts the focus on the button that opened it, and the back button of TVs leaves the app again.
function Menu:resume()
    window.setBackLeavesApp(true)
    self.document.visible = true
    self.document:command(self.selected, 'focus')
end

return Menu
