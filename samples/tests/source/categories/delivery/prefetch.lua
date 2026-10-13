-- The rule "delivery/prefetched/**" of "app.json" prefetches the tracks of the folder: a release downloads them in the background once the app runs, before anything asks for them. "content.prefetch" brings assets onto the device on request without decoding them, so they cost storage and no memory, and its progress function receives the bytes as they arrive.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Prefetch = haylen.class('Prefetch', Test)

Prefetch.paths = {'delivery/prefetched/track-1.json', 'delivery/prefetched/track-2.json', 'delivery/prefetched/track-3.json'}

function Prefetch:enter()
    self:frame{
        stage = false,
        content = {ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Tracks that a release prefetches'},
            ui.label{id = 'before', text = ''},
            ui.progress{id = 'progress', value = 0, text = 'Waiting'},
            ui.label{id = 'result', text = ''},
            ui.button{id = 'again', text = 'Prefetch again', onClick = function()
                self:prefetch()
            end},
            ui.card{ui.label{text = "content.prefetch(paths, function(progress)\n    print(progress.receivedBytes, progress.totalBytes)\nend):await()", font = 'monospace'}},
        }},
        focus = 'again',
    }
    self:prefetch()
end

-- Prefetches the tracks with a progress bar, then reads one of them, which no longer waits.
function Prefetch:prefetch()
    local held = 0
    for _, path in ipairs(Prefetch.paths) do
        held = held + (content.isLocal(path) and 1 or 0)
    end
    self:set('before', {text = string.format('Before the prefetch, %d of the %d tracks are on the device.', held, #Prefetch.paths)})
    self:spawn(function()
        local done, err = content.prefetch(Prefetch.paths, function(progress)
            local share = progress.totalBytes > 0 and progress.receivedBytes / progress.totalBytes or 1
            self:set('progress', {value = share, text = string.format('%d of %d bytes', progress.receivedBytes, progress.totalBytes)})
        end, {owner = self}):await()
        if not done then
            self:set('result', {text = 'The prefetch failed: ' .. tostring(err), color = 'dangerText'})
            return
        end
        self:set('progress', {value = 1, text = 'Every track is on the device'})
        local track = assets.json(Prefetch.paths[1])
        self:set('result', {text = string.format('The track "%s" with %d notes now loads synchronously.', track.name, #track.notes), color = 'successText'})
    end)
end

return Prefetch
