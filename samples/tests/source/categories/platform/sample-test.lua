-- The base of the tests of the plugin `platform-sample`: the rows of results on the stage, the summary in the status line and the language that answers on each platform. The project builds the native code of the plugin on Apple platforms, Android and the web, so a check fails there when the native code is missing, while the desktop player runs no native code and marks the checks that need it as not applicable.
local haylen = require('haylen')
local platform = require('haylen.platform')

local Results = require('harness.results')
local Test = require('harness.test')
local platformSample = require('platform-sample')

local SampleTest = haylen.class('SampleTest', Test)

SampleTest.languages = {ios = 'Objective-C', tvos = 'Objective-C', macos = 'Objective-C', android = 'Java', web = 'JavaScript'}
SampleTest.built = {ios = true, tvos = true, android = true, web = true}
SampleTest.player = 'The desktop player runs the Lua of the app and nothing else, so no native code of the plugin answers here. The macOS app of the project and the apps of the other platforms build it.'

function SampleTest:init(entry)
    SampleTest.super.init(self, entry)
    self.results = Results(entry.code)
end

-- Whether the native code runs here, after marking the row `native` as passed, failed where the project always builds it, or not applicable in the desktop player.
function SampleTest:checkNative()
    local name = 'The native code of "platform-sample" runs'
    if platformSample.available() then
        self.results:set('native', 'pass', name, 'The platform loaded the native part of the plugin, which ' .. tostring(SampleTest.languages[haylen.platform]) .. ' implements.')
        return true
    end
    if SampleTest.built[haylen.platform] then
        self.results:set('native', 'fail', name, string.format('The project builds the native part of the plugin on "%s", but the platform did not load it.', haylen.platform))
    else
        self.results:set('native', 'skip', name, SampleTest.player)
    end
    return false
end

function SampleTest:update(dt)
    SampleTest.super.update(self, dt)
    self:status(string.format('%s, platform "%s", native "%s", %d pending calls', self.results:summary(), haylen.platform, tostring(platformSample.available()), platform.pendingCallCount()))
end

return SampleTest
