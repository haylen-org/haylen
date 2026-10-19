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

function Updates:enter()
    self.lines = {}
    self:frame{
        stage = false,
        code = Updates.code,
        content = {ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'The content of the app'},
            ui.label{id = 'state', text = ''},
            ui.button{id = 'check', text = 'Check for an update', onClick = function()
                self:check()
            end},
            ui.sectionTitle{text = 'Events'},
            ui.scroll{grow = 1, ui.label{id = 'events', text = 'No content event arrived yet.', font = 'monospace'}},
        }},
        focus = 'check',
    }
    for _, name in ipairs(Updates.events) do
        self:listen(name, function(event)
            self:note(string.format('The event "%s" arrived with %s.', name, tostring(event and (event.code or event.reason or event.generation) or 'nothing')))
        end)
    end
end

function Updates:update(dt)
    Updates.super.update(self, dt)
    local failure = content.error()
    local stats = content.stats()
    self:status(string.format('Status "%s", generation %d, %d bytes downloaded in %d requests%s.', content.status(), content.generation(), stats.downloadedBytes, stats.requests, failure and (', last error "' .. failure.code .. '"') or ''))
    self:set('state', {text = content.isProtected() and 'The app plays a protected release.' or 'The app plays its folder, as in development, which has no updates.'})
end

-- Asks the channel for newer content, which raises in development and in a release without a server, and reports what the check found.
function Updates:check()
    local ok, promise = pcall(content.check)
    if not ok then
        self:note('The check raised: ' .. tostring(promise))
        return
    end
    self:spawn(function()
        local update, err = promise:await()
        if err then
            self:note('The check failed, and the app keeps playing its content: ' .. tostring(err))
        elseif update then
            self:note(string.format('Generation %d is available with %d bytes to download.', update.generation, update.downloadBytes))
        else
            self:note('The app plays the newest content.')
        end
    end)
end

function Updates:note(text)
    self.lines[#self.lines + 1] = text
    self:set('events', {text = table.concat(self.lines, '\n')})
    self:log('%s', text)
end

return Updates
