-- The status of the content and its update events. A release with a content server checks its channel, downloads newer content in the background and plays it from the next start, and when nothing can download, offline or with a server that fails, it keeps playing the content on the device, which the events and "content.error" explain. A release whose downloaded content was damaged falls back to earlier content and tells the app with "contentRecovered". In development the app plays its folder, which has no updates, so a check raises.
local content = require('haylen.content')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Updates = haylen.class('Updates', Test)

Updates.code = [[
events.on('contentUpdateReady', function(event) toast('New content plays from the next start.') end, {owner = self})
local ok, promise = pcall(content.check)  -- Raises in development, which has no updates.
if ok then local found = promise:await() end
print(content.status(), content.generation(), content.stats().downloadedBytes)
local failure = content.error()  -- Why nothing downloaded, such as offline.]]

Updates.events = {'contentUpdateAvailable', 'contentUpdateReady', 'contentUpdateFailed', 'contentRecovered'}
Updates.tones = {idle = 'neutral', checking = 'warning', downloading = 'warning', ready = 'success'}
Updates.limit = 30

-- A card with a caption over a value, whose label takes the id `id`.
local function valueCard(id, caption)
    return ui.card{grow = 1, minWidth = 200, gap = 6, align = 'stretch',
        ui.label{text = caption, font = 'caption', color = 'textMuted'},
        ui.label{id = id, text = '', font = 'heading'},
    }
end

function Updates:enter()
    self.items = {}
    self.shown = {}
    self.count = 0
    self:frame{
        stage = false,
        code = Updates.code,
        content = {ui.panel{grow = 1, gap = 16,
            ui.row{gap = 16, alignItems = 'center',
                ui.label{text = 'The content of the app', font = 'heading', grow = 1},
                ui.badge{id = 'contentStatus', text = '', tone = 'neutral', solid = true},
            },
            ui.label{id = 'state', text = '', color = 'textMuted'},
            ui.row{wrap = true, gap = 16, alignItems = 'stretch',
                valueCard('generation', 'Generation'),
                valueCard('downloaded', 'Downloaded'),
                valueCard('failure', 'Last error'),
            },
            ui.button{id = 'check', text = 'Check for an update', onClick = function()
                self:check()
            end},
            ui.sectionTitle{text = 'Events'},
            ui.scroll{grow = 1, fill = true, focusable = false,
                ui.column{grow = 1, align = 'stretch',
                    ui.emptyState{id = 'quiet', title = 'No event yet', message = 'Check for an update, or let a release download its content, and every event shows here.', grow = 1},
                    ui.list{id = 'events', items = {}, visible = false},
                },
            },
        }},
        focus = 'check',
    }
    for _, name in ipairs(Updates.events) do
        self:listen(name, function(event)
            self:note(string.format('The event "%s" arrived', name), tostring(event and (event.code or event.reason or event.generation) or 'Without a payload'))
        end)
    end
end

-- Shows the status, the counts and the last error once they change.
function Updates:update(dt)
    Updates.super.update(self, dt)
    local failure = content.error()
    local stats = content.stats()
    local status = content.status()
    local values = {
        contentStatus = (status:gsub('^%l', string.upper)),
        state = content.isProtected() and 'The app plays a protected release, which checks its channel for newer content.' or 'The app plays its folder, as in development, which has no updates.',
        generation = tostring(content.generation()),
        downloaded = string.format('%d bytes in %d requests', stats.downloadedBytes, stats.requests),
        failure = failure and failure.code or 'None',
    }
    for id, text in pairs(values) do
        if self.shown[id] ~= text then
            self.shown[id] = text
            self:set(id, id == 'contentStatus' and {text = text, tone = Updates.tones[status] or 'danger'} or {text = text})
        end
    end
end

-- Asks the channel for newer content, which raises in development and in a release without a server, and reports what the check found.
function Updates:check()
    local ok, promise = pcall(content.check)
    if not ok then
        self:note('The check raised', tostring(promise))
        return
    end
    self:spawn(function()
        local update, err = promise:await()
        if err then
            self:note('The check failed, and the app keeps playing its content', tostring(err))
        elseif update then
            self:note(string.format('Generation %d is available', update.generation), string.format('%d bytes to download', update.downloadBytes))
        else
            self:note('The app plays the newest content')
        end
    end)
end

function Updates:note(text, detail)
    self.count = self.count + 1
    table.insert(self.items, 1, {id = 'event-' .. self.count, text = text, caption = os.date('%H:%M:%S') .. (detail and '  ' .. detail or '')})
    self.items[Updates.limit + 1] = nil
    self:set('quiet', {visible = false})
    self:set('events', {items = self.items, visible = true})
    self:log('%s.', text)
end

return Updates
