-- The function `accessibility.settings` reads the accessibility settings of the system and `accessibilitySettingsChanged` announces every change. The UI scales its text and switches its theme by itself, and two icons slide with tweens, one that keeps its motion and one that skips it under reduced motion.
local accessibility = require('haylen.accessibility')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Settings = haylen.class('Settings', Test)

Settings.code = [[
local settings = accessibility.settings()
print(settings.textScale, settings.reducedMotion, settings.highContrast, settings.reducedTransparency, settings.screenReader)
events.on('accessibilitySettingsChanged', function(settings) print(settings.textScale) end, {owner = self})
tween.to(dot, 1.5, {x = 1}, {ease = 'sineInOut', owner = self, reducedMotion = 'skip'})  -- Jumps to the end under reduced motion.]]

Settings.switches = {
    {key = 'reducedMotion', text = 'Reduced motion'},
    {key = 'highContrast', text = 'High contrast'},
    {key = 'reducedTransparency', text = 'Reduced transparency'},
    {key = 'screenReader', text = 'Screen reader'},
}
Settings.lanes = {
    {target = 'kept', title = 'Keeps its motion', icon = 'interface/themes/fantasy/icons/star.png', color = '#FF8FB0FF'},
    {target = 'skipped', title = 'Skips its motion under reduced motion', icon = 'interface/themes/fantasy/icons/gem.png', color = '#FFF2B23A'},
}

function Settings:init(entry)
    Settings.super.init(self, entry)
    self.kept = {x = 0}
    self.skipped = {x = 0}
    self.changes = 0
    self.icons = {}
    for index, lane in ipairs(Settings.lanes) do
        self.icons[index] = assets.texture(lane.icon, {filter = 'linear'})
    end
end

-- A row of the panel: the name of a setting and a chip that says whether the system turns it on.
function Settings.switchRow(switch, on)
    return ui.row{gap = 12, ui.label{text = switch.text, grow = 1}, ui.badge{id = 'switch-' .. switch.key, text = on and 'On' or 'Off', tone = on and 'success' or 'neutral', solid = on}}
end

-- A row of the panel: a swatch of a theme color in a rim of the strong border color, so even the color of the panel around it shows, with its name.
function Settings.swatchRow(id, name)
    return ui.row{gap = 12, ui.panel{width = 40, height = 40, padding = 3, align = 'stretch', style = {colors = {panel = 'borderStrong'}}, ui.panel{id = id, grow = 1, padding = 0}}, ui.label{text = name, grow = 1}}
end

function Settings:enter()
    local settings = accessibility.settings()
    local rows = {ui.sectionTitle{text = 'The system asks for'}, ui.row{gap = 12, ui.label{text = 'Text scale', grow = 1}, ui.badge{id = 'textScale', text = '', tone = 'accent', solid = true}}}
    for _, switch in ipairs(Settings.switches) do
        rows[#rows + 1] = Settings.switchRow(switch, settings[switch.key])
    end
    rows[#rows + 1] = ui.sectionTitle{text = 'The UI follows'}
    rows[#rows + 1] = Settings.swatchRow('swatch-text', 'The color of the text')
    rows[#rows + 1] = Settings.swatchRow('swatch-panel', 'The color of the panels')
    rows[#rows + 1] = ui.button{text = 'A button whose text scales', align = 'stretch'}
    rows[#rows + 1] = ui.label{id = 'events', text = 'The system changed nothing yet.', color = 'textMuted'}
    self:frame{
        hint = 'Change the text size, the motion, the contrast or the transparency in the accessibility settings of the system while the test shows.',
        code = Settings.code,
        controls = rows,
    }
    self:show(settings)
    self:listen('accessibilitySettingsChanged', function(changed)
        self.changes = self.changes + 1
        self:show(changed)
        self:set('events', {text = string.format('The system changed a setting %d %s.', self.changes, self.changes == 1 and 'time' or 'times')})
    end)
    self:slide()
end

function Settings:show(settings)
    self:set('textScale', {text = string.format('%.2fx', settings.textScale)})
    for _, switch in ipairs(Settings.switches) do
        local on = settings[switch.key]
        self:set('switch-' .. switch.key, {text = on and 'On' or 'Off', tone = on and 'success' or 'neutral', solid = on})
    end
end

-- The upper lane keeps its motion and the lower one skips it while the system asks for reduced motion.
function Settings:slide()
    self.kept.x, self.skipped.x = 0, 0
    tween.to(self.kept, 1.6, {x = 1}, {ease = 'sineInOut', owner = self, onComplete = function() self:slide() end})
    tween.to(self.skipped, 1.6, {x = 1}, {ease = 'sineInOut', owner = self, reducedMotion = 'skip'})
end

function Settings:update(dt)
    Settings.super.update(self, dt)
    local scale, system = ui.textScale()
    self:status(string.format('The UI draws the %s theme with text at %.2f times its size%s.', ui.theme(), scale, system and ', as the system asks' or ''))
    -- The theme follows high contrast by itself, so the swatches follow the theme.
    if ui.theme() ~= self.swatchTheme then
        self.swatchTheme = ui.theme()
        self:set('swatch-text', {style = {colors = {panel = ui.themeColor('text'):toHex()}}})
        self:set('swatch-panel', {style = {colors = {panel = ui.themeColor('panel'):toHex()}}})
    end
end

-- Two lanes of the play area, each a rounded panel with its title and a groove along which an icon travels, leaving a soft trail.
function Settings:draw(area)
    local margin = 48
    local height = math.min(220, (area.height - margin * 3) / 2)
    local top = (area.height - height * 2 - margin) / 2
    local reduced = accessibility.settings().reducedMotion
    for index, lane in ipairs(Settings.lanes) do
        local y = top + (index - 1) * (height + margin)
        local rect = {margin, y, area.width - margin * 2, height}
        graphics2d.drawShape({rect[1], rect[2] + 8, rect[3], rect[4]}, {radius = 24, color = '#50000000', softness = 24, layer = -2})
        graphics2d.drawShape(rect, {radius = 24, color = Test.surface, borderWidth = 2, borderColor = Test.line, layer = -1})
        Test.caption(lane.title, rect[1] + 32, rect[2] + 24, {size = 30, color = Test.ink})
        local skipped = index == 2 and reduced
        local note = skipped and 'Jumps to the end' or 'Slides'
        Test.caption(note, rect[1] + rect[3] - 32, rect[2] + 28, {size = 24, color = skipped and Test.warm or Test.muted, anchor = {1, 0}})

        local grooveY = rect[2] + rect[4] * 0.66
        local left, right = rect[1] + 80, rect[1] + rect[3] - 80
        graphics2d.drawShape({left - 12, grooveY - 9, right - left + 24, 18}, {radius = 9, color = '#FF1B1F2C'})
        local progress = self[lane.target].x
        local x = left + (right - left) * progress
        graphics2d.drawShape({left - 12, grooveY - 9, x - left + 24, 18}, {radius = 9, color = lane.color})
        graphics2d.drawEllipse(x, grooveY + 26, 34, 8, '#40000000')
        local size = 84
        graphics2d.draw(self.icons[index], x, grooveY - size * 0.5 + 6, {width = size, height = size, layer = 2})
    end
end

return Settings
