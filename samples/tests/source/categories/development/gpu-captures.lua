-- GPU captures: the capture tool of the graphics API that draws the app, and buttons that capture the next frames into a file of user storage, whose path, or the reason a platform has no capture, the card beside the panel lists.
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local LogCard = require('categories.development.log-card')

local GpuCaptures = haylen.class('GpuCaptures', Test)

GpuCaptures.code = [[
print(debugging.gpuCaptureTool())  -- 'metalFrameCapture', 'renderDoc', 'pix' or 'none'.
scene.spawn(self, function()
    local capture, message, failure = debugging.captureGpuAsync({frames = 1}):await()
    if capture then print(capture.path, capture.tool, capture.frames)
    else print(failure.code, message) end  -- 'unsupported' says what is missing.
end)]]
-- The name of each capture tool, what it writes and which debugger opens it.
GpuCaptures.tools = {
    metalFrameCapture = {name = 'Metal frame capture', text = 'The frame capture of Metal writes a ".gputrace" document, which Xcode opens.'},
    renderDoc = {name = 'RenderDoc', text = 'RenderDoc launched the app, and writes an ".rdc" file, which RenderDoc opens.'},
    pix = {name = 'PIX', text = 'PIX launched the app for a GPU capture, and writes a ".wpix" file, which PIX opens.'},
    none = {name = 'None', text = 'The graphics API of the app has no capture tool here. A capture says what is missing.'},
}

function GpuCaptures:enter()
    local name = debugging.gpuCaptureTool()
    local tool = GpuCaptures.tools[name]
    self.captures = LogCard(self, 'captures', 'Captures', 'The path of each capture, or the reason a capture failed, shows here.')
    self:frame{
        hint = 'Capture the next frames of the GPU with the capture tool of the graphics API, then open the file in its debugger.',
        code = GpuCaptures.code,
        content = {ui.row{wrap = true, grow = 1, gap = 24, alignItems = 'stretch',
            ui.panel{width = 560, gap = 12,
                ui.row{gap = 12, alignItems = 'center',
                    ui.label{text = 'Capture tool', font = 'heading', grow = 1},
                    ui.badge{id = 'tool', text = tool.name, tone = name == 'none' and 'neutral' or 'success'},
                },
                ui.label{text = tool.text, color = 'textMuted'},
                ui.label{text = string.format('The app draws through "%s".', graphics.apiName()), color = 'textMuted'},
                ui.button{id = 'one', text = 'Capture a frame', align = 'stretch', variant = 'primary', onClick = function() self:capture(1) end},
                ui.button{id = 'three', text = 'Capture 3 frames', align = 'stretch', onClick = function() self:capture(3) end},
            },
            self.captures:node(),
        }},
    }
end

function GpuCaptures:capture(frames)
    self:spawn(function()
        local capture, message, failure = debugging.captureGpuAsync({frames = frames}):await()
        if capture then
            self.captures:add(string.format('Captured %d %s with "%s"', capture.frames, capture.frames == 1 and 'frame' or 'frames', capture.tool), capture.path)
            return
        end
        self.captures:add(string.format('No capture, "%s"', failure.code), message, 'danger')
    end)
end

return GpuCaptures
