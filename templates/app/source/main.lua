-- The starter app of Haylen: a logo, a title, a button and a label that counts the presses.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Home = {}
Home.__index = Home

function Home.new()
    return setmetatable({presses = 0}, Home)
end

function Home:enter()
    self.document = ui.mount(ui.column{
        align = 'center',
        gap = 32,
        ui.image{image = 'logo.png', width = 256, height = 256, align = 'center'},
        ui.label{text = haylen.config.name, font = 'title', align = 'center'},
        ui.label{text = 'Made with Haylen ' .. haylen.version .. ' on ' .. haylen.platform, color = 'textMuted', align = 'center'},
        ui.button{id = 'press', text = 'Press me', variant = 'primary', align = 'center', onClick = function()
            self:press()
        end},
        ui.label{id = 'presses', text = 'Nobody pressed the button yet.', align = 'center'},
    })

    -- Gamepads and TV remotes move the focus, so the button starts with it.
    self.document:command('press', 'focus')
end

function Home:press()
    self.presses = self.presses + 1
    local text = self.presses == 1 and 'Pressed once.' or ('Pressed ' .. self.presses .. ' times.')
    self.document:set('presses', {text = text})
end

function Home:exit()
    self.document:unmount()
end

scene.push(Home.new())
