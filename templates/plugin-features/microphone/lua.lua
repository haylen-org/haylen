-- Starts the microphone, whose samples reach the audio stream of `microphoneStream()`. Returns a platform call, whose `await` gives `{sampleRate, channels}`.
function plugin.startMicrophone()
    return handle:call('startMicrophone', {})
end

function plugin.stopMicrophone()
    return handle:call('stopMicrophone', {})
end

-- Returns the audio stream of the microphone, which plays as a voice with `play` and copies its newest samples with `read`, or `nil` until the microphone started once.
function plugin.microphoneStream()
    return handle:audioStream('microphone')
end
