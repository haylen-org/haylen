-- The conversations of the person with sellers, each about one listing. A message the person sends gets an answer from the seller a moment later, and every change emits "chatChanged" on the event bus.
local events = require('haylen.events')
local timer = require('haylen.timer')

local Market = require('data.market')
local Random = require('random')

local Chats = {}

Chats.list = {}
Chats.byListing = {}
Chats.sent = 0
Chats.openers = {'Hi! Is this still available?', 'Could you hold it until Saturday?', 'Would you take a little less?', 'Can I pick it up tonight?'}
Chats.answers = {
    'Yes, it is still available.',
    'Sure, I can hold it for you.',
    'I could do a small discount if you pick it up today.',
    'I am home after six, does that work?',
    'Great, see you then!',
    'It works perfectly, I tested it this morning.',
}
Chats.times = {'09:12', '10:40', '11:05', '13:27', '15:50', '17:18', '18:02', '19:44', '21:10'}

function Chats.build()
    local random = Random(77)
    for index = 1, 26 do
        local listing = Market.listings[random:integer(1, 2000)]
        local conversation = Chats.newConversation(listing)
        local count = random:integer(2, 7)
        for line = 1, count do
            local outgoing = line % 2 == 1
            Chats.append(conversation, outgoing and 'outgoing' or 'incoming', outgoing and random:pick(Chats.openers) or random:pick(Chats.answers), random:pick(Chats.times))
        end
        conversation.unread = random:chance(0.35) and random:integer(1, 3) or 0
        Chats.describe(conversation)
    end
end

function Chats.newConversation(listing)
    local seller = Market.seller(listing.seller)
    local conversation = {
        id = 'chat-' .. listing.id,
        type = 'conversation',
        listing = listing.id,
        name = seller.name,
        title = listing.title .. ' · ' .. listing.priceText,
        image = listing.image,
        backdrop = listing.backdrop,
        messages = {{id = 'chat-' .. listing.id .. '-day', type = 'day', text = 'Today'}},
        unread = 0,
    }
    Chats.list[#Chats.list + 1] = conversation
    Chats.byListing[listing.id] = conversation
    return conversation
end

-- Returns the conversation about a listing, made when it does not exist yet.
function Chats.about(listing)
    return Chats.byListing[listing.id] or Chats.newConversation(listing)
end

function Chats.append(conversation, kind, text, time)
    Chats.sent = Chats.sent + 1
    conversation.messages[#conversation.messages + 1] = {id = conversation.id .. '-' .. Chats.sent, type = kind, text = text, time = time}
    conversation.last = (kind == 'outgoing' and 'You: ' or '') .. text
    conversation.time = time
end

-- Fills the texts a row of the inbox shows.
function Chats.describe(conversation)
    conversation.hasUnread = conversation.unread > 0
    conversation.unreadText = tostring(conversation.unread)
end

-- Sends a message and moves the conversation to the top of the inbox. The seller answers after a moment, whether or not the chat is still open.
function Chats.send(conversation, text)
    Chats.append(conversation, 'outgoing', text, 'Now')
    Chats.toTop(conversation)
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
