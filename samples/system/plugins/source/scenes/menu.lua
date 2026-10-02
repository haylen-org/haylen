-- The menu of the sample: one button per test next to its description, with the focus on a button for gamepads and TV remotes.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local sample = require('sample')
local tests = require('tests')

local Menu = haylen.class('Menu', scene.Scene)

local kPadding = 32
local kGap = 24
local kHeaderHeight = 110

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
        padding = {kPadding, 48},
        gap = kGap,
        ui.pageHeader{id = 'header', title = haylen.config.name, caption = 'The "native-demo" plugin of this app, written with the APIs of each platform alone, tests every capability of native plugins: calls, events, bytes, streams, batched events, parameters, native views over the app, native UI that covers it, screens of the plugin, native results, permissions and notifications, requirements of the project, opened URLs and app errors.'},
        ui.scroll{id = 'list', height = self:listHeight(), ui.column{gap = 12, padding = {0, 24, 0, 0}, children = rows}},
    }, {owner = self})
    self.document:command(self.selected, 'focus')
end

-- The list scrolls in the height the safe area leaves under the header, measured once the header has been drawn.
function Menu:listHeight()
    local header = self.document and self.document:bounds('header')
    return math.max(200, viewport.safeRect().height - kPadding * 2 - kGap - (header and header.height or kHeaderHeight))
end

function Menu:update(dt)
    local height = self:listHeight()
    if height ~= self.height then
        self.height = height
        self.document:set('list', {height = height})
    end
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
