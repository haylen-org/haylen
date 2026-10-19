### plugin.startMicrophone(), plugin.stopMicrophone() and plugin.microphoneStream()

Start and stop the microphone, whose mono samples reach the audio stream of `microphoneStream()`, which returns `nil` until the microphone started once. The stream plays as a voice of `haylen.audio` with `stream:play()`, and `stream:read(buffer)` copies its newest samples into a float buffer, such as for a level meter. The call `startMicrophone` gives `{sampleRate, channels}`. The platform asks the person for the microphone the first time, and the call fails with the code `permissionDenied` when the person refuses, and with `unsupported` on a device without a microphone, on Apple TV and on the desktop library.

```lua
local collections = require('haylen.collections')
local plugin = require('{{ID}}')
local scene = require('haylen.scene')

local samples = collections.newFloatBuffer(1024)

scene.push({
    enter = function(self)
        scene.spawn(self, function()
            plugin.startMicrophone():await()
            self.stream = plugin.microphoneStream()
        end)
    end,
    update = function(self, dt)
        if self.stream then
            local peak = 0
            for index = 1, self.stream:read(samples) do
                peak = math.max(peak, math.abs(samples[index]))
            end
            self.level = peak
        end
    end,
})
```
