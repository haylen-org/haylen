-- The player's preferences and best run. Preferences live in haylen.preferences and apply to the audio buses, the language and the window.
local audio = require('haylen.audio')
local localization = require('haylen.localization')
local haylen = require('haylen')
local storage = require('haylen.storage')
local stored = require('haylen.preferences')
local window = require('haylen.window')

local preferences = {}

local desktop = {macos = true, windows = true, linux = true}

function preferences.desktop()
    return desktop[haylen.platform] == true
end

function preferences.get(key)
    local defaults = {music = 0.7, effects = 0.9, language = localization.language(), fullscreen = false, touch = true, class = 'warrior'}
    return stored.get(key, defaults[key])
end

-- Changes a preference and applies it at once. Sliders change preferences every frame, so writing them to storage waits for save.
function preferences.set(key, value)
    stored.set(key, value)
    preferences.apply()
end

function preferences.save()
    stored.save()
end

-- The sample ships Google sign-in plugins for Android and the web.
function preferences.googleSignIn()
    return haylen.platform == 'android' or haylen.platform == 'web'
end

function preferences.apply()
    audio.setBusVolume('music', preferences.get('music'))
    local effects = preferences.get('effects')
    audio.setBusVolume('sfx', effects)
    audio.setBusVolume('ui', effects)
    audio.setBusVolume('ambience', effects)
    localization.setLanguage(preferences.get('language'))
    if preferences.desktop() and window.fullscreen() ~= preferences.get('fullscreen') then
        window.setFullscreen(preferences.get('fullscreen'))
    end
end

function preferences.best()
    if not storage.slotExists('record') then
        return nil
    end
    return storage.readSlot('record')
end

-- Keeps the run when it beats the best one and returns whether it did.
function preferences.record(days, kills, class)
    local best = preferences.best()
    if best and best.days >= days then
        return false
    end
    storage.writeSlot('record', {days = days, kills = kills, class = class}, {days = days})
    return true
end

return preferences
