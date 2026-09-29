-- What every test of the sample shares: the page with the Back button, the title, the description, the language picker and the hint line, and the way to and from the menu.
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local language = require('language')

local sample = {}

-- The fade that every change between the menu and a test plays.
sample.transition = {effect = 'fade', duration = 0.3, color = '#FF101418'}

function sample.open(entry)
    if not scene.transitioning() then
        scene.push(require(entry.module)(entry), sample.transition)
    end
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(sample.transition)
    end
end

-- The base of every test scene. A test sets its hints and the control that takes the focus, returns the nodes of its page from content and hears languageChanged whenever the language changes, from the picker, the keys or anywhere else.
local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

Test.hints = ''
Test.focus = 'language'

function Test:init(entry)
    self.entry = entry
end

function Test:content()
    return {}
end

function Test:languageChanged()
end

-- Mounts the page, which the scene owns, so it goes away when the scene unloads. The Back button, Escape, the east gamepad button and the Menu button of a TV remote return to the menu.
function Test:enter()
    window.setBackLeavesApp(false)
    self.language = localization.language()
    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 16,
        onCancel = sample.back,
        ui.row{gap = 24,
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.entry.title, font = 'heading'},
                ui.label{text = self.entry.description, color = 'textMuted'},
            },
            ui.segmentedControl{id = 'language', width = 1080, align = 'start', items = language.items(), selected = self.language, onChange = function(event)
                language.use(event.value)
            end},
        },
        ui.row{grow = 1, gap = 24, children = self:content()},
        ui.label{text = self.hints .. ' L or the right shoulder picks the next language, K or the left shoulder the previous one.', font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.focus, 'focus')
    self:languageChanged()
end

function Test:exit()
    window.setBackLeavesApp(true)
end

-- Follows the language keys, which read as up while a text field takes the keyboard, and tells the test when the language changed.
function Test:update(dt)
    if input.pressed('nextLanguage') then
        language.step(1)
    elseif input.pressed('previousLanguage') then
        language.step(-1)
    end
    if localization.language() ~= self.language then
        self.language = localization.language()
        self:show('language', {selected = self.language})
        self:languageChanged()
    end
end

function Test:show(id, properties)
    self.document:set(id, properties)
end

return sample
