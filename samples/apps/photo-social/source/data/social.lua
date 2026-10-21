-- The made-up people of the app, their posts, comments and stories, and the person who uses it. Every value comes from a seeded generator, so the same feed shows on every run, and every change of a like, a save, a follow or a comment emits "socialChanged" on the event bus.
local events = require('haylen.events')

local Avatars = require('avatars')
local Photos = require('photos')
local Random = require('random')

local Social = {}

Social.people = {
    {id = 'ava', name = 'Ava Lindqvist', handle = 'ava.lind', bio = 'Mountains first, coffee second.'},
    {id = 'kai', name = 'Kai Moreno', handle = 'kaimoreno', bio = 'Surf, sand and slow mornings.'},
    {id = 'noor', name = 'Noor Haddad', handle = 'noor.h', bio = 'I cook for the people I love.'},
    {id = 'tomas', name = 'Tomás Rivera', handle = 'tomas.rides', bio = 'Taking the long way around the city.'},
    {id = 'mei', name = 'Mei Tanaka', handle = 'meitanaka', bio = 'Noodles, notebooks and quiet streets.'},
    {id = 'leo', name = 'Leo Okafor', handle = 'leo.okafor', bio = 'Dog dad. Ball thrower in chief.'},
    {id = 'sofia', name = 'Sofia Rossi', handle = 'sofi.rossi', bio = 'Flowers on every table.'},
    {id = 'arjun', name = 'Arjun Mehta', handle = 'arjun.cooks', bio = 'Weekend pancakes are a tradition.'},
    {id = 'lina', name = 'Lina Becker', handle = 'lina.b', bio = 'Plants, cats and sunny windows.'},
    {id = 'yusuf', name = 'Yusuf Demir', handle = 'yusufdemir', bio = 'Golden hour hunter.'},
    {id = 'clara', name = 'Clara Dubois', handle = 'clara.dubois', bio = 'Cafes where the croissants are warm.'},
    {id = 'mateo', name = 'Mateo Silva', handle = 'mateo.silva', bio = 'Trail runner and stream listener.'},
    {id = 'hana', name = 'Hana Kim', handle = 'hana.kim', bio = 'Up early for the balloons.'},
    {id = 'omar', name = 'Omar Farouk', handle = 'omar.farouk', bio = 'Rooftops and long walks.'},
    {id = 'elena', name = 'Elena Petrova', handle = 'elena.p', bio = 'Lakes in every season.'},
    {id = 'jonah', name = 'Jonah Brooks', handle = 'jonah.brooks', bio = 'Beach days and boat days.'},
    {id = 'priya', name = 'Priya Nair', handle = 'priya.nair', bio = 'Markets, colors and street food.'},
    {id = 'felix', name = 'Felix Wagner', handle = 'felixw', bio = 'City nights and tram rides.'},
}

Social.me = {id = 'me', name = 'Rosa Almeida', handle = 'rosa.almeida', bio = 'Collecting small happy moments around town.', me = true}

-- What the posts of each picture say, for the whole picture and for its close crop, and where they were taken.
Social.captions = {
    ['lake-sunrise'] = {place = 'Mirror Lake', full = 'Woke up at five for this light. Worth every minute.', close = 'Our little red canoe, waiting at the pier.'},
    ['beach-umbrella'] = {place = 'Coral Bay', full = 'Salt in the hair, sand everywhere, no complaints.', close = 'Hat, towel, ocean. That is the whole plan.'},
    ['noodle-soup'] = {place = 'Lantern Street', full = 'Found the best noodle soup in town, and I am not sharing the address.', close = 'The egg was perfect. Ten out of ten.'},
    ['pancakes'] = {place = 'Sunny Side Kitchen', full = 'Sunday stack with berries. The syrup was not optional.', close = 'A close look, because they deserve it.'},
    ['cat-window'] = {place = 'Home', full = 'Professional napper, part time window inspector.', close = 'Do not disturb.'},
    ['dog-park'] = {place = 'Riverside Park', full = 'He ran for an hour and still wanted more.', close = 'That smile when the ball comes back.'},
    ['city-tram'] = {place = 'Old Town', full = 'Evening ride through the old town. The lights came on just as we passed.', close = 'Catching the last tram of the day.'},
    ['skyline-golden'] = {place = 'Sunset Rooftop', full = 'Golden hour from the rooftop garden.', close = 'The city in blue.'},
    ['cafe-latte'] = {place = 'Corner Cafe', full = 'Latte, croissant, empty notebook. Today is a writing day.', close = 'A heart in the foam.'},
    ['forest-trail'] = {place = 'Pine Ridge Trail', full = 'Morning walk on the pine trail. The stream was singing.', close = 'Follow the signpost, wherever it points.'},
    ['flower-market'] = {place = 'Flower Square', full = 'Bought tulips, left with sunflowers too.', close = 'All the colors at once.'},
    ['balloons-valley'] = {place = 'Green Valley', full = 'Up at dawn to watch the balloons rise over the valley.', close = 'A rainbow in the sky.'},
}

