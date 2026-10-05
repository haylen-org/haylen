-- The base of the tests of the category: the languages and their font while the test shows, a language picker over the page, the keys that pick the next and the previous language, and the hook `languageChanged`, which runs whenever the language changes, from the picker, the keys or anywhere else. A test sets `hint` and `focus` and returns the nodes of its page from `content`.
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local Test = require('harness.test')
local language = require('categories.localization.language')

local LanguageTest = haylen.class('LanguageTest', Test)

LanguageTest.hint = ''
LanguageTest.focus = 'language'
LanguageTest.keysHint = 'L or the right shoulder picks the next language, and K or the left shoulder the previous one.'
LanguageTest.actions = {actions = {
    {name = 'nextLanguage', type = 'button', bindings = {'key:l', 'button:rightShoulder'}},
    {name = 'previousLanguage', type = 'button', bindings = {'key:k', 'button:leftShoulder'}},
}}

function LanguageTest:content()
    return {}
end

function LanguageTest:languageChanged()
end

function LanguageTest:enter()
    self.previous = language.setup()
    self.language = localization.language()
    self:loadActions(LanguageTest.actions)
    self:frame{
        hint = self.hint .. ' ' .. LanguageTest.keysHint,
        focus = self.focus,
        content = {
            ui.segmentedControl{id = 'language', width = 1080, align = 'start', items = language.items(), selected = self.language, onChange = function(event)
                localization.setLanguage(event.value)
            end},
            ui.row{grow = 1, gap = 24, children = self:content()},
        },
    }
    self:languageChanged()
end

function LanguageTest:exit()
    language.restore(self.previous)
    LanguageTest.super.exit(self)
end

-- Follows the language keys, which read as up while a text field takes the keyboard, and tells the test when the language changed.
function LanguageTest:update(dt)
    LanguageTest.super.update(self, dt)
    if input.pressed('nextLanguage') then
        language.step(1)
    elseif input.pressed('previousLanguage') then
        language.step(-1)
    end
    if localization.language() ~= self.language then
        self.language = localization.language()
        self:set('language', {selected = self.language})
        self:languageChanged()
    end
end

return LanguageTest
