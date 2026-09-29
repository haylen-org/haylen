-- Download progress: http.client.stream hands the body over chunk by chunk as it arrives, after onResponse has given the status and the headers, whose content-length tells how much is coming. The finished file goes to the user folder with Varn fs. The browser build of Varn cannot stream, so there the stream fails with its reason and the plain download still works.
local fs = require('fs')
local haylen = require('haylen')
local http = require('http')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local sample = require('sample')
local services = require('services')

local Download = haylen.class('Download', sample.Test)

Download.hints = 'Pick what to download, then stream it with progress or fetch it in one piece. The file is saved in the downloads folder of the user folder.'
Download.focus = 'files'

function Download:init(entry)
    Download.super.init(self, entry)
    self.choice = services.downloads[1]
    self.received = 0
    self.expected = nil
    self.running = false
end

function Download:content()
    local items = {}
    for index, choice in ipairs(services.downloads) do
        items[index] = {id = choice.id, text = choice.text, caption = choice.caption}
    end
    return {
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
            ui.row{gap = 16, ui.busyIndicator{id = 'busy', size = 40, visible = false}, ui.label{id = 'status', text = '', grow = 1}},
            ui.label{id = 'result', text = '', color = 'textMuted'},
        },
    }
end

-- The chunks arrive through callbacks of Varn, which only count bytes, and the page reads the count once per frame.
function Download:update(dt)
    if not self.running then
        return
    end
    local fraction = self.expected and self.received / self.expected or 0
    local total = self.expected and ' of ' .. sample.bytes(self.expected) or ', size unknown'
    self:show('progress', {value = math.min(1, fraction), text = sample.bytes(self.received) .. total})
end

function Download:begin(how)
    self.running = true
    self.received = 0
    self.expected = nil
    self.started = sample.millis()
    self:show('url', {text = how .. ' ' .. self.choice.url})
    self:show('busy', {visible = true})
    self:show('status', {text = 'Connecting', color = 'text'})
    self:show('result', {text = ''})
end

-- Stops the progress and shows the outcome, saving the data when there is some.
function Download:finish(data, failure)
    self.running = false
    self:show('busy', {visible = false})
    if data == nil then
        self:show('status', {text = 'The download failed: ' .. tostring(failure), color = 'dangerText'})
        return
    end
    local seconds = math.max(0.001, (sample.millis() - self.started) / 1000)
    self:show('progress', {value = 1, text = sample.bytes(#data)})
    self:show('status', {text = string.format('Received %s in %.1f s, %s per second', sample.bytes(#data), seconds, sample.bytes(#data / seconds)), color = 'successText'})
    local folder = storage.root() .. '/downloads'
    fs.mkdir(folder):await()
    local saved, problem = fs.writeFile(folder .. '/' .. self.choice.file, data):await()
    self:show('result', {text = saved and 'Saved as ' .. folder .. '/' .. self.choice.file or 'Saving failed: ' .. tostring(problem)})
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
