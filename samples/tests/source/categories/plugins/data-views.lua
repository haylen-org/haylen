-- Large data crosses the bridge without a copy as data views of Varn: the JSON carries a token with the pointer and the size of the memory, and every language holds a reference to the same bytes. The native part fills camera frames in views that the app uploads into a texture in place, and reads and marks a view that the app made, which the app then reads back from the same memory.
local dataview = require('dataview')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local DataViews = haylen.class('DataViews', DemoTest)

DataViews.code = [[
local shot = demo.captureFrame(320, 180):await()
texture:update(shot.frame)
local view = dataview.new(4096)
local answer = demo.checksumView(view):await()
print(view:read(0, 6)) -- NATIVE]]

DataViews.width = 320
DataViews.height = 180
DataViews.framesToPass = 30
DataViews.viewSize = 4096
DataViews.mark = 'NATIVE'
DataViews.picture = '@native-frames'
DataViews.framesName = 'Frames arrive in data views that update a texture in place'
DataViews.sharedName = 'The native part reads and writes a view of the app'

function DataViews:enter()
    self.texture = graphics.newTexture(DataViews.width, DataViews.height, {dynamic = true, filter = 'linear'})
    ui.setImage(DataViews.picture, self.texture)
    self:frame{
        hint = 'The frames start when the test opens. Run the check of a shared view again to see that it holds.',
        focus = 'shared',
        controls = {
            ui.button{id = 'shared', text = 'Share a view again', variant = 'primary', onClick = function() self:act(function() self:share() end) end},
            ui.label{text = 'A view is memory with a reference in each language. The JSON names it with a token, so the bytes never cross as text or as a copy. Swift and Objective-C hold "VarnDataView", Kotlin and Java "com.varn.VarnDataView", JavaScript "context.VarnDataView" and C the "varn_dataview" of "HaylenNativeApi".', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12,
                ui.label{text = 'Frames in data views', font = 'heading'},
                ui.image{image = DataViews.picture, radius = 16, grow = 1, align = 'stretch'},
                ui.label{id = 'framesInfo', text = 'The frames show here once the first one arrives.', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
            ui.card{align = 'stretch', gap = 8,
                ui.label{text = 'A view of the app', font = 'heading'},
                ui.row{gap = 16, ui.label{text = 'Before', width = 120, color = 'textMuted'}, ui.label{id = 'before', text = 'Nothing yet', font = 'monospace', grow = 1}},
                ui.row{gap = 16, ui.label{text = 'After', width = 120, color = 'textMuted'}, ui.label{id = 'after', text = 'Nothing yet', font = 'monospace', grow = 1}},
            },
        },
    }
    self:act(function()
        self:share()
        self:stream()
    end)
end

function DataViews:exit()
    ui.removeImage(DataViews.picture)
    DataViews.super.exit(self)
end

-- Asks for one frame after the other for as long as the test runs, each a new view that the texture reads in place.
function DataViews:stream()
    self.results:set('frames', 'waiting', DataViews.framesName, 'Waiting for the first frame.')
    local started = haylen.elapsed()
    local count = 0
    while true do
        local shot, err = demo.captureFrame(DataViews.width, DataViews.height):await()
        if err then
            self.results:failure('frames', DataViews.framesName, err)
            return
        end
        if not dataview.is(shot.frame) or shot.frame.size ~= DataViews.width * DataViews.height * 4 then
            self.results:set('frames', 'fail', DataViews.framesName, string.format('The frame of %s is a %s instead of a view of %d bytes.', shot.language, type(shot.frame), DataViews.width * DataViews.height * 4))
            return
        end

        self.texture:update(shot.frame)
        shot.frame:release()
        count = count + 1
        local seconds = haylen.elapsed() - started
        self:set('framesInfo', {text = string.format('Frame %d from %s, %d by %d pixels in a view of %d bytes, %.0f frames per second.', shot.index, shot.language, shot.width, shot.height, shot.width * shot.height * 4, count / math.max(seconds, 0.001))})
        if count == DataViews.framesToPass then
            self.results:set('frames', 'pass', DataViews.framesName, string.format('%s filled %d frames in views of %d bytes, and "texture:update" uploaded each one from its memory.', shot.language, count, shot.width * shot.height * 4))
        end
    end
end

-- Fills a view in Lua, which the native part sums and marks in place, so the mark reaches Lua through the same memory and not through the answer.
-- The bytes in hexadecimal, such as "4E 41 54".
function DataViews.hex(bytes)
    local parts = {}
    for index = 1, #bytes do
        parts[index] = string.format('%02X', bytes:byte(index))
    end
    return table.concat(parts, ' ')
end

function DataViews:share()
    self.results:set('shared', 'waiting', DataViews.sharedName, string.format('Sharing a view of %d bytes.', DataViews.viewSize))
    local view = dataview.new(DataViews.viewSize)
    local bytes = {}
    local checksum = 0
    for index = 0, DataViews.viewSize - 1 do
        local value = index % 251
        bytes[#bytes + 1] = string.char(value)
        checksum = checksum + value
    end
    view:write(0, table.concat(bytes))
    self:set('before', {text = DataViews.hex(view:read(0, #DataViews.mark))})

    local answer, err = demo.checksumView(view):await()
    if err then
        self.results:failure('shared', DataViews.sharedName, err)
        return
    end
    local marked = view:read(0, #DataViews.mark)
    self:set('after', {text = string.format('%s, the text "%s"', DataViews.hex(marked), marked)})
    local valid = answer.checksum == checksum and answer.size == DataViews.viewSize and marked == DataViews.mark
    self.results:set('shared', valid and 'pass' or 'fail', DataViews.sharedName, string.format('%s summed %d bytes to %d, which Lua sums to %d, and the view of Lua now starts with "%s".', answer.language, answer.size, answer.checksum, checksum, marked))
    view:release()
end

return DataViews
