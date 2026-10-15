-- The made-up local market: its categories, twelve thousand listings of second-hand goods placed on the city map, the sellers with their reviews and the person who uses the app. Every value comes from a seeded generator, so the same market shows on every run.
local City = require('city')
local Random = require('random')

local Market = {}

Market.listingCount = 12000
Market.sellerCount = 400
Market.city = City(23)

Market.categories = {
    {id = 'furniture', name = 'Furniture', goods = {
        {'sofa', {'Blue three-seat sofa', 'Velvet loveseat', 'Compact two-seat sofa', 'Corner sofa with chaise'}},
        {'chair', {'Oak dining chair', 'Pair of wooden chairs', 'Folding café chair', 'Kitchen stool'}},
        {'lamp', {'Yellow table lamp', 'Bedside lamp', 'Brass reading lamp', 'Vintage desk lamp'}},
    }},
    {id = 'electronics', name = 'Electronics', goods = {
        {'phone', {'Smartphone, 128 GB', 'Phone with a new battery', 'Unlocked phone, 64 GB'}},
        {'camera', {'Instant camera', 'Film camera with strap', 'Compact digital camera'}},
        {'television', {'32 inch television', 'Small kitchen television', 'Smart television with remote'}},
        {'headphones', {'Wireless headphones', 'Studio headphones', 'Noise cancelling headphones'}},
    }},
    {id = 'bikes', name = 'Bikes and boards', goods = {
        {'bicycle', {'City bike with basket', 'Road bike, medium frame', 'Kids bike, 20 inch', 'Folding bike'}},
        {'skateboard', {'Complete skateboard', 'Cruiser board', 'Longboard with soft wheels'}},
    }},
    {id = 'music', name = 'Music', goods = {
        {'guitar', {'Acoustic guitar', 'Travel guitar with bag', 'Classical guitar', 'Ukulele'}},
    }},
    {id = 'books', name = 'Books', goods = {
        {'books', {'Box of novels', 'Cookbooks, set of four', 'Comic collection', 'Travel guides'}},
    }},
    {id = 'fashion', name = 'Fashion', goods = {
        {'jacket', {'Red rain jacket', 'Denim jacket', 'Winter coat, medium', 'Light bomber jacket'}},
        {'sneaker', {'White sneakers, size 42', 'Running shoes, size 39', 'High tops, size 44'}},
        {'watch', {'Leather strap watch', 'Gold tone watch', 'Diver watch'}},
    }},
    {id = 'kids', name = 'Kids and toys', goods = {
        {'teddy', {'Big teddy bear', 'Plush bear with bow', 'Soft toy set'}},
    }},
    {id = 'games', name = 'Games', goods = {
        {'gamepad', {'Wireless controller', 'Pair of controllers', 'Arcade pad'}},
    }},
    {id = 'home', name = 'Home and garden', goods = {
        {'kettle', {'Electric kettle', 'Teal kettle', 'Stovetop kettle'}},
        {'plant', {'Monstera in a pot', 'Snake plant', 'Hanging pothos', 'Rubber plant'}},
    }},
}

Market.backdrops = {'mint', 'sky', 'peach', 'lemon', 'lilac', 'rose', 'sand', 'slate'}
Market.conditions = {'New', 'Like new', 'Good', 'Fair'}
Market.firstNames = {'Ana', 'Bruno', 'Clara', 'Diego', 'Elena', 'Felix', 'Gabi', 'Hugo', 'Ines', 'Jonas', 'Kira', 'Leo', 'Maya', 'Nico', 'Olga', 'Pedro', 'Rita', 'Sami', 'Tara', 'Victor', 'Wen', 'Yara', 'Zoe', 'Marta'}
Market.lastNames = {'Costa', 'Novak', 'Reyes', 'Lindqvist', 'Mori', 'Okoye', 'Silva', 'Dubois', 'Ferraz', 'Haddad', 'Iversen', 'Kato', 'Moreau', 'Petrov', 'Quinn', 'Rossi'}
Market.neighborhoods = {'Riverside', 'Old Town', 'Harbor', 'Hilltop', 'Market Square', 'University', 'Docklands', 'Garden District', 'North End', 'Lakeside', 'Midtown', 'Westgate'}
Market.ages = {'Just now', '20 min ago', '1 h ago', '3 h ago', 'Today', 'Yesterday', '2 days ago', '4 days ago', 'Last week'}
Market.reviewTexts = {
    'Friendly and on time, the item was as described.',
    'Quick replies and an easy pickup.',
    'Great condition, thanks again!',
    'Packed everything with care.',
    'Smooth deal, would buy again.',
    'A bit late, but very kind.',
}
Market.descriptions = {
    'Used for a year and kept with care. Pickup only, cash or card on the spot.',
    'Moving out, so it has to go this week. Small marks from use, works perfectly.',
    'Bought it new and barely used it. Comes with everything that was in the box.',
    'In good shape for its age. Happy to send more photos on request.',
}

