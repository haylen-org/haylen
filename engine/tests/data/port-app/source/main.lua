-- The app of the test port: it checks that the platform, the operating system and the graphics API are the ones of the port, that the port refuses to start programs and to make callbacks of Lua functions, which write code into memory, that Varn takes its external driver of processes, checks that its jobs run on the worker threads of the pumped event loop of the port, draws frames of shapes, sprites and text through the device of the port, which runs the code that the shader tool of the port wrote, until its sound plays through the audio output of the port with the latency of that output, and quits with the status 0 when every check passed.
local async = require('async')
local audio = require('haylen.audio')
local debug = require('haylen.debug')
local ffi = require('ffi')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local storage = require('haylen.storage')
local process = require('process')
local system = require('haylen.system')

local failures = 0
local checks = {
    {'platform', haylen.platform, 'test-port'},
    {'operating system', system.info().os, 'port'},
    {'graphics API', haylen.graphicsApi, 'port'},
}
for _, check in ipairs(checks) do
    if check[2] ~= check[3] then
        log.error(string.format('The %s of the test port is "%s" instead of "%s".', check[1], tostring(check[2]), check[3]))
        failures = failures + 1
    end
end

-- The process module of Varn runs on the external driver of the test port, which names its own folder.
if process.cwd() ~= '/test-port' then
    log.error(string.format('The process module of the test port runs in "%s" instead of the external driver of the port.', tostring(process.cwd())))
    failures = failures + 1
end

-- The test port does not allow `processes`, so the functions that start other programs say so.
local started, reason = pcall(io.popen, 'echo')
if started or not tostring(reason):find('This platform starts no other program, so "io.popen" is not available.', 1, true) then
    log.error(string.format('The test port started a program through "io.popen" or failed with "%s".', tostring(reason)))
    failures = failures + 1
end

-- The test port does not allow `executableMemory`, so the module `ffi` makes no callback of a Lua function, as the engine makes no native callback.
local made, refusal = pcall(ffi.cast, 'void (*)(void)', function() end)
if made or not tostring(refusal):find('This platform makes no native callback, since it writes no code into memory for the function pointer that native code would call.', 1, true) then
    log.error(string.format('The test port made a callback through "ffi.cast" or failed with "%s".', tostring(refusal)))
    failures = failures + 1
end

-- The test port runs threads with the event loop that its frames pump, so a job of the engine runs on a worker of the pools of Varn, which a trace shows on a lane of its own beside the lane of the frames.
local jobsChecked = false
debug.startTrace()
async.spawn(function()
    graphics.newImage(64, 64, '#FF8040FF'):encodePngAsync():await()
    local written = debug.stopTraceAsync('traces/port.json'):await()
    local trace = storage.readJson(written.path)
    local lanes = {}
    for _, event in ipairs(trace.traceEvents) do
        if event.name == 'thread_name' then
            lanes[event.tid] = event.args.name
        end
    end

    local workerJobs = 0
    for _, event in ipairs(trace.traceEvents) do
        if event.name == 'job' and lanes[event.tid] ~= 'frame' then
            workerJobs = workerJobs + 1
        end
    end
    if workerJobs == 0 then
        log.error('The jobs of the test port ran on the thread of the frames instead of the workers of its pools.')
        failures = failures + 1
    end
    jobsChecked = true
end)

-- The audio output of the test port plays 3 periods of 735 frames at 44100 frames each second, which the mixer reports as its latency.
local portLatency = 3 * 735 / 44100
local camera = graphics2d.newCamera()
local frames = 0

scene.push({
    update = function(self, dt)
        frames = frames + 1
        if frames < 10 or not jobsChecked or audio.outputState() ~= 'running' then
            return
        end

        if math.abs(audio.outputLatency() - portLatency) > 1e-6 then
            log.error(string.format('The audio output of the test port reports a latency of %f seconds instead of %f.', audio.outputLatency(), portLatency))
            failures = failures + 1
        end
        log.info(string.format('The test port drew %d frames through "%s", ran its jobs on workers and plays its sound.', frames, graphics.apiName()))
        haylen.quit({status = failures == 0 and 0 or 1})
    end,
    render = function(self)
        graphics2d.beginWorld(camera)
        graphics2d.drawCircle(0, 0, 40, '#FFD040FF')
        graphics2d.draw(graphics.whiteTexture(), 60, 0, {width = 40, height = 40, color = '#4080FFFF'})
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, 'Port', 40, 40, {size = 32})
    end,
})
