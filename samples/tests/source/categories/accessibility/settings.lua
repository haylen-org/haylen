-- The function `accessibility.settings` reads the accessibility settings of the system and `accessibilitySettingsChanged` announces every change. The UI scales its text and switches its theme by itself, and the two dots slide with tweens that keep their motion and skip it under reduced motion.
local accessibility = require('haylen.accessibility')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Settings = haylen.class('Settings', Test)

local function describe(settings)
    return string.format('Text scale %.2f\nReduced motion %s\nHigh contrast %s\nReduced transparency %s\nScreen reader %s', settings.textScale, tostring(settings.reducedMotion), tostring(settings.highContrast), tostring(settings.reducedTransparency), tostring(settings.screenReader))
end

function Settings:init(entry)
    Settings.super.init(self, entry)
    self.kept = {x = 0}
    self.skipped = {x = 0}
    self.changes = 0
end

function Settings:enter()
    self:frame{
        hint = 'Change the text size, the motion, the contrast or the transparency in the accessibility settings of the system while the test shows.',
        controls = {
            ui.sectionTitle{text = 'Settings'},
            ui.label{id = 'settings', text = describe(accessibility.settings()), font = 'monospace'},
            ui.sectionTitle{text = 'The UI'},
            ui.label{id = 'theme', text = '', color = 'textMuted'},
            ui.button{text = 'A button whose text scales'},
            ui.sectionTitle{text = 'Events'},
            ui.label{id = 'events', text = 'No change yet.', color = 'textMuted'},
        },
    }
    self:listen('accessibilitySettingsChanged', function(settings)
        self.changes = self.changes + 1
        self:set('settings', {text = describe(settings)})
        self:set('events', {text = string.format('%d changes, the last at %.1f seconds.', self.changes, haylen.elapsed())})
    end)
    self:slide()
end

-- The upper dot keeps its motion and the lower one skips it while the system asks for reduced motion.
function Settings:slide()
    self.kept.x, self.skipped.x = 0, 0
    tween.to(self.kept, 1.5, {x = 1}, {ease = 'sineInOut', owner = self, onComplete = function() self:slide() end})
    tween.to(self.skipped, 1.5, {x = 1}, {ease = 'sineInOut', owner = self, reducedMotion = 'skip'})
end

function Settings:update(dt)
    Settings.super.update(self, dt)
    local scale, system = ui.textScale()
    self:status(string.format('The UI draws the theme "%s" with text at %.2f times its size%s.', ui.theme(), scale, system and ', as the system asks' or ''))
    self:set('theme', {text = string.format('Text color %s, panel color %s.', ui.themeColor('text'):toHex(), ui.themeColor('panel'):toHex())})
end

function Settings:draw(area)
    local margin = 60
    for index, dot in ipairs({self.kept, self.skipped}) do
        local y = area.height * index / 3
        graphics2d.drawLine(margin, y, area.width - margin, y, 4, Test.line)
        graphics2d.drawCircle(margin + (area.width - margin * 2) * dot.x, y, 24, index == 1 and Test.accent or Test.warm)
        Test.caption(index == 1 and 'Keeps its motion' or 'Skips its motion under reduced motion', margin, y - 60)
    end
end

return Settings
