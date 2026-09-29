-- A run on the island. It opens the pause menu on the pause action and ends shortly after the survivor falls. The engine halts the run while the app is out of focus or in the background.
local input = require('haylen.input')
local scene = require('haylen.scene')

local game = require('systems.game')
local hud = require('ui.hud')
local preferences = require('systems.preferences')
local sound = require('systems.sound')

local gameplay = {}
gameplay.__index = gameplay

local endDelay = 2

function gameplay.new(class)
    return setmetatable({class = class}, gameplay)
end

-- Loads the enemies, effects, sounds and music of a run while the transition covers the screen.
function gameplay:load(context)
    context:preload('gameplay')
end

function gameplay:enter()
    self.game = game.new(self.class)
    self.hud = hud.new(self.game, function()
        self:openPause()
    end)
    self.game.onNotice = function(key)
        self.hud:notice(key)
    end
    self.endTime = 0
end

function gameplay:exit()
    self.hud:destroy()
    self.game:destroy()
end

function gameplay:pause()
    self.paused = true
    self.hud:setVisible(false)
    input.clearVirtual()
end

-- Leaves the run for another screen once the menu over it closes. The run stays frozen and dimmed during the fade.
function gameplay:leave(next)
    self.next = next
end

function gameplay:resume()
    if self.next then
        scene.replace(self.next, {duration = 1, color = '#FF1B1E2B'})
        return
    end
    self.paused = false
    self.hud:setVisible(true)
    self.hud:setTouch(preferences.get('touch'))
end

function gameplay:openPause()
    if not self.game.over and scene.top() == self and not scene.transitioning() then
        scene.push(require('scenes.pause').new(self))
    end
end

function gameplay:update(dt)
    if self.next then
        return
    end
    if input.pressed('pause') then
        self:openPause()
        return
    end

    self.game:update(dt)
    self.hud:update()

    if self.game.over then
        self.endTime = self.endTime + dt
        if self.endTime >= endDelay and not self.ended then
            self.ended = true
            local player = self.game.player
            local record = preferences.record(self.game.cycle.day, self.game.kills, player.class.id)
            sound.music('gameOver', 1)
            scene.push(require('scenes.game-over').new(self, self.game.cycle.day, self.game.kills, record))
        end
    end
end

function gameplay:render()
    self.game:draw(self.paused)
end

return gameplay
