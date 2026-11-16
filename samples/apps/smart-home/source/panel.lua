-- The GUI of the panel: the bar on top with the greeting and the time, and the cards in a grid that fits as many columns as the screen holds, from one on a phone to four on a wide wall tablet. The cards hold the house, the scenes, the climate, every room, the security, the energy and the devices, and every control reaches the home. The texts change only when what they show changes.
local ui = require('haylen.ui')

local Home = require('home')
local Style = require('style')
local Text = require('text')

local Panel = {}
Panel.__index = Panel

Panel.columnWidth = 360

function Panel.new(owner, home)
    local self = setmetatable({home = home, shown = {}}, Panel)
    local space = Style.space
    -- The cards of a like height share a row on a wide screen: the house, the scenes, the climate and the energy, then the rooms, then the security and the devices.
    local cards = {self:houseCard(), self:scenesCard(), self:climateCard(), self:energyCard()}
    for _, room in ipairs(home.rooms) do
        cards[#cards + 1] = self:roomCard(room)
    end
    cards[#cards + 1] = self:securityCard()
    cards[#cards + 1] = self:devicesCard()
    self.gui = ui.mount(ui.column{gap = 0,
        ui.row{gap = space.m, padding = {space.m, space.l, space.s, space.l},
            ui.column{gap = 0, grow = 1,
                ui.label{id = 'greeting', text = '', font = 'title', wrap = false},
                ui.label{id = 'summary', text = '', color = 'textMuted', wrap = false},
            },
            ui.label{id = 'clock', text = '', font = 'title', wrap = false},
        },
        ui.scroll{grow = 1, height = 0,
            ui.grid{id = 'cards', minColumnWidth = Panel.columnWidth, columns = 4, gap = space.l, padding = {space.s, space.l, space.xl, space.l}, alignItems = 'stretch', children = cards},
        },
    }, {owner = owner})
    self.notices = ui.mount(ui.toast{id = 'notice', position = 'top', duration = 3, indicator = 'icon'}, {owner = owner, layer = 10})
    home:on('notice', function(text)
        self.notices:command('notice', 'show', {text = text, tone = 'information'})
    end)
    home:on('changed', function()
        self:showControls()
    end)
    self:showControls()
    return self
end

function Panel.icon(name)
    return 'icons/' .. name .. '.svg'
end

function Panel.card(children)
    children.gap = children.gap or Style.space.m
    children.style = Style.card
    return ui.card(children)
end

-- A title row of a card: an icon in a soft round tile, the title and optional trailing nodes.
function Panel.title(icon, text, color, trailing)
    local row = {gap = Style.space.s, align = 'stretch',
        ui.icon{image = Panel.icon(icon), size = 30, color = color or 'text'},
        ui.label{text = text, font = 'heading', grow = 1, wrap = false},
    }
    if trailing then
        row[#row + 1] = trailing
    end
    return ui.row(row)
end

function Panel:houseCard()
    return Panel.card{
        ui.image{id = 'house', image = '@house', aspectRatio = 1.6, align = 'stretch', radius = 20, accessibilityLabel = 'The house'},
        ui.row{gap = Style.space.s, wrap = true,
            ui.statusIndicator{id = 'doors', text = ''},
            ui.statusIndicator{id = 'alarmChip', text = ''},
        },
    }
end

function Panel:scenesCard()
    local buttons = {}
    for _, id in ipairs(Home.sceneOrder) do
        local scene = Home.scenes[id]
        buttons[#buttons + 1] = ui.button{id = 'scene-' .. id, text = scene.text, icon = Panel.icon(scene.icon), iconPlacement = 'top', height = 104, align = 'stretch', accessibilityLabel = scene.text .. ' scene',
            onClick = function()
                self.home:applyScene(id)
            end}
    end
    return Panel.card{
        Panel.title('leaf', 'Scenes', 'accentText'),
        ui.grid{columns = 2, gap = Style.space.s, alignItems = 'stretch', children = buttons},
    }
end

function Panel:climateCard()
    local climate = self.home.climate
    return Panel.card{
        Panel.title('thermostat', 'Climate', 'accentText', ui.label{id = 'humidity', text = '', color = 'textMuted', wrap = false}),
        ui.image{id = 'dial', image = '@dial', aspectRatio = 1.6, align = 'stretch', accessibilityLabel = 'Thermostat'},
        ui.row{gap = Style.space.s, align = 'stretch',
            ui.button{id = 'cooler', icon = Panel.icon('minus'), iconColor = 'text', width = 64, accessibilityLabel = 'Cooler', onClick = function()
                climate.target = math.max(10, climate.target - 0.5)
            end},
            ui.segmentedControl{id = 'mode', grow = 1, items = Panel.modes, selected = climate.mode, onChange = function(event)
                climate.mode = event.value
            end},
            ui.button{id = 'warmer', icon = Panel.icon('plus'), iconColor = 'text', width = 64, accessibilityLabel = 'Warmer', onClick = function()
                climate.target = math.min(30, climate.target + 0.5)
            end},
        },
    }
end

Panel.modes = {{id = 'heat', text = 'Heat'}, {id = 'cool', text = 'Cool'}, {id = 'auto', text = 'Auto'}, {id = 'off', text = 'Off'}}

-- A room: its lights on or off, their brightness and their color.
function Panel:roomCard(room)
    local prefix = room.id .. '-'
    local swatches = {gap = Style.space.s}
    for _, entry in ipairs(Home.lightColors) do
        swatches[#swatches + 1] = ui.button{id = prefix .. entry.id, width = 52, height = 52, accessibilityLabel = entry.text .. ' light', tooltip = entry.text,
            style = {colors = {raised = entry.color, hover = entry.color, pressed = entry.color, border = Style.color.line}, metrics = {controlRadius = 26, controlPaddingX = 0}},
            onClick = function()
                room.color = entry.id
                room.on = true
                room.level = math.max(room.level, 10)
                self:showRoom(room)
            end}
    end
    return Panel.card{
        Panel.title(room.icon, room.text, nil, ui.toggle{id = prefix .. 'on', checked = room.on, accessibilityLabel = room.text .. ' lights', onChange = function(event)
            room.on = event.checked
            if room.on and room.level == 0 then
                room.level = 60
            end
            self:showRoom(room)
        end}),
        ui.label{id = prefix .. 'state', text = '', color = 'textMuted'},
        ui.slider{id = prefix .. 'level', min = 0, max = 100, step = 1, value = room.level, accessibilityLabel = room.text .. ' brightness', onChange = function(event)
            room.level = event.value
            room.on = room.level > 0
            self:showRoom(room)
        end},
        ui.row(swatches),
    }
end

function Panel:securityCard()
    local home = self.home
    local locks = {gap = Style.space.xs}
    for _, lock in ipairs(home.locks) do
        locks[#locks + 1] = ui.row{gap = Style.space.s,
            ui.icon{id = 'lock-' .. lock.id .. '-icon', image = Panel.icon(lock.locked and 'lock' or 'unlock'), size = 28, color = 'text'},
            ui.label{text = lock.text, grow = 1, wrap = false},
            ui.toggle{id = 'lock-' .. lock.id, checked = lock.locked, accessibilityLabel = lock.text .. ' locked', onChange = function(event)
                lock.locked = event.checked
                self:showControls()
            end},
        }
    end
    return Panel.card{
        Panel.title('camera', 'Security', 'accentText'),
        ui.image{id = 'camera', image = '@camera', aspectRatio = 16 / 9, align = 'stretch', radius = 18, accessibilityLabel = 'Front door camera'},
        ui.segmentedControl{id = 'alarm', items = Panel.alarms, selected = home.alarm, onChange = function(event)
            home.alarm = event.value
            self:showControls()
        end},
        ui.column(locks),
    }
end

Panel.alarms = {{id = 'off', text = 'Disarmed'}, {id = 'home', text = 'Home'}, {id = 'away', text = 'Away'}}

function Panel:energyCard()
    local stat = function(id, label)
        return ui.column{gap = 0, grow = 1, ui.label{id = id, text = '', font = 'heading', wrap = false}, ui.label{text = label, font = 'caption', color = 'textMuted', wrap = false}}
    end
    return Panel.card{
        Panel.title('sun', 'Energy today', 'accentText'),
        ui.image{id = 'energy', image = '@energy', aspectRatio = 2.2, align = 'stretch', accessibilityLabel = 'Solar and usage over the day'},
        ui.row{gap = Style.space.s, wrap = true, stat('solar', 'Solar'), stat('usage', 'Home'), stat('battery', 'Battery'), stat('grid', 'Grid')},
    }
end

function Panel:devicesCard()
    local home = self.home
    return Panel.card{
        Panel.title('blinds', 'Devices', 'accentText'),
        ui.label{id = 'blindsLabel', text = '', color = 'textMuted'},
        ui.slider{id = 'blinds', min = 0, max = 100, step = 5, value = home.blinds, accessibilityLabel = 'Blinds open', onChange = function(event)
            home.blinds = event.value
        end},
        ui.divider{},
        ui.row{gap = Style.space.s,
            ui.icon{image = Panel.icon('speaker'), size = 30, color = 'text'},
            ui.column{gap = 0, grow = 1, ui.label{id = 'track', text = home.speaker.track, wrap = false}, ui.label{text = home.speaker.artist, font = 'caption', color = 'textMuted', wrap = false}},
            ui.button{id = 'play', icon = Panel.icon('pause'), variant = 'primary', width = 64, accessibilityLabel = 'Play or pause', onClick = function()
                home.speaker.playing = not home.speaker.playing
                self:showControls()
            end},
        },
        ui.slider{id = 'volume', min = 0, max = 100, step = 1, value = home.speaker.volume, accessibilityLabel = 'Volume', onChange = function(event)
            home.speaker.volume = event.value
        end},
        ui.divider{},
        ui.row{gap = Style.space.s,
            ui.icon{image = Panel.icon('washer'), size = 30, color = 'text'},
            ui.label{text = 'Washer', grow = 1, wrap = false},
            ui.label{id = 'washerLeft', text = '', color = 'textMuted', wrap = false},
        },
        ui.progress{id = 'washer', value = 0},
    }
end

-- Sets a property of a node only when the value behind it changed.
-- Whether the value behind a readout changed since it last showed, which then becomes the value it shows.
function Panel:changed(id, value)
    if self.shown[id] == value then
        return false
    end
    self.shown[id] = value
    return true
end

-- Sets the text or the value of a node through one table that every call fills again, so a readout that changes builds no table.
function Panel:setText(id, text)
    Panel.textProperties.text = text
    self.gui:set(id, Panel.textProperties)
end

function Panel:setValue(id, value)
    Panel.valueProperties.value = value
    self.gui:set(id, Panel.valueProperties)
end

Panel.textProperties = {text = ''}
Panel.valueProperties = {value = 0}

function Panel:showRoom(room)
    local prefix = room.id .. '-'
    local color = self.home:lightColor(room.color)
    self.gui:set(prefix .. 'on', {checked = room.on})
    self.gui:set(prefix .. 'level', {value = room.level})
    local state = room.on and string.format('%d lights on · %d%% · %s', room.lights, room.level, color.text) or 'Lights off'
    self.gui:set(prefix .. 'state', {text = state})
    for _, entry in ipairs(Home.lightColors) do
        self.gui:set(prefix .. entry.id, {style = {colors = {raised = entry.color, hover = entry.color, pressed = entry.color, border = entry.id == room.color and Style.color.ink or Style.color.line}, metrics = {controlRadius = 26, controlPaddingX = 0, borderWidth = entry.id == room.color and 3 or 1}}})
    end
end

-- Shows the controls that a scene or another control changed.
function Panel:showControls()
    local home = self.home
    for _, room in ipairs(home.rooms) do
        self:showRoom(room)
    end
    for _, id in ipairs(Home.sceneOrder) do
        self.gui:set('scene-' .. id, {variant = id == home.scene and 'primary' or 'default', iconColor = id == home.scene and 'onAccent' or 'text'})
    end
    for _, lock in ipairs(home.locks) do
        self.gui:set('lock-' .. lock.id, {checked = lock.locked})
        self.gui:set('lock-' .. lock.id .. '-icon', {image = Panel.icon(lock.locked and 'lock' or 'unlock')})
    end
    self.gui:set('alarm', {selected = home.alarm})
    self.gui:set('mode', {selected = home.climate.mode})
    self.gui:set('blinds', {value = home.blinds})
    self.gui:set('play', {icon = Panel.icon(home.speaker.playing and 'pause' or 'play')})
    local unlocked = home:unlockedDoors()
    self.gui:set('doors', {text = unlocked == 0 and 'All doors locked' or (unlocked == 1 and '1 door unlocked' or unlocked .. ' doors unlocked'), tone = unlocked == 0 and 'success' or 'warning'})
    self.gui:set('alarmChip', {text = Panel.alarmTexts[home.alarm], tone = home.alarm == 'off' and 'warning' or 'success'})
end

Panel.alarmTexts = {off = 'Alarm off', home = 'Armed at home', away = 'Armed away'}

-- The texts that follow the time and the energy. Each one is made and set only in the frame its value changes, so the other frames build nothing.
function Panel:update()
    local home = self.home
    local minute = math.floor(home.minute % (24 * 60))
    if self:changed('clock', minute) then
        local hour = minute // 60
        self:setText('greeting', hour < 12 and 'Good morning' or (hour < 18 and 'Good afternoon' or 'Good evening'))
        self:setText('clock', Text.clock(minute * 60))
    end

    local lights = home:lightsOn()
    if self:changed('summary', lights) then
        self:setText('summary', lights == 0 and 'Every light is off' or string.format('%d lights on in the home', lights))
    end
    local humidity = math.floor(home.climate.humidity + 0.5)
    if self:changed('humidity', humidity) then
        self:setText('humidity', Text.format('Humidity %.0f%%', humidity, 1))
    end
    if self:changed('blindsLabel', home.blinds) then
        self:setText('blindsLabel', Text.format('Blinds %.0f%% open', home.blinds, 1))
    end

    local solar = home:solar()
    if self:changed('solar', math.floor(solar * 10)) then
        self:setText('solar', Text.format('%.1f kW', solar, 0.1))
    end
    local usage = home:consumption()
    if self:changed('usage', math.floor(usage * 10)) then
        self:setText('usage', Text.format('%.1f kW', usage, 0.1))
    end
    if self:changed('battery', math.floor(home.battery * 100)) then
        self:setText('battery', Text.format('%.0f%%', home.battery * 100, 1))
    end
    local _, grid = home:balance()
    if self:changed('grid', math.floor(math.abs(grid) * 10)) then
        self:setText('grid', Text.format('%.1f kW', math.abs(grid), 0.1))
    end

    local washer = home.washer
    local left = math.ceil(washer.left)
    if self:changed('washer', left) then
        self:setText('washerLeft', washer.running and Text.duration(left * 60) .. ' left' or 'Done')
        self:setValue('washer', 1 - washer.left / washer.total)
    end
end

return Panel
