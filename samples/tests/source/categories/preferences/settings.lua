-- The settings the tests of the category keep: their defaults, the groups of preferences they own, the way stored preferences reach the engine the way an app does at startup, and the state of the engine they put back when a test exits, so the other tests of the project never inherit a volume, a mute, fullscreen or a language.
local audio = require('haylen.audio')
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local window = require('haylen.window')

local settings = {}

settings.actions = 'preferences/input/actions.json'
settings.locale = 'preferences/locale'
settings.buses = {'master', 'music', 'sfx', 'ui', 'ambience'}
settings.volumes = {master = 1, music = 0.6, sfx = 0.8, ui = 0.8, ambience = 1}
settings.game = {
    ['game.difficulty'] = 'normal',
    ['game.language'] = 'en',
    ['game.subtitles'] = true,
    ['game.textSize'] = 'medium',
    ['game.playerName'] = 'Player',
    ['controls.invertY'] = false,
}

-- The groups of preferences these tests own. The rest of the file belongs to the project, such as the group `tests` with the last test opened.
settings.groups = {'audio', 'window', 'input', 'game', 'controls', 'settings'}

-- Returns a game setting, or its default when the player never changed it.
function settings.get(key)
    return preferences.get(key, settings.game[key])
end

-- Returns the state of the engine that the tests change, which `restore` puts back.
function settings.snapshot()
    local snapshot = {volumes = {}, muted = {}, fullscreen = window.fullscreen(), language = localization.language()}
    for _, bus in ipairs(settings.buses) do
        snapshot.volumes[bus] = audio.busVolume(bus)
        snapshot.muted[bus] = audio.busMuted(bus)
    end
    return snapshot
end

function settings.restore(snapshot)
    for _, bus in ipairs(settings.buses) do
        audio.setBusVolume(bus, snapshot.volumes[bus])
        audio.setBusMuted(bus, snapshot.muted[bus])
    end
    window.setFullscreen(snapshot.fullscreen)
    if snapshot.language ~= '' then
        localization.setLanguage(snapshot.language)
    end
end

-- Brings the stored choices back the way an app does at startup: the translations, the default action map of the test, the stored engine settings over it and the stored language. The first visit stores the defaults.
function settings.start(test)
    localization.loadFolder(settings.locale)
    test:loadActions(settings.actions)
    if not preferences.has('game') then
        settings.reset(test)
        return
    end
    preferences.apply()
    localization.setLanguage(settings.get('game.language'))
end

-- Puts every setting of these tests back to its default, in the engine and in the preferences, and saves them, while the preferences of the project stay.
function settings.reset(test)
    for _, group in ipairs(settings.groups) do
        preferences.remove(group)
    end
    for bus, volume in pairs(settings.volumes) do
        audio.setBusVolume(bus, volume)
        audio.setBusMuted(bus, false)
    end
    window.setFullscreen(false)
    test:loadActions(settings.actions)
    preferences.capture()
    for key, value in pairs(settings.game) do
        preferences.set(key, value)
    end
    localization.setLanguage(settings.game['game.language'])
    preferences.save()
end

return settings
