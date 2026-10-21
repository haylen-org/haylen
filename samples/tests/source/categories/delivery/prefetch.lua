-- The rule "delivery/prefetched/**" of "app.json" prefetches the tracks of the folder: a release downloads them in the background once the app runs, before anything asks for them. "content.prefetch" brings assets onto the device on request without decoding them, so they cost storage and no memory, and its progress function receives the bytes as they arrive. Each track shows where it is, and the bar the bytes of all of them.
local assets = require('haylen.assets')
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Prefetch = haylen.class('Prefetch', Test)

Prefetch.code = [[
-- The file "app.json" holds "content": {"delivery": [{"match": "delivery/prefetched/**", "mode": "prefetch"}]}.
print(content.isLocal('delivery/prefetched/track-1.json'))
local done, err = content.prefetch(paths, function(progress) print(progress.receivedBytes, progress.totalBytes) end, {owner = self}):await()
local track = assets.json('delivery/prefetched/track-1.json')  -- Loads at once, since it is on the device.]]

Prefetch.paths = {'delivery/prefetched/track-1.json', 'delivery/prefetched/track-2.json', 'delivery/prefetched/track-3.json'}

function Prefetch:enter()
    local rows = {}
    for index in ipairs(Prefetch.paths) do
        rows[index] = ui.card{padding = {8, 20}, align = 'stretch',
            ui.row{gap = 16, alignItems = 'center',
                ui.image{image = 'interface/themes/fantasy/icons/star.png', scale = 0.35},
                ui.column{gap = 2, grow = 1,
                    ui.label{id = 'name-' .. index, text = string.format('Track %d', index), font = 'button'},
                    ui.label{id = 'detail-' .. index, text = Prefetch.paths[index], font = 'caption', color = 'textMuted'},
                },
                ui.badge{id = 'state-' .. index, text = 'Waiting', tone = 'neutral'},
            },
        }
    end
    self:frame{
        stage = false,
        code = Prefetch.code,
        content = {ui.panel{grow = 1, gap = 12,
            ui.sectionTitle{text = 'Tracks that a release prefetches'},
            ui.label{id = 'before', text = '', color = 'textMuted'},
            ui.column{gap = 10, align = 'stretch', children = rows},
            ui.row{gap = 12, alignItems = 'center',
                ui.label{text = 'Bytes', font = 'button'},
                ui.label{id = 'bytes', text = 'Waiting for the first bytes', color = 'textMuted', grow = 1},
            },
            ui.progress{id = 'progress', value = 0, tone = 'success'},
            ui.alert{id = 'result', tone = 'information', title = 'Prefetching', message = 'The tracks are on their way.'},
            ui.button{id = 'again', text = 'Prefetch again', onClick = function()
                self:prefetch()
            end},
        }},
        focus = 'again',
    }
    self:prefetch()
end

-- Shows each track as on the device or still to come.
function Prefetch:showTracks()
    for index, path in ipairs(Prefetch.paths) do
        local here = content.isLocal(path)
        self:set('state-' .. index, {text = here and 'On the device' or 'Downloading', tone = here and 'success' or 'warning', solid = here})
        if here then
            local track = assets.json(path)
            self:set('name-' .. index, {text = track.name})
            self:set('detail-' .. index, {text = string.format('%d notes in "%s"', #track.notes, path)})
        end
    end
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
            self:set('progress', {value = share})
            self:set('bytes', {text = string.format('%d of %d bytes', progress.receivedBytes, progress.totalBytes)})
        end, {owner = self}):await()
        if not done then
            self:set('result', {tone = 'danger', title = 'The prefetch failed', message = tostring(err)})
            return
        end
        self:set('progress', {value = 1})
        self:set('bytes', {text = 'Every byte arrived'})
        self:showTracks()
        local track = assets.json(Prefetch.paths[1])
        self:set('result', {tone = 'success', title = 'Every track is on the device', message = string.format('The track "%s" with %d notes now loads synchronously.', track.name, #track.notes)})
    end)
end

return Prefetch
