-- The current language follows the languages the person prefers on the device until the app picks one, and `localization.followSystem` lets it follow them again. The event "systemLanguagesChanged" tells of a change of the list of the device while the app runs, and "languageChanged" of every change of the current language, whatever made it, at the start of the next frame.
local events = require('haylen.events')
local haylen = require('haylen')
local localization = require('haylen.localization')
local system = require('haylen.system')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')

local SystemLanguage = haylen.class('SystemLanguage', LanguageTest)

SystemLanguage.code = [[
-- The app.json of the project holds "localization": {"folder": "localization/locale", "fallback": "en"}.
localization.followSystem()  -- The first language of system.languages() with a match.
localization.setLanguage('pt_BR')  -- The choice of the app, until followSystem again.
events.on('languageChanged', function(change) print(change.previous, change.language) end, {owner = self})
events.on('systemLanguagesChanged', function(change) print(change.languages[1]) end, {owner = self})]]

SystemLanguage.hint = 'Pick a language above to choose it, or follow the system again. Change the language of the device while the test shows to see the events.'
SystemLanguage.focus = 'follow'
SystemLanguage.maxEvents = 8

function SystemLanguage:init(entry)
    SystemLanguage.super.init(self, entry)
    self.heard = {}
    self.heardCount = 0
end

function SystemLanguage:content()
    return {
        ui.panel{width = 640, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The choice'},
            ui.label{id = 'mode', text = ''},
            ui.label{id = 'device', text = '', color = 'textMuted'},
            ui.label{id = 'fallback', text = '', color = 'textMuted'},
            ui.button{id = 'follow', text = 'Follow the system', variant = 'primary', align = 'stretch', onClick = function()
                localization.followSystem()
                self:describe()
            end},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Events'},
            ui.scroll{grow = 1, height = 0, ui.list{id = 'events', items = {}}},
        },
    }
end

function SystemLanguage:enter()
    SystemLanguage.super.enter(self)
    localization.followSystem()
    events.on('languageChanged', function(change)
        self:hear(string.format('Language changed from "%s" to "%s".', change.previous, change.language))
    end, {owner = self})
    events.on('systemLanguagesChanged', function(change)
        self:hear(string.format('The device now prefers "%s".', table.concat(change.languages, '", "')))
        self:describe()
    end, {owner = self})
    self:describe()
end

function SystemLanguage:languageChanged()
    self:describe()
end

-- Shows the newest event first and keeps the last few.
function SystemLanguage:hear(line)
    self.heardCount = self.heardCount + 1
    table.insert(self.heard, 1, {id = 'event-' .. self.heardCount, text = line})
    self.heard[SystemLanguage.maxEvents + 1] = nil
    self:set('events', {items = self.heard})
end

-- Tells whether the language follows the device or the choice of the app, and what the device prefers.
function SystemLanguage:describe()
    local current = localization.language()
    local mode = localization.followingSystem() and 'The language follows the system and is "%s".' or 'The app chose the language, which is "%s".'
    self:set('mode', {text = string.format(mode, current)})
    local languages = system.languages()
    local device = #languages > 0 and string.format('The device prefers "%s".', table.concat(languages, '", "')) or 'The platform does not report the languages of the device, so the fallback language shows.'
    self:set('device', {text = device})
    self:set('fallback', {text = string.format('The fallback language is "%s", and the app has "%s".', localization.fallback(), table.concat(localization.languages(), '", "'))})
end

return SystemLanguage
