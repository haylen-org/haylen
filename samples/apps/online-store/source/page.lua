-- The base of every screen: it builds the tree of the screen, shows the count of its bag buttons and runs the start of the screen the first time it shows, tells the screen when the window crosses the wide width, and shows its back button while there is somewhere to go back to.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Shell = require('shell')

local Page = haylen.class('Page', ui.Screen)

function Page:init(params)
    self.params = params or {}
end

function Page:build()
    return self:tree()
end

function Page:appear()
    Shell.adapt(self)
    self:showBack()
    if self.started then
        return
    end
    self.started = true
    Shell.showBag(self)
    if self.start then
        self:start()
    end
end

function Page:update(dt)
    Shell.adapt(self)
end

-- A screen beside the list of a wide window has nowhere to go back to, so its back button hides there.
function Page:showBack()
    if self.gui:has('back') then
        self.gui:set('back', {visible = self.stack ~= nil and self.stack.canGoBack})
    end
end

return Page
