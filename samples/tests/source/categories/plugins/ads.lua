-- The fake ads of an SDK: a full-screen ad and a rewarded ad, each a screen of the plugin, so the engine covers the app while it shows and the app stands still, stays silent and hears "appInactive" and "appActive" around it. The rewarded ad grants its reward with "adRewarded" when the person watches it to the end.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Ads = haylen.class('Ads', DemoTest)

function Ads:enter()
    self:listen('appInactive', function()
        if haylen.appCovered() and self.showing then
            self.results:set('covered', 'pass', 'The app is covered while the ad shows', 'The app became inactive with "haylen.appCovered()" returning true.')
        end
    end)
    self:frame{
        hint = 'Show the full-screen ad and the rewarded ad, and watch or skip.',
        focus = 'interstitial',
        controls = {
            ui.button{id = 'interstitial', text = 'Show a full-screen ad', variant = 'primary', onClick = function() self:interstitial() end},
            ui.button{id = 'rewarded', text = 'Show a rewarded ad', onClick = function() self:rewarded() end},
            ui.label{text = 'The full-screen ad is a native screen in the colors of the ad, and the rewarded ad asks whether the person watches it to the end.', color = 'textMuted', font = 'caption'},
        },
        code = "local closed = demo.showInterstitial():await()\ndemo.onAdRewarded(function(reward) print(reward.amount) end)\nlocal result = demo.showRewarded():await()",
    }
    if self.native then
        self.connection = demo.onAdRewarded(function(reward)
            self.results:set('reward', 'pass', 'The event "adRewarded" grants the reward', string.format('%d %s arrived from %s.', reward.amount, reward.currency, reward.language))
        end)
    end
end

function Ads:exit()
    if self.connection then
        self.connection:disconnect()
    end
    Ads.super.exit(self)
end

function Ads:interstitial()
    self:act(function()
        local name = 'The full-screen ad closes'
        self.showing = true
        local closed, err = demo.showInterstitial():await()
        self.showing = false
        if err and err.code == 'cancelled' then
            self.results:set('interstitial', 'pass', name, 'The person closed the ad, which failed the call with the code "cancelled".')
            return
        end
        if err then
            self.results:failure('interstitial', name, err)
            return
        end
        self.results:set('interstitial', closed.closed and 'pass' or 'fail', name, closed.language .. ' answered once the person continued to the app.')
    end)
end

function Ads:rewarded()
    self:act(function()
        local name = 'The rewarded ad answers'
        self.showing = true
        local result, err = demo.showRewarded():await()
        self.showing = false
        if err then
            self.results:failure('rewarded', name, err)
            return
        end
        self.results:set('rewarded', 'pass', name, result.rewarded and string.format('The person watched to the end and earned %d %s.', result.amount, result.currency) or 'The person skipped the ad and earned nothing.')
    end)
end

return Ads
