-- The resume state: what a run keeps with `storage.setResumeState` reaches the disk when the app goes to the background, and only a launch after the system ended the app there reads it back with `storage.resumeState`, together with its age.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ResumeState = haylen.class('ResumeState', Test)

ResumeState.screens = {{id = 'map', text = 'Map'}, {id = 'shop', text = 'Shop'}, {id = 'inventory', text = 'Inventory'}}

function ResumeState:enter()
    self.state = {screen = 'map', coins = 0}
    self:frame{
        hint = 'Pick a screen and earn coins, send the app to the background, end it from the system and open it again, and this test shows what the app kept. A launch after the app quit by itself finds nothing.',
        focus = 'screen',
        content = {ui.panel{align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'What this launch found'},
            ui.label{id = 'found', text = ResumeState.describeFound()},
            ui.sectionTitle{text = 'What the app keeps now'},
            ui.segmentedControl{id = 'screen', items = ResumeState.screens, selected = 'map', onChange = function(event)
                self.state.screen = event.value
                self:keep()
            end},
            ui.button{id = 'coins', text = 'Earn a coin', onClick = function()
                self.state.coins = self.state.coins + 1
                self:keep()
            end},
            ui.label{id = 'kept', text = ''},
        }},
    }
    self:keep()
end

-- The project keeps no resume state outside this test.
function ResumeState:exit()
    storage.setResumeState(nil)
    ResumeState.super.exit(self)
end

function ResumeState.describeFound()
    local value, age = storage.resumeState()
    if type(value) ~= 'table' then
        return 'Nothing, because the run before quit, came back to the foreground or never went to the background.'
    end
    return string.format('The screen "%s" with %d coins, kept %.0f seconds before this launch.', tostring(value.screen), tonumber(value.coins) or 0, age)
end

function ResumeState:keep()
    storage.setResumeState(self.state)
    self:set('kept', {text = string.format('The call "storage.setResumeState" keeps the screen "%s" with %d coins.', self.state.screen, self.state.coins)})
end

return ResumeState
