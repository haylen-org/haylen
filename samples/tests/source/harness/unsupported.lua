-- The page of a test that cannot run on this platform, which shows the exact reason instead of the test, the platforms where it runs and its code.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Unsupported = haylen.class('Unsupported', Test)

Unsupported.image = 'interface/images/sign.png'
Unsupported.names = {macos = 'macOS', windows = 'Windows', linux = 'Linux', ios = 'iOS', tvos = 'tvOS', android = 'Android', web = 'the web', headless = 'the headless player'}

function Unsupported:init(entry, reason)
    Unsupported.super.init(self, entry)
    self.reason = reason
end

function Unsupported:enter()
    local platforms = {}
    for _, platform in ipairs(self.entry.platforms) do
        platforms[#platforms + 1] = Unsupported.names[platform]
    end
    local runs = #platforms > 1 and table.concat(platforms, ', ', 1, #platforms - 1) .. ' and ' .. platforms[#platforms] or platforms[1]
    self:frame{
        code = Unsupported.codeOf(require(self.entry.module)),
        content = {
            ui.spacer{grow = 1},
            ui.emptyState{image = Unsupported.image, title = 'Not available on ' .. Unsupported.names[haylen.platform], message = self.reason},
            ui.label{text = 'This test runs on ' .. runs .. '.', color = 'accentText', textAlign = 'center', align = 'stretch'},
            ui.spacer{grow = 1},
        },
    }
end

-- The code of the class of a test: its `code`, or the excerpts of a Varn test one after the other.
function Unsupported.codeOf(class)
    if class.code or not class.excerpts then
        return class.code
    end
    local parts = {}
    for _, excerpt in ipairs(class.excerpts) do
        parts[#parts + 1] = '-- ' .. excerpt[1] .. '.\n' .. excerpt[2]
    end
    return table.concat(parts, '\n')
end

return Unsupported
