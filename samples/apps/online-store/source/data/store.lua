-- The made-up store: its categories, three thousand products with their colors, sizes, prices and reviews, and the person who shops. Every value comes from a seeded generator, so the same store shows on every run.
local Random = require('random')

local Store = {}

Store.productCount = 3000

Store.colors = {
    {id = 'coral', name = 'Coral', value = '#FFF2545B'},
    {id = 'navy', name = 'Navy', value = '#FF3E5A96'},
    {id = 'sage', name = 'Sage', value = '#FF8DB596'},
    {id = 'sand', name = 'Sand', value = '#FFE8C89A'},
    {id = 'sky', name = 'Sky', value = '#FF7FB8E6'},
    {id = 'plum', name = 'Plum', value = '#FFA275A6'},
    {id = 'charcoal', name = 'Charcoal', value = '#FF6A6F80'},
    {id = 'mustard', name = 'Mustard', value = '#FFE8B53A'},
    {id = 'white', name = 'White', value = '#FFFFFFFF'},
}

Store.categories = {
    {id = 'tops', name = 'Tops', art = 'tee', sizes = {'XS', 'S', 'M', 'L', 'XL'}, base = 18, names = {'Everyday tee', 'Boxy tee', 'Pocket tee', 'Striped tee', 'Long sleeve tee'}},
    {id = 'hoodies', name = 'Hoodies', art = 'hoodie', sizes = {'XS', 'S', 'M', 'L', 'XL'}, base = 44, names = {'Cozy hoodie', 'Zip hoodie', 'Fleece hoodie', 'Oversized hoodie'}},
    {id = 'jeans', name = 'Jeans', art = 'jeans', sizes = {'26', '28', '30', '32', '34', '36'}, base = 56, names = {'Straight jeans', 'Wide leg jeans', 'Slim jeans', 'Relaxed jeans'}},
    {id = 'dresses', name = 'Dresses', art = 'dress', sizes = {'XS', 'S', 'M', 'L', 'XL'}, base = 52, names = {'Summer dress', 'Wrap dress', 'Midi dress', 'Linen dress'}},
    {id = 'sneakers', name = 'Sneakers', art = 'sneaker', sizes = {'38', '39', '40', '41', '42', '43', '44'}, base = 68, names = {'Court sneakers', 'Runner sneakers', 'Canvas sneakers', 'Trail sneakers'}},
    {id = 'boots', name = 'Boots', art = 'boot', sizes = {'38', '39', '40', '41', '42', '43', '44'}, base = 96, names = {'Chelsea boots', 'Rain boots', 'Hiking boots', 'Ankle boots'}},
    {id = 'hats', name = 'Hats', art = 'cap', sizes = {'One size'}, base = 22, names = {'Baseball cap', 'Six panel cap', 'Washed cap'}},
    {id = 'bags', name = 'Bags', art = 'backpack', sizes = {'One size'}, base = 48, names = {'City backpack', 'Roll top backpack', 'Day pack'}},
    {id = 'totes', name = 'Totes', art = 'tote', sizes = {'One size'}, base = 26, names = {'Canvas tote', 'Market tote', 'Zip tote'}},
    {id = 'watches', name = 'Watches', art = 'watch', sizes = {'One size'}, base = 120, names = {'Field watch', 'Minimal watch', 'Diver watch'}},
    {id = 'eyewear', name = 'Eyewear', art = 'sunglasses', sizes = {'One size'}, base = 64, names = {'Round sunglasses', 'Square sunglasses', 'Sport sunglasses'}},
    {id = 'audio', name = 'Audio', art = 'headphones', sizes = {'One size'}, base = 89, names = {'Over-ear headphones', 'Wireless headphones', 'Travel headphones'}},
    {id = 'speakers', name = 'Speakers', art = 'speaker', sizes = {'One size'}, base = 79, names = {'Bookshelf speaker', 'Portable speaker', 'Smart speaker'}},
    {id = 'lighting', name = 'Lighting', art = 'lamp', sizes = {'One size'}, base = 58, names = {'Table lamp', 'Desk lamp', 'Bedside lamp'}},
    {id = 'kitchen', name = 'Kitchen', art = 'mug', sizes = {'One size'}, base = 14, names = {'Stoneware mug', 'Espresso cup', 'Tall mug'}},
    {id = 'home', name = 'Home', art = 'candle', sizes = {'One size'}, base = 24, names = {'Scented candle', 'Pillar candle', 'Travel candle'}},
}