Social.comments = {
    'This is stunning!', 'Saving this for my next trip.', 'Okay, now I am hungry.', 'The colors in this!', 'Where is this?',
    'I need this view on my wall.', 'So cozy.', 'Best one yet.', 'Take me with you next time.', 'Look at that face!',
    'How do you always find these places?', 'Weekend goals.', 'This made my day.', 'Wow, the light.', 'We have to go back there.',
}
Social.storyNotes = {'Good morning!', 'Look at this', 'Best day', 'Guess where', 'Back here again', false, false}

function Social.thousands(number)
    local text = tostring(number):reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
    return text
end

function Social.person(id)
    return Social.byId[id]
end

function Social.post(id)
    return Social.postById[id]
end

function Social.build()
    local random = Random(29)
    Social.byId = {me = Social.me}
    for index, person in ipairs(Social.people) do
        person.avatar = Avatars.register(person.id, index * 7 + 3)
        person.followers = random:integer(120, 48000)
        person.following = random:integer(80, 900)
        person.followed = index % 3 ~= 0
        person.posts = {}
        Social.byId[person.id] = person
    end
    local me = Social.me
    me.avatar = Avatars.register('me', 4)
    me.avatarRing = Avatars.registerRing('me', 4)
    me.followers, me.following, me.posts = 1284, 312, {}
    Social.saved = {}
    Social.posts, Social.postById = {}, {}
    -- Every person posts the looks of the pictures in an order of their own, and one post in four holds three looks that turn like pages.
    local looks = {}
    for _, photo in ipairs(Photos.list) do
        looks[#looks + 1] = {photo = photo.id, look = 'full'}
        looks[#looks + 1] = {photo = photo.id, look = 'close'}
    end
    for _, person in ipairs(Social.people) do
        local hours = random:number(0.2, 6)
        for _ = 1, random:integer(5, 9) do
            local pages = nil
            if random:chance(0.25) then
                pages = {random:pick(looks), random:pick(looks), random:pick(looks)}
            end
            Social.addPost(random, person, pages and pages[1] or random:pick(looks), pages, hours)
            hours = hours + random:number(8, 60)
        end
    end
    for index = 1, 9 do
        Social.addPost(random, me, looks[(index * 11) % #looks + 1], nil, index * 30)
    end
    table.sort(Social.posts, function(a, b) return a.hours < b.hours end)
    for _, person in ipairs(Social.people) do
        table.sort(person.posts, function(a, b) return a.hours < b.hours end)
    end
    Social.stories = {}
    for index = 1, 10 do
        local person = Social.people[(index * 7) % #Social.people + 1]
        local frames = {}
        for frame = 1, random:integer(2, 3) do
            local photo = Photos.list[(index * 3 + frame * 5) % #Photos.list + 1]
            frames[frame] = {image = Photos.look(photo.id, frame % 2 == 0 and 'close' or 'full'), note = random:pick(Social.storyNotes) or ''}
        end
        Social.stories[index] = {id = 'story-' .. person.id, type = 'story', person = person.id, name = person.handle, avatar = person.avatar, frames = frames, seen = false, ring = 'images/ring.svg', age = random:pick({'1 hour', '3 hours', '6 hours', '12 hours'})}
    end
end

-- Says how long ago something happened, from its age in hours.
function Social.age(hours)
    if hours < 1 then
        return math.max(1, math.floor(hours * 60)) .. ' minutes'
    elseif hours < 2 then
        return '1 hour'
    elseif hours < 24 then
        return math.floor(hours) .. ' hours'
    elseif hours < 48 then
        return '1 day'
    elseif hours < 24 * 14 then
        return math.floor(hours / 24) .. ' days'
    end
    return math.floor(hours / 24 / 7) .. ' weeks'
end

-- Adds a post of a person with one look, or with several pages of looks, posted some hours ago, and the comments it starts with.
function Social.addPost(random, author, first, pages, hours)
    local number = #Social.posts + 1
    local caption = Social.captions[first.photo]
    local post = {
        id = 'post-' .. number,
        type = pages and 'gallery' or 'post',
        author = author.id,
        photo = Photos.look(first.photo, first.look),
        place = caption.place,
        text = caption[first.look],
        likes = random:integer(12, 9800),
        hours = hours,
        age = Social.age(hours),
        liked = random:chance(0.2),
        comments = {},
    }
    if pages then
        post.pages = {}
        for page, look in ipairs(pages) do
            post.pages[page] = Photos.look(look.photo, look.look)
            post['page' .. page] = post.pages[page]
        end
    end
    local count = random:integer(2, 9)
    for index = 1, count do
        local writer = Social.people[random:integer(1, #Social.people)]
        post.comments[index] = Social.newComment(post, writer, random:pick(Social.comments), Social.age(random:number(0, hours)), random:integer(0, 40))
    end
    post.commentCount = count + random:integer(0, 60)
    Social.describe(post)
    Social.posts[number] = post
    Social.postById[post.id] = post
    author.posts[#author.posts + 1] = post
    return post
end

function Social.newComment(post, writer, text, age, likes)
    return {
        id = post.id .. '-comment-' .. (#post.comments + 1),
        type = 'comment',
        author = writer.id,
        avatar = writer.avatar,
        markup = '[b]' .. writer.handle .. '[/b]  ' .. text,
        meta = age == 'Now' and 'Now' or age .. ' ago',
        likes = likes,
        likesText = likes == 0 and '' or (likes == 1 and '1 like' or Social.thousands(likes) .. ' likes'),
    }
end

-- Fills the texts and pictures that the cells of a post bind.
function Social.describe(post)
    local author = Social.byId[post.author]
    post.authorName = author.handle
    post.avatar = author.avatar
    post.captionMarkup = '[b]' .. author.handle .. '[/b]  ' .. post.text
    post.likesText = post.likes == 1 and '1 like' or Social.thousands(post.likes) .. ' likes'
    post.commentsText = post.commentCount == 0 and 'Add a comment' or (post.commentCount == 1 and 'View the comment' or 'View all ' .. post.commentCount .. ' comments')
    post.ageText = post.age == 'Now' and 'Now' or post.age .. ' ago'
    post.likeIcon = post.liked and 'icons/heart-filled.svg' or 'icons/heart.svg'
    post.likeColor = post.liked and 'danger' or 'text'
    post.saveIcon = Social.saved[post.id] and 'icons/bookmark-filled.svg' or 'icons/bookmark.svg'
end

-- The posts of the people the person follows and the person's own, the newest first.
function Social.feed()
    local list = {}
    for _, post in ipairs(Social.posts) do
        local author = Social.byId[post.author]
        if author.followed or (author.me and post.hours == 0) then
            list[#list + 1] = post
        end
    end
    return list
end

function Social.toggleLike(post)
    post.liked = not post.liked
    post.likes = post.likes + (post.liked and 1 or -1)
    Social.describe(post)
    events.emit('socialChanged', post)
    return post.liked
end

function Social.toggleSave(post)
    Social.saved[post.id] = not Social.saved[post.id] or nil
    Social.describe(post)
    events.emit('socialChanged', post)
    return Social.saved[post.id] == true
end

function Social.toggleFollow(person)
    person.followed = not person.followed
    person.followers = person.followers + (person.followed and 1 or -1)
    Social.me.following = Social.me.following + (person.followed and 1 or -1)
    events.emit('socialChanged', person)
    return person.followed
end

function Social.comment(post, text)
    local comment = Social.newComment(post, Social.me, text, 'Now', 0)
    post.comments[#post.comments + 1] = comment
    post.commentCount = post.commentCount + 1
    Social.describe(post)
    events.emit('socialChanged', post)
    return comment
end

-- Publishes a post of the person who uses the app with a picture made at run time, at the top of the feed and the profile.
function Social.publish(image, text, place)
    local me = Social.me
    local number = #Social.posts + 1
    local post = {id = 'post-' .. number, type = 'post', author = 'me', photo = image, place = place ~= '' and place or 'Home', text = text ~= '' and text or 'A new day.', likes = 0, hours = 0, age = 'Now', liked = false, comments = {}, commentCount = 0}
    Social.describe(post)
    table.insert(Social.posts, 1, post)
    table.insert(me.posts, 1, post)
    Social.postById[post.id] = post
    events.emit('socialChanged', post)
    return post
end

function Social.savedPosts()
    local list = {}
    for _, post in ipairs(Social.posts) do
        if Social.saved[post.id] then
            list[#list + 1] = post
        end
    end
    return list
end

Social.build()

return Social
