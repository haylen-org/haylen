-- The page of a test that cannot run on this platform, which shows the exact reason instead of the test.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Unsupported = haylen.class('Unsupported', Test)

function Unsupported:init(entry, reason)
    Unsupported.super.init(self, entry)
    self.reason = reason
end

function Unsupported:enter()
    self:frame{content = {
        ui.spacer{grow = 1},
        ui.emptyState{title = string.format('Unsupported on "%s"', haylen.platform), message = self.reason},
        ui.spacer{grow = 1},
    }}
end

return Unsupported