Store.adjectives = {'Classic', 'Organic', 'Essential', 'Weekend', 'Studio', 'Recycled', 'Heritage', 'Soft', 'Light', 'Urban'}
Store.backdrops = {'peach', 'sand', 'sky', 'mint', 'lilac', 'lemon', 'rose', 'slate'}
Store.reviewTexts = {
    'Fits true to size and the color is lovely.',
    'Great quality for the price, I bought a second one.',
    'Arrived quickly and well packed.',
    'Comfortable from the first day.',
    'A little smaller than I expected, but I kept it.',
    'Exactly like the pictures.',
}
Store.reviewers = {'Ana C.', 'Bruno R.', 'Clara M.', 'Diego S.', 'Elena K.', 'Felix N.', 'Gabi T.', 'Hugo L.', 'Ines P.', 'Jonas W.'}

Store.me = {name = 'Rosa Almeida', email = 'rosa@example.com', address = {name = 'Rosa Almeida', street = '12 Harbor Lane', city = 'Riverside', zip = '20450'}}

function Store.money(amount)
    return string.format('$%.2f', amount)
end

function Store.thousands(number)
    local text = tostring(number):reverse():gsub('(%d%d%d)', '%1,'):reverse():gsub('^,', '')
    return text
end

function Store.category(id)
    return Store.categoryById[id]
end

function Store.product(id)
    return Store.productById[id]
end

function Store.color(id)
    return Store.colorById[id]
end

function Store.build()
    Store.categoryById, Store.colorById = {}, {}
    for _, category in ipairs(Store.categories) do
        Store.categoryById[category.id] = category
    end
    for _, color in ipairs(Store.colors) do
        Store.colorById[color.id] = color
    end
    local random = Random(61)
    Store.products, Store.productById = {}, {}
    for index = 1, Store.productCount do
        local category = random:pick(Store.categories)
        local colors = {}
        local first = random:integer(1, #Store.colors)
        for offset = 0, random:integer(1, 3) do
            colors[#colors + 1] = Store.colors[(first + offset * 2 - 1) % #Store.colors + 1].id
        end
        local price = category.base + random:integer(0, 12) * 2 + 0.99
        local sale = random:chance(0.22) and random:pick({0.2, 0.3, 0.4}) or nil
        local product = {
            id = 'p' .. index,
            type = 'product',
            name = random:pick(Store.adjectives) .. ' ' .. string.lower(random:pick(category.names)),
            category = category.id,
            image = 'products/' .. category.art .. '.svg',
            backdrop = 'products/backdrops/' .. random:pick(Store.backdrops) .. '.svg',
            colors = colors,
            price = sale and math.floor(price * (1 - sale)) + 0.99 or price,
            oldPrice = sale and price or nil,
            rating = math.floor(random:number(3.8, 5) * 10 + 0.5) / 10,
            reviews = random:integer(3, 1800),
            popularity = random:integer(1, 100000),
        }
        product.name = product.name:sub(1, 1):upper() .. product.name:sub(2)
        Store.describe(product)
        Store.products[index] = product
        Store.productById[product.id] = product
    end
end

-- Fills the texts and the color a cell of a product shows: the first color of the product tints its picture.
function Store.describe(product)
    product.tint = Store.colorById[product.colors[1]].value
    product.priceText = Store.money(product.price)
    product.hasSale = product.oldPrice ~= nil
    product.priceMarkup = product.oldPrice and string.format('[color=#C2452D][b]%s[/b][/color]  [color=#9A9189][s]%s[/s][/color]', product.priceText, Store.money(product.oldPrice)) or '[b]' .. product.priceText .. '[/b]'
    product.saleText = product.oldPrice and string.format('%d%% off', math.floor((1 - product.price / product.oldPrice) * 100 + 0.5)) or ''
    product.ratingText = string.format('%.1f · %s reviews', product.rating, Store.thousands(product.reviews))
    product.colorsText = #product.colors == 1 and '1 color' or #product.colors .. ' colors'
    product.searchText = string.lower(product.name .. ' ' .. Store.categoryById[product.category].name)
end

-- The reviews of a product, made once from its id.
function Store.reviewsOf(product)
    if product.reviewList then
        return product.reviewList
    end
    local random = Random(tonumber(product.id:sub(2)) * 13)
    local list = {}
    for index = 1, math.min(product.reviews, 24) do
        local stars = random:chance(0.75) and 5 or random:integer(3, 4)
        list[index] = {id = product.id .. '-review-' .. index, author = random:pick(Store.reviewers), stars = stars .. ' out of 5', text = random:pick(Store.reviewTexts)}
    end
    product.reviewList = list
    return list
end

-- Products of the same category, for the shelf under a product.
function Store.related(product, count)
    local list = {}
    for _, other in ipairs(Store.products) do
        if other.category == product.category and other ~= product then
            list[#list + 1] = other
            if #list == count then
                break
            end
        end
    end
    return list
end

Store.build()

return Store
