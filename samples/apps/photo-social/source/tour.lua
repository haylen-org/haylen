-- The automatic run of the headless platform: it likes a post, watches and answers a story, comments on a post, follows a person, searches people, chats, makes a post with a filter and a crop, edits the profile, saves a post and sends one to a friend, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local Chats = require('chats')
local Shell = require('shell')
local Social = require('data.social')
local Widgets = require('widgets')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local feed = Social.feed()
    return {
        {'Home', function() end},
        {'Like a post', function() local page = scene.top() Social.toggleLike(feed[1]) Widgets.popHeart(page, page.feed, feed[1]) end},
        {'Open a story', function() scene.top():openStory(Social.stories[1].id) end},
        {'Next picture of the story', function() scene.top():next() end},
        {'Reply to the story', function() scene.top():reply('This looks amazing!') end},
        {'Close the story', function() Shell.back() end},
        {'Open a post', function() Shell.open(require('screens.post'), {post = feed[3].id}) end},
        {'Write a comment', function() scene.top():send('What a view, I want to go there.') end},
        {'Open the profile of the author', function() Widgets.openPerson(feed[3].author) end},
        {'Follow the person', function() local page = scene.top() if page.person.followed then Social.toggleFollow(page.person) end page:headAction('primary') end},
        {'Explore', function() Shell.showTab('explore') end},
        {'Search people', function() local page = scene.top() page.query = 'ma' page.gui:set('query', {value = 'ma'}) page:refresh() end},
        {'Messages', function() Shell.showTab('messages') end},
        {'Open a chat', function() Shell.open(require('screens.chat'), {conversation = Chats.list[1].id}) end},
        {'Send a message', function() scene.top():send('Saturday at the corner cafe sounds great.') end},
        {'Read the answer', function() end},
        {'Start a new post', function() Shell.showTab('create') end},
        {'Pick a picture', function() scene.top():pick('balloons-valley') end},
        {'Edit the picture', function() scene.top():next() end},
        {'Pick a filter', function() local page = scene.top() page.gui:set('filters', {selected = {'vivid'}}) page:change('filter', 'vivid') end},
        {'Crop it close', function() local page = scene.top() page.gui:set('crop', {selected = 'close'}) page:change('crop', 'close') end},
        {'Write the caption', function() Shell.open(require('screens.compose-share')) end},
        {'Share the post', function() local page = scene.top() page.caption = 'Balloons over the valley at dawn.' page.place = 'Green Valley' page.gui:set('caption', {value = page.caption}) page.gui:set('place', {value = page.place}) end},
        {'See it in the feed', function() scene.top():share() end},
        {'Open the profile', function() Shell.showTab('profile') end},
        {'Edit the profile', function() scene.top():headAction('primary') end},
        {'Show the saved posts', function() local page = scene.top() page.gui:set('editDialog', {open = false}) Social.toggleSave(feed[5]) Social.toggleSave(feed[8]) page.section = 'saved' page:showGrid() end},
        {'Back home', function() Shell.showTab('home') end},
        {'Send a post to a friend', function() Widgets.askFriend(scene.top(), feed[2]) end},
        {'Back to the feed', function() scene.top().gui:set('friendDialog', {open = false}) end},
    }
end

function Tour.start()
    local steps = Tour.steps()
    local index = 0
    timer.every(Tour.interval, function()
        index = index + 1
        if index > #steps then
            log.info('The tour finished.')
            haylen.quit()
            return
        end
        log.info(string.format('Step %d of %d: %s.', index, #steps, steps[index][1]))
        steps[index][2]()
    end, {owner = Tour})
end

return Tour
