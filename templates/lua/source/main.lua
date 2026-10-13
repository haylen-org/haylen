-- The starter app of Haylen: a logo, a title, a button that counts the presses and the answer of the native code of the platform.
local haylen = require('haylen')
local platform = require('haylen.platform')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Home = {}
Home.__index = Home

function Home.new()
    return setmetatable({presses = 0, greetings = 0, answer = 'Asking the native code of the platform.'}, Home)
end

function Home:enter()
    self.gui = ui.mount(ui.column{
        align = 'center',
        gap = 32,
        ui.image{image = 'logo.png', width = 256, height = 256, align = 'center'},
        ui.label{text = haylen.config.name, font = 'title', align = 'center'},
        ui.label{text = 'Made with Haylen ' .. haylen.version .. ' on "' .. haylen.platform .. '"', color = 'textMuted', align = 'center'},
        ui.button{id = 'press', text = 'Press me', variant = 'primary', align = 'center', onClick = function()
            self:press()
        end},
        ui.label{id = 'presses', text = 'Nobody pressed the button yet.', align = 'center'},
        ui.label{id = 'native', text = self.answer, color = 'textMuted', align = 'center'},
    })

    -- Gamepads and TV remotes move the focus, so the button starts with it.
    self.gui:command('press', 'focus')
    self:greet()
end

function Home:press()
    self.presses = self.presses + 1
    local text = self.presses == 1 and 'Pressed once.' or ('Pressed ' .. self.presses .. ' times.')
    self.gui:set('presses', {text = text})
end

-- Calls "app.greet" through the platform bridge, which the native code of the platform projects answers: "AppBridge.swift" on Apple platforms, "App.kt" on Android and "app.js" on the web. After each answer the native code sends "app.greeted" with the count of greetings.
function Home:greet()
    platform.on('app.greeted', function(event)
        self.greetings = event.count
        self:showGreeting()
    end, {owner = self})
    scene.spawn(self, function()
        local answer, err = platform.call('app.greet', {name = 'Lua'}):await()
        self.answer = answer and answer.message or ('The native code did not answer: ' .. err.message)
        self:showGreeting()
    end)
end

function Home:showGreeting()
    local text = self.answer
    if self.greetings > 0 then
        text = text .. ' It sent ' .. self.greetings .. (self.greetings == 1 and ' event.' or ' events.')
    end
    self.gui:set('native', {text = text})
end

function Home:exit()
    self.gui:unmount()
end

scene.push(Home.new())
