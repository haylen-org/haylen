-- The survivor choice: the camera frames the chosen class at the fire while a paper sheet compares its stats.
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local art = require('systems.art')
local classes = require('data.classes')
local loadingView = require('ui.loading')
local preferences = require('systems.preferences')
local sound = require('systems.sound')
local widgets = require('ui.widgets')

local classSelect = {}
classSelect.__index = classSelect

-- Stats shown as bars, each relative to the best class at it.
local stats = {
    {key = 'health', value = 'health'},
    {key = 'speed', value = 'speed'},
    {key = 'damage', value = 'damage'},
    {key = 'range', value = 'range'},
    {key = 'chop', value = 'chop'},
    {key = 'carry', value = 'capacity'},
}

local function classById(id)
    for _, class in ipairs(classes) do
        if class.id == id then
            return class
        end
    end
end

function classSelect.new(scenery)
    return setmetatable({backdrop = scenery}, classSelect)
end

-- The Menu button of a TV remote and the Back button of Android reach this screen and the ones after it as Escape instead of leaving the app.
function classSelect:enter()
    window.setBackLeavesApp(false)
    local cards = {}
    for _, class in ipairs(classes) do
        cards[#cards + 1] = ui.column{
            gap = 4,
            ui.imageButton{id = 'pick_' .. class.id, image = art.portrait(class), scale = 0.66, align = 'center', onClick = function()
                sound.play('click')
                self:select(class.id)
            end},
            ui.label{id = 'name_' .. class.id, text = widgets.text('classes.' .. class.id .. '.name'), font = 'button', color = 'onAccent', outline = '#FF1B1E2B', outlineWidth = 3, align = 'center'},
        }
    end

    local rows = {
        ui.label{id = 'name', font = 'heading'},
        ui.label{id = 'special', color = 'textMuted'},
    }
    for _, stat in ipairs(stats) do
        rows[#rows + 1] = ui.row{
            gap = 20,
            ui.label{text = widgets.text('classes.' .. stat.key), width = 190, wrap = false},
            ui.progress{id = 'stat_' .. stat.key, grow = 1},
        }
    end

    -- The stats sheet and the buttons share the right side next to the cards, so the sheet shows every row on short screens, such as TVs inside their safe area and phones in landscape.
    self.document = ui.mount(ui.column{
        padding = {40, 64},
        gap = 24,
        onCancel = function()
            sound.play('back')
            self:back()
        end,
        ui.pageHeader{title = widgets.text('classes.title'), banner = true, textAlign = 'center', align = 'center'},
        ui.row{
            grow = 1,
            gap = 24,
            ui.column{
                grow = 1,
                align = 'stretch',
                ui.spacer{grow = 1},
                ui.row{justify = 'center', gap = 24, children = cards},
            },
            ui.column{
                width = 640,
                align = 'stretch',
                gap = 24,
                ui.panel{gap = 14, children = rows},
                ui.spacer{grow = 1},
                ui.row{
                    gap = 32,
                    widgets.button('back', 'classes.back', function()
                        self:back()
                    end, {variant = 'default', sound = 'back', grow = 1}),
                    widgets.button('start', 'classes.start', function()
                        self:start()
                    end, {sound = 'confirm', grow = 1}),
                },
            },
        },
    })
    self:select(preferences.get('class'))
    self.document:command('start', 'focus')
end

function classSelect:exit()
    self.document:unmount()
end

function classSelect:select(id)
    local class = classById(id) or classes[1]
    self.class = class
    self.backdrop:focus(class.id)
    self.backdrop:cheer(class.id)

    local document = self.document
    document:set('name', {text = widgets.text('classes.' .. class.id .. '.name')})
    document:set('special', {text = widgets.text('classes.' .. class.id .. '.special')})
    for _, stat in ipairs(stats) do
        local best = 0
        for _, other in ipairs(classes) do
            best = math.max(best, other[stat.value])
        end
        document:set('stat_' .. stat.key, {value = class[stat.value] / best})
    end
    for _, other in ipairs(classes) do
        local chosen = other == class
        document:set('pick_' .. other.id, {tint = chosen and '#FFFFFFFF' or '#FF8C8C9C'})
        document:set('name_' .. other.id, {color = chosen and 'focus' or 'onAccent'})
    end
end

-- Leaves once, so a second click during the fade does not queue another screen.
function classSelect:leave(next, transition)
    if not self.leaving then
        self.leaving = true
        scene.replace(next, transition)
    end
end

function classSelect:back()
    self:leave(require('scenes.menu').new(self.backdrop))
end

function classSelect:start()
    preferences.set('class', self.class.id)
    preferences.save()
    self:leave(require('scenes.gameplay').new(self.class), {duration = 1, color = '#FF1B1E2B', loading = loadingView.new(self.class, self.backdrop), minimumLoadingTime = 1})
end

function classSelect:update(dt)
    self.backdrop:update(dt)
end

function classSelect:render()
    self.backdrop:draw()
end

return classSelect
