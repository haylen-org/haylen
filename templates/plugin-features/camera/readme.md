### plugin.startCamera(options), plugin.stopCamera() and plugin.cameraStream()

Start and stop the camera, whose frames reach the video stream of `cameraStream()`, which returns `nil` until the camera started once. The table `options` takes `facing`, `'back'` (the default) or `'front'`, and `startCamera` gives `{width, height, facing}`. The call asks for the permission `camera` of `system.requestPermission` where the app does not hold it yet, so an app that wants to explain the camera first checks it with `system.permission('camera')`. It fails with the code `denied` when the person refused the camera, with a message that says how to ask again, and with `unsupported` on a device without a camera, on Apple TV before tvOS 17 and on the desktop library.

```lua
local graphics2d = require('haylen.graphics2d')
local plugin = require('{{ID}}')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        scene.spawn(self, function()
            local started, err = plugin.startCamera({facing = 'front'}):await()
            print(started and started.width or err.message)
        end)
    end,
    render = function(self)
        local stream = plugin.cameraStream()
        graphics2d.beginScreen()
        if stream and stream.texture then
            graphics2d.draw(stream.texture, 0, 0)
        end
    end,
})
```

### plugin.takePhoto()

Takes a photo from the newest frame of the camera and gives `{jpeg, width, height}`, with the bytes of a JPEG file in `jpeg`, which `graphics.newTexture(jpeg)` turns into a texture. It fails with the code `cameraOff` while the camera is off.
