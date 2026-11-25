-- The automatic run of the headless platform: it likes a post, watches and answers a story, shares a post with a friend, whose answer drops in as a banner, comments on a post and deletes the comment after an alert, follows and unfollows a person, searches people, chats, makes a post with a filter and a crop and deletes it after an alert, edits the profile, checks the fields of the sign-in, saves posts, opens links of the scheme and of the web and sends a post to a friend, logs each step and quits, so an unattended run checks every path of the app.
local haylen = require('haylen')
local log = require('haylen.log')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Chats = require('chats')
local Routes = require('routes')
local Shell = require('shell')
local Social = require('data.social')
local Widgets = require('widgets')

local Tour = {}

Tour.interval = 1.5

function Tour.steps()
    local feed = Social.feed()
    return {
        {'Home', function() end},
        {'Like a post', function() local page = Shell.top() Social.toggleLike(feed[1]) Widgets.popHeart(page, page.feed, feed[1]) end},
        {'Open a story', function() Shell.top():openStory(Social.stories[1].id) end},
        {'Next picture of the story', function() Shell.top():next() end},
        {'Reply to the story', function() Shell.top():reply('This looks amazing!') end},
        {'Close the story', function() ui.goBack() end},
        {'Open a post', function() Shell.open(Routes.post(feed[3])) end},
        {'Share the post', function() local page = Shell.top() Widgets.sharePost(page, page.post) end},
        {'Send it to a friend', function() Shell.top().shareSheet:dismiss({result = 'send'}) end},
        {'Pick the friend, whose answer drops in as a banner', function() Shell.top().friendSheet:dismiss({result = Chats.list[2].id}) end},
        {'Write a comment', function() Shell.top():send('What a view, I want to go there.') end},
        {'Ask before deleting the comment', function() local page = Shell.top() page:askDeleteComment(page.post.comments[#page.post.comments].id) end},
        {'Delete the comment', function() Shell.top().deleteAlert:dismiss({result = 'confirm'}) end},
        {'Open the profile of the author', function() Widgets.openPerson(feed[3].author) end},
        {'Follow the person', function() local page = Shell.top() if page.person.followed then Social.toggleFollow(page.person) end page:headAction('primary') end},
        {'Unfollow the person, which a snackbar takes back', function() Shell.top():headAction('primary') end},
        {'Explore', function() Shell.showTab('explore') end},
        {'Search people', function() local page = Shell.top() page.query = 'ma' page.gui:set('query', {value = 'ma'}) page:refresh() end},
        {'Messages', function() Shell.showTab('messages') end},
        {'Open a chat', function() Shell.router:push(Routes.chat(Chats.list[1].person)) end},
        {'Send a message', function() Shell.top():send('Saturday at the corner cafe sounds great.') end},
        {'Read the answer', function() end},
        {'Start a new post', function() Shell.showTab('create') end},
        {'Pick a picture', function() Shell.top():pick('balloons-valley') end},
        {'Edit the picture', function() Shell.top():next() end},
        {'Pick a filter', function() local page = Shell.top() page.gui:set('filters', {selected = {'vivid'}}) page:change('filter', 'vivid') end},
        {'Crop it close', function() local page = Shell.top() page.gui:set('crop', {selected = 'close'}) page:change('crop', 'close') end},
        {'Write the caption', function() Shell.top():action() end},
        {'Share the post', function() local page = Shell.top() page.caption = 'Balloons over the valley at dawn.' page.place = 'Green Valley' page.gui:set('caption', {value = page.caption}) page.gui:set('place', {value = page.place}) end},
        {'See it in the feed', function() Shell.top():share() end},
        {'Open the new post', function() Shell.open(Routes.post(Social.posts[1])) end},
        {'Ask before deleting the post', function() Shell.top():askDelete() end},
        {'Delete the post', function() Shell.top().deleteAlert:dismiss({result = 'confirm'}) end},
        {'Open the profile', function() Shell.showTab('profile') end},
        {'Edit the profile', function() Shell.top():headAction('primary') end},
        {'Open the sign-in', function() local page = Shell.top() page.editSheet:dismiss() page:headAction('account') end},
        {'Check the fields before they go out', function() Shell.top().signInSheet.screen:submit() end},
        {'Close the sign-in', function() Shell.top().signInSheet:dismiss() end},
        {'Show the saved posts', function() local page = Shell.top() Social.toggleSave(feed[5]) Social.toggleSave(feed[8]) page.section = 'saved' page:showGrid() end},
        {'Back home, which kept its screens', function() Shell.showTab('home') end},
        {'Back to the root of home', function() Shell.showTab('home') end},
        {'Open a link to a profile', function() Shell.router:open(Routes.link(Routes.profile('ava'))) end},
        {'Open a web link to a post', function() Shell.router:open(Routes.webLink(Routes.post(feed[4]))) end},
        {'Open a link to a chat', function() Shell.router:open(Routes.link(Routes.chat('kai'))) end},
        {'Open a link to a post that is gone', function() Shell.router:open(Routes.link('post/9999')) end},
        {'Back home', function() Shell.showTab('home') end},
        {'Send a post to a friend', function() Widgets.askFriend(Shell.top(), feed[2]) end},
        {'Back to the feed', function() Shell.top().friendSheet:dismiss() end},
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
