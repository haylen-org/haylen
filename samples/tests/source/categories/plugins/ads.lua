-- The fake ads of an SDK: a full-screen ad and a rewarded ad, each a screen of the plugin, so the engine covers the app while it shows and the app stands still, stays silent and hears "appInactive" and "appActive" around it. The rewarded ad grants its reward with "adRewarded" when the person watches it to the end, which fills the purse of the test.
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Ads = haylen.class('Ads', DemoTest)

function Ads:enter()
    self.coins = 0
    self.shownAds = 0
    self:listen('appInactive', function()
        if haylen.appCovered() and self.showing then
            self.results:set('covered', 'pass', 'The app is covered while the ad shows', 'The app became inactive with "haylen.appCovered()" returning true.')
        end
    end)
    self:frame{
        hint = 'Show the full-screen ad and the rewarded ad, and watch or skip.',
        focus = 'interstitial',
        checksEmpty = 'Show an ad to check how it covers the app.',
        controls = {
            ui.button{id = 'interstitial', text = 'Show a full-screen ad', variant = 'primary', onClick = function() self:interstitial() end},
            ui.button{id = 'rewarded', text = 'Show a rewarded ad', onClick = function() self:rewarded() end},
            ui.label{text = 'The full-screen ad is a native screen in the colors of the ad, and the rewarded ad asks whether the person watches it to the end.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 16, justify = 'center',
                ui.icon{image = 'plugins/coins.svg', size = 128, align = 'center'},
                ui.label{id = 'purse', text = '0 coins', font = 'title', textAlign = 'center', align = 'stretch'},
                ui.label{text = 'Rewards of the ads watched to the end', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
            ui.card{align = 'stretch', gap = 8,
                ui.label{text = 'The last ad', font = 'heading'},
                ui.label{id = 'lastAd', text = 'No ad has shown yet.', color = 'textMuted'},
            },
        },
        code = "local closed = demo.showInterstitial():await()\ndemo.onAdRewarded(function(reward) print(reward.amount) end)\nlocal result = demo.showRewarded():await()",
    }
    if self.native then
        self.connection = demo.onAdRewarded(function(reward)
            self.coins = self.coins + reward.amount
            self:set('purse', {text = string.format('%d %s', self.coins, reward.currency)})
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

function Ads:showLast(text)
    self.shownAds = self.shownAds + 1
    self:set('lastAd', {text = string.format('%s Ads shown in this visit: %d.', text, self.shownAds)})
end

function Ads:interstitial()
    self:act(function()
        local name = 'The full-screen ad closes'
        self.showing = true
        local closed, err = demo.showInterstitial():await()
        self.showing = false
        if err and err.code == 'cancelled' then
            self:showLast('The person closed the full-screen ad.')
            self.results:set('interstitial', 'pass', name, 'The person closed the ad, which failed the call with the code "cancelled".')
            return
        end
        if err then
            self.results:failure('interstitial', name, err)
            return
        end
        self:showLast('The person continued from the full-screen ad to the app.')
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
        self:showLast(result.rewarded and string.format('The person watched the rewarded ad to the end and earned %d %s.', result.amount, result.currency) or 'The person skipped the rewarded ad and earned nothing.')
        self.results:set('rewarded', 'pass', name, result.rewarded and string.format('The person watched to the end and earned %d %s.', result.amount, result.currency) or 'The person skipped the ad and earned nothing.')
    end)
end

return Ads
