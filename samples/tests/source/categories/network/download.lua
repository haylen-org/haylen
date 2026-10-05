-- The function `http.client.stream` hands the body over chunk by chunk as it arrives, after `onResponse` has given the status and the headers, whose `content-length` tells how much is coming. The finished file goes to the user folder with Varn `fs`. The browser build of Varn cannot stream, so there the stream fails with its reason and the plain download still works.
local fs = require('fs')
local haylen = require('haylen')
local http = require('http')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Test = require('harness.test')
local readout = require('categories.network.readout')
local services = require('categories.network.services')

local Download = haylen.class('Download', Test)

function Download:init(entry)
    Download.super.init(self, entry)
    self.choice = services.downloads[1]
    self.received = 0
    self.expected = nil
    self.running = false
end

function Download:enter()
    local items = {}
    for index, choice in ipairs(services.downloads) do
        items[index] = {id = choice.id, text = choice.text, caption = choice.caption}
    end
    self:frame{
        hint = 'Pick what to download, then stream it with progress or fetch it in one piece. The file is saved in the downloads folder of the user folder.',
        focus = 'files',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 640, align = 'stretch', gap = 12,
                ui.list{id = 'files', items = items, selected = self.choice.id, onSelect = function(event)
                    for _, choice in ipairs(services.downloads) do
                        if choice.id == event.item then
                            self.choice = choice
                        end
                    end
                end},
                ui.button{id = 'stream', text = 'Stream with progress', variant = 'primary', align = 'stretch', onClick = function()
                    self:stream()
                end},
                ui.button{id = 'fetch', text = 'Fetch in one piece', align = 'stretch', onClick = function()
                    self:fetch()
                end},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 16,
                ui.label{id = 'url', text = '', font = 'monospace', color = 'accentText'},
                ui.progress{id = 'progress', value = 0, text = 'Nothing downloaded yet'},
                ui.row{gap = 16, ui.busyIndicator{id = 'busy', size = 40, visible = false}, ui.label{id = 'outcome', text = '', grow = 1}},
                ui.label{id = 'result', text = '', color = 'textMuted'},
            },
        }},
    }
end

-- The chunks arrive through callbacks of Varn, which only count bytes, and the page reads the count once per frame.
function Download:update(dt)
    Download.super.update(self, dt)
    if not self.running then
        return
    end
    local fraction = self.expected and self.received / self.expected or 0
    local total = self.expected and ' of ' .. readout.bytes(self.expected) or ', size unknown'
    self:set('progress', {value = math.min(1, fraction), text = readout.bytes(self.received) .. total})
end

function Download:begin(how)
    self.running = true
    self.received = 0
    self.expected = nil
    self.started = readout.millis()
    self:set('url', {text = how .. ' ' .. self.choice.url})
    self:set('busy', {visible = true})
    self:set('outcome', {text = 'Connecting', color = 'text'})
    self:set('result', {text = ''})
end

-- Stops the progress and shows the outcome, saving the data when there is some.
function Download:finish(data, failure)
    self.running = false
    self:set('busy', {visible = false})
    if data == nil then
        self:set('outcome', {text = 'The download failed: ' .. tostring(failure), color = 'dangerText'})
        return
    end
    local seconds = math.max(0.001, (readout.millis() - self.started) / 1000)
    self:set('progress', {value = 1, text = readout.bytes(#data)})
    self:set('outcome', {text = string.format('Received %s in %.1f s, %s per second.', readout.bytes(#data), seconds, readout.bytes(#data / seconds)), color = 'successText'})
    local folder = storage.root() .. '/downloads'
    fs.mkdir(folder):await()
    local saved, problem = fs.writeFile(folder .. '/' .. self.choice.file, data):await()
    self:set('result', {text = saved and 'Saved as "' .. folder .. '/' .. self.choice.file .. '".' or 'Saving failed: ' .. tostring(problem)})
end

function Download:stream()
    if self.running then
        return
    end
    self:begin('Streaming')
    local chunks = {}
    self:spawn(function()
        local done, failure = http.client.stream({url = self.choice.url, timeoutSeconds = 30, onResponse = function(status, headers)
            self.answer = status
            self.expected = tonumber(headers['content-length'])
        end}, function(chunk)
            chunks[#chunks + 1] = chunk
            self.received = self.received + #chunk
        end):await()
        if done == nil then
            self:finish(nil, failure)
        elseif self.answer and self.answer >= 400 then
            self:finish(nil, 'the server answered ' .. self.answer)
        else
            self:finish(table.concat(chunks))
        end
    end)
end

function Download:fetch()
    if self.running then
        return
    end
    self:begin('Fetching')
    self:spawn(function()
        local response, failure = http.client.get(self.choice.url, {timeoutSeconds = 30}):await()
        if response == nil then
            self:finish(nil, failure)
        elseif not response.ok then
            self:finish(nil, 'the server answered ' .. response.status)
        else
            self.received = #response.body
            self:finish(response.body)
        end
    end)
end

return Download
