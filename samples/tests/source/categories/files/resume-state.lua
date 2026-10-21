-- The resume state: what a run keeps with `storage.setResumeState` reaches the disk when the app goes to the background, and only a launch after the system ended the app there reads it back with `storage.resumeState`, together with its age.
local haylen = require('haylen')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ResumeState = haylen.class('ResumeState', Test)

ResumeState.code = [[
storage.setResumeState({screen = 'shop', coins = 12})  -- Reaches the disk when the app goes to the background.
local value, age = storage.resumeState()  -- Found only after the system ended the app.
if value and age < 600 then openScreen(value.screen) end]]

ResumeState.screens = {{id = 'map', text = 'Map'}, {id = 'shop', text = 'Shop'}, {id = 'inventory', text = 'Inventory'}}
ResumeState.names = {map = 'Map', shop = 'Shop', inventory = 'Inventory'}
ResumeState.pictures = {map = 'interface/icons/flag.png', shop = 'interface/themes/fantasy/icons/gem.png', inventory = 'interface/themes/fantasy/icons/potion.png'}

function ResumeState:enter()
    self.state = {screen = 'map', coins = 0}
    self:frame{
        code = ResumeState.code,
        hint = 'Pick a screen and earn coins, send the app to the background, end it from the system and open it again, and this test shows what the app kept. A launch after the app quit by itself finds nothing.',
        focus = 'screen',
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 460, grow = 1, gap = 12,
                ui.sectionTitle{text = 'What this launch found'},
                self:found(),
            },
            ui.panel{width = 560, grow = 1, gap = 16,
                ui.sectionTitle{text = 'What the app keeps now'},
                ui.segmentedControl{id = 'screen', items = ResumeState.screens, selected = 'map', onChange = function(event)
                    self.state.screen = event.value
                    self:keep()
                end},
                ui.card{gap = 20, align = 'stretch',
                    ui.row{gap = 24, alignItems = 'center',
                        ui.image{id = 'picture', image = ResumeState.pictures.map, scale = 0.75},
                        ui.column{gap = 6, grow = 1,
                            ui.label{id = 'screenName', text = 'Map', font = 'title'},
                            ui.row{gap = 10, alignItems = 'center',
                                ui.image{image = 'interface/themes/fantasy/icons/coin.png', scale = 0.3},
                                ui.label{id = 'coinCount', text = '', font = 'heading', color = 'warningText'},
                            },
                        },
                    },
                },
                ui.button{id = 'coins', text = 'Earn a coin', align = 'stretch', onClick = function()
                    self.state.coins = self.state.coins + 1
                    self:keep()
                end},
                ui.label{id = 'kept', text = '', color = 'textMuted'},
            },
        }},
    }
    self:keep()
end

-- The project keeps no resume state outside this test.
function ResumeState:exit()
    storage.setResumeState(nil)
    ResumeState.super.exit(self)
end

-- What the launch found, as an alert with the screen and the coins, or the empty state when the run before kept nothing.
function ResumeState:found()
    local value, age = storage.resumeState()
    if type(value) ~= 'table' then
        return ui.emptyState{title = 'Nothing kept', message = 'The run before quit, came back to the foreground or never went to the background.', grow = 1}
    end
    return ui.alert{tone = 'success', title = string.format('The %s with %d coins', ResumeState.names[value.screen] or tostring(value.screen), tonumber(value.coins) or 0), message = string.format('Kept %.0f seconds before this launch.', age)}
end

function ResumeState:keep()
    storage.setResumeState(self.state)
    self:set('picture', {image = ResumeState.pictures[self.state.screen]})
    self:set('screenName', {text = ResumeState.names[self.state.screen]})
    self:set('coinCount', {text = self.state.coins == 1 and '1 coin' or string.format('%d coins', self.state.coins)})
    self:set('kept', {text = string.format('The call "storage.setResumeState" keeps the screen "%s" with %d coins.', self.state.screen, self.state.coins)})
end

return ResumeState
