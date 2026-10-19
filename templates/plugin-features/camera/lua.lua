-- Starts the camera, whose frames reach the video stream of `cameraStream()`. The table `options` takes `facing`, `'back'` or `'front'`. Returns a platform call, whose `await` gives `{width, height, facing}`.
function plugin.startCamera(options)
    return handle:call('startCamera', {facing = options and options.facing or 'back'})
end

function plugin.stopCamera()
    return handle:call('stopCamera', {})
end

-- Takes a photo from the newest frame of the camera. Returns a platform call, whose `await` gives `{jpeg, width, height}` with the bytes of a JPEG file in `jpeg`.
function plugin.takePhoto()
    return handle:call('takePhoto', {})
end

-- Returns the video stream of the camera, whose `texture` draws the newest frame, or `nil` until the camera started once.
function plugin.cameraStream()
    return handle:videoStream('camera')
end
