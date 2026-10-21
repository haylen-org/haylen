-- The direct messages: one conversation with each of a few people, the newest on top. A message the person sends gets an answer a moment later, a post can travel in a message, and every change emits "chatChanged" on the event bus.
local events = require('haylen.events')
local timer = require('haylen.timer')

local Random = require('random')
local Social = require('data.social')

local Chats = {}

Chats.list = {}
Chats.byPerson = {}
Chats.sent = 0
Chats.openers = {'Hey! How was the trip?', 'Did you see the sunset yesterday?', 'Coffee on Saturday?', 'That photo is unreal.', 'Where did you get those pancakes?', 'Are you going to the market tomorrow?'}
Chats.answers = {
    'Yes! It was even better in person.',
    'Saturday works, the corner cafe at ten?',
    'Haha, thank you! The light did all the work.',
    'I will send you the address later.',
    'Count me in.',
    'Sounds perfect, see you there!',
    'I took about a hundred photos and kept one.',
}
Chats.times = {'9:12', '10:40', '11:05', '13:27', '15:50', '17:18', '18:02', '19:44', '21:10'}

function Chats.build()
    local random = Random(77)
    for index = 1, 9 do
        local person = Social.people[(index * 4) % #Social.people + 1]
        local conversation = Chats.newConversation(person)
        for line = 1, random:integer(3, 8) do
            local outgoing = line % 2 == 0
            Chats.append(conversation, outgoing and 'outgoing' or 'incoming', outgoing and random:pick(Chats.answers) or random:pick(Chats.openers), random:pick(Chats.times))
        end
        conversation.unread = random:chance(0.4) and random:integer(1, 3) or 0
        Chats.describe(conversation)
    end
end

function Chats.newConversation(person)
    local conversation = {
        id = 'chat-' .. person.id,
        type = 'conversation',
        person = person.id,
        name = person.name,
        handle = person.handle,
        avatar = person.avatar,
        messages = {{id = 'chat-' .. person.id .. '-day', type = 'day', text = 'Today'}},
        unread = 0,
    }
    Chats.list[#Chats.list + 1] = conversation
    Chats.byPerson[person.id] = conversation
    return conversation
end

-- Returns the conversation with a person, made when it does not exist yet.
function Chats.with(person)
    return Chats.byPerson[person.id] or Chats.newConversation(person)
end

function Chats.find(id)
    for _, conversation in ipairs(Chats.list) do
        if conversation.id == id then
            return conversation
        end
    end
end

function Chats.append(conversation, kind, text, time, photo)
    Chats.sent = Chats.sent + 1
    conversation.messages[#conversation.messages + 1] = {id = conversation.id .. '-' .. Chats.sent, type = kind, text = text, time = time, photo = photo}
    conversation.last = (kind == 'outgoing' or kind == 'outgoingPost') and 'You: ' .. text or text
    conversation.time = time
end

-- Fills the texts a row of the inbox shows.
function Chats.describe(conversation)
    conversation.hasUnread = conversation.unread > 0
    conversation.lastColor = conversation.hasUnread and 'text' or 'textMuted'
end

-- Sends a message, or a post with `post`, and moves the conversation to the top. The other person answers after a moment, whether or not the chat still shows.
function Chats.send(conversation, text, post)
    if post then
        Chats.append(conversation, 'outgoingPost', text, 'Now', post.photo)
    else
        Chats.append(conversation, 'outgoing', text, 'Now')
    end
    Chats.toTop(conversation)
    Chats.describe(conversation)
    events.emit('chatChanged', conversation)
    timer.after(1.6, function()
        Chats.append(conversation, 'incoming', Chats.answers[(Chats.sent % #Chats.answers) + 1], 'Now')
        conversation.unread = Chats.reading == conversation and 0 or conversation.unread + 1
        Chats.describe(conversation)
        events.emit('chatChanged', conversation)
    end, {owner = Chats})
end

function Chats.toTop(conversation)
    for index, other in ipairs(Chats.list) do
        if other == conversation then
            table.remove(Chats.list, index)
            break
        end
    end
    table.insert(Chats.list, 1, conversation)
end

-- Marks a conversation as read while its chat shows.
function Chats.read(conversation)
    Chats.reading = conversation
    if conversation then
        conversation.unread = 0
        Chats.describe(conversation)
        events.emit('chatChanged', conversation)
    end
end

function Chats.unreadCount()
    local count = 0
    for _, conversation in ipairs(Chats.list) do
        count = count + conversation.unread
    end
    return count
end

Chats.build()

return Chats
