-- The menu of the sample: one button per test next to its description, with the focus on a button for gamepads and TV remotes.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')
local tests = require('tests')

local Menu = {}
Menu.__index = Menu

local kPadding = 32
local kGap = 24
local kHeaderHeight = 110

function Menu.new()
    return setmetatable({selected = tests[1].id}, Menu)
end

function Menu:enter()
    local rows = {}
    for _, test in ipairs(tests) do
        rows[#rows + 1] = ui.row{gap = 24,
            ui.button{id = test.id, text = test.title, width = 440, onClick = function()
                self.selected = test.id
                sample.open(test)
            end},
            ui.label{text = test.description, color = 'textMuted', grow = 1},
        }
    end

    self.document = ui.mount(ui.column{
        padding = {kPadding, 48},
        gap = kGap,
        ui.pageHeader{id = 'header', title = haylen.config.name, caption = 'Rigid bodies, joints and the physics helpers of Haylen, one test per scene.'},
        ui.scroll{id = 'list', height = self:listHeight(), ui.column{gap = 12, padding = {0, 24, 0, 0}, children = rows}},
    })
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

function Menu:exit()
    self.document:unmount()
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
