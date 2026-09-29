-- The settings of the sample: their defaults, the way stored preferences reach the engine at startup and the way back to the defaults.
local audio = require('haylen.audio')
local input = require('haylen.input')
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local window = require('haylen.window')

local settings = {}

settings.actions = 'input/actions.json'
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

-- Returns a game setting, or its default when the player never changed it.
function settings.get(key)
    return preferences.get(key, settings.game[key])
end

-- Brings the stored choices back at startup: the engine settings through preferences.apply, over the default action map, and the language of the game. The first launch stores the defaults.
function settings.start()
    input.loadActions(settings.actions)
    if not preferences.has('game') then
        settings.reset()
        return
    end
    preferences.apply()
    localization.setLanguage(settings.get('game.language'))
end

-- Puts every setting back to its default, in the engine and in the preferences, and saves them.
function settings.reset()
    preferences.clear()
    for bus, volume in pairs(settings.volumes) do
        audio.setBusVolume(bus, volume)
        audio.setBusMuted(bus, false)
    end
    window.setFullscreen(false)
    input.loadActions(settings.actions)
    preferences.capture()
    for key, value in pairs(settings.game) do
        preferences.set(key, value)
    end
    localization.setLanguage(settings.game['game.language'])
    preferences.save()
end

return settings