-- The person who uses the app.
Market.me = {id = 'me', name = 'Rosa Almeida', rating = '4.9', sales = 12, joined = 'Member since 2021', neighborhood = 'Riverside'}

function Market.money(amount)
    return '$' .. tostring(math.floor(amount))
end

function Market.category(id)
    return Market.categoryById[id]
end

function Market.listing(id)
    return Market.listingById[id]
end

function Market.seller(id)
    return Market.sellerById[id]
end

function Market.build()
    local random = Random(41)
    Market.categoryById = {}
    for _, category in ipairs(Market.categories) do
        Market.categoryById[category.id] = category
    end
    Market.sellers = {}
    Market.sellerById = {}
    for index = 1, Market.sellerCount do
        local seller = {
            id = 's' .. index,
            name = random:pick(Market.firstNames) .. ' ' .. random:pick(Market.lastNames),
            rating = string.format('%.1f', random:number(4.1, 5)),
            sales = random:integer(1, 240),
            joined = 'Member since ' .. random:integer(2014, 2025),
            neighborhood = random:pick(Market.neighborhoods),
            reply = random:pick({'Replies within an hour', 'Replies within a day', 'Replies in minutes'}),
            listings = {},
        }
        Market.sellers[index] = seller
        Market.sellerById[seller.id] = seller
    end
    Market.sellerById.me = Market.me
    Market.me.listings = {}
    Market.listings = {}
    Market.listingById = {}
    for index = 1, Market.listingCount do
        local seller = Market.sellers[random:integer(1, #Market.sellers)]
        local listing = Market.newListing(random, index, seller)
        Market.listings[index] = listing
        Market.listingById[listing.id] = listing
        seller.listings[#seller.listings + 1] = listing
    end
end

-- Makes one listing from the generator: what it is, its price, its condition, its age and its place on the map.
function Market.newListing(random, index, seller)
    local category = random:pick(Market.categories)
    local good = random:pick(category.goods)
    local column, row = random:integer(0, 15), random:pick({0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15})
    local x, y, width, height = Market.city:blockRect(column, row)
    local price = math.floor(random:number(5, 60) ^ (random:chance(0.3) and 1.6 or 1.2))
    local listing = {
        id = 'l' .. index,
        type = 'listing',
        title = random:pick(good[2]),
        category = category.id,
        art = good[1],
        image = 'goods/' .. good[1] .. '.svg',
        backdrop = 'goods/backdrops/' .. random:pick(Market.backdrops) .. '.svg',
        price = price,
        condition = random:pick(Market.conditions),
        age = random:pick(Market.ages),
        neighborhood = seller.neighborhood,
        seller = seller.id,
        description = random:pick(Market.descriptions),
        x = x + random:number(12, width - 12),
        y = y + random:number(12, height - 12),
        views = random:integer(4, 900),
    }
    Market.describe(listing)
    return listing
end

-- Fills the texts a cell of a listing shows.
function Market.describe(listing)
    listing.priceText = listing.price == 0 and 'Free' or Market.money(listing.price)
    listing.place = listing.neighborhood .. ' · ' .. listing.age
    listing.searchText = string.lower(listing.title .. ' ' .. Market.categoryById[listing.category].name .. ' ' .. listing.neighborhood)
end

-- The reviews of a seller, made once from its id.
function Market.reviews(seller)
    if seller.reviews then
        return seller.reviews
    end
    local random = Random(#seller.id * 97 + seller.sales)
    local reviews = {}
    for index = 1, math.min(40, 3 + seller.sales // 3) do
        local stars = random:chance(0.8) and 5 or 4
        reviews[index] = {
            id = seller.id .. '-review-' .. index,
            type = 'review',
            author = random:pick(Market.firstNames) .. ' ' .. random:pick(Market.lastNames),
            stars = stars .. ' out of 5',
            text = random:pick(Market.reviewTexts),
            age = random:pick(Market.ages),
        }
    end
    seller.reviews = reviews
    return reviews
end

-- Adds a listing of the person to the market and returns it.
function Market.publish(fields)
    local index = #Market.listings + 1
    local x, y, width, height = Market.city:blockRect(3, 12)
    local listing = {
        id = 'l' .. index,
        type = 'listing',
        title = fields.title,
        category = fields.category,
        art = fields.art,
        image = 'goods/' .. fields.art .. '.svg',
        backdrop = 'goods/backdrops/' .. fields.backdrop .. '.svg',
        price = fields.price,
        condition = fields.condition,
        age = 'Just now',
        neighborhood = Market.me.neighborhood,
        seller = 'me',
        description = fields.description ~= '' and fields.description or 'No description yet.',
        x = x + width / 2,
        y = y + height / 2,
        views = 0,
    }
    Market.describe(listing)
    table.insert(Market.listings, 1, listing)
    Market.listingById[listing.id] = listing
    table.insert(Market.me.listings, 1, listing)
    return listing
end

Market.build()

return Market
