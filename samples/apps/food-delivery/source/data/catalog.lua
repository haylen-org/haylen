-- The made-up restaurants of the city, their menus and the dishes they serve. Every value comes from a seeded generator, so the same city shows on every run.
local Random = require('random')

local Catalog = {}

Catalog.restaurantCount = 2400

Catalog.neighborhoods = {'Riverside', 'Old Town', 'Harbor', 'Hilltop', 'Market Square', 'University', 'Docklands', 'Garden District', 'North End', 'Lakeside', 'Midtown', 'Westgate'}

Catalog.prefixes = {'Golden', 'Little', 'Corner', 'Blue Door', 'Sunny', 'Harbor', 'Maple', 'Copper', 'Lucky', 'Velvet', 'Old Town', 'Red Lantern', 'Green Fig', 'Twin Oaks', 'Northside', 'Seaside', 'Night Owl', 'Happy', 'Urban', 'Wild Thyme'}

Catalog.cuisines = {
    {id = 'pizza', name = 'Pizza', art = 'pizza', places = {'Pizzeria', 'Slice House', 'Wood Oven'}, mains = {
        {'Margherita', 'Tomato, fresh mozzarella and basil on a blistered crust.'},
        {'Pepperoni', 'Crispy cupped pepperoni over a rich tomato sauce.'},
        {'Four Cheese', 'Mozzarella, gorgonzola, parmesan and smoked provolone.'},
        {'Mushroom Truffle', 'Roasted mushrooms, thyme and a drizzle of truffle oil.'},
        {'Garden Veggie', 'Peppers, olives, red onion and cherry tomatoes.'},
        {'Spicy Salami', 'Hot salami, chili honey and oregano.'},
    }},
    {id = 'burgers', name = 'Burgers', art = 'burger', places = {'Burger Bar', 'Grill House', 'Smash Shack'}, mains = {
        {'Classic Cheeseburger', 'Beef patty, cheddar, pickles and the house sauce.'},
        {'Double Smash', 'Two crispy patties, onions and melted cheese.'},
        {'Crispy Chicken Burger', 'Buttermilk chicken, slaw and spicy mayo.'},
        {'Mushroom Swiss', 'Sautéed mushrooms and Swiss cheese on a brioche bun.'},
        {'Garden Burger', 'A grilled vegetable patty with avocado and greens.'},
    }},
    {id = 'sushi', name = 'Sushi', art = 'sushi', places = {'Sushi Bar', 'Roll House', 'Fish Market'}, mains = {
        {'Salmon Nigiri', 'Four pieces of salmon over seasoned rice.'},
        {'Rainbow Roll', 'Crab and cucumber topped with five kinds of fish.'},
        {'Spicy Tuna Roll', 'Tuna, chili mayo and scallions.'},
        {'Avocado Maki', 'Ripe avocado and sesame in nori.'},
        {'Chef Selection', 'Twelve pieces of the catch of the day.'},
    }},
    {id = 'ramen', name = 'Ramen', art = 'ramen', places = {'Ramen House', 'Noodle Bar', 'Broth Kitchen'}, mains = {
        {'Tonkotsu Ramen', 'Creamy pork broth, chashu, egg and scallions.'},
        {'Miso Ramen', 'Rich miso broth, corn, butter and bean sprouts.'},
        {'Spicy Tantan', 'Sesame chili broth with minced pork and greens.'},
        {'Shoyu Ramen', 'A clear soy broth with bamboo shoots and nori.'},
        {'Vegetable Ramen', 'Kombu broth with tofu and seasonal vegetables.'},
    }},
    {id = 'tacos', name = 'Tacos', art = 'taco', places = {'Taqueria', 'Taco Stand', 'Cantina'}, mains = {
        {'Carnitas Tacos', 'Slow-cooked pork, onion and cilantro.'},
        {'Grilled Fish Tacos', 'Chipotle slaw and lime crema.'},
        {'Chicken Tinga Tacos', 'Smoky chicken with pickled onions.'},
        {'Mushroom Tacos', 'Roasted mushrooms, salsa verde and queso fresco.'},
        {'Beef Birria Tacos', 'Braised beef with a cup of its broth.'},
    }},
    {id = 'salads', name = 'Salads', art = 'salad', places = {'Greens', 'Salad Co.', 'Leaf Bar'}, mains = {
        {'Caesar Salad', 'Romaine, parmesan, croutons and a creamy dressing.'},
        {'Harvest Bowl', 'Roasted squash, quinoa, kale and pumpkin seeds.'},
        {'Greek Salad', 'Tomato, cucumber, olives and feta.'},
        {'Chicken Cobb', 'Grilled chicken, egg, avocado and blue cheese.'},
    }},
    {id = 'curry', name = 'Curry', art = 'curry', places = {'Curry House', 'Spice Kitchen', 'Masala Bar'}, mains = {
        {'Butter Chicken', 'Tender chicken in a silky tomato and butter sauce.'},
        {'Chickpea Masala', 'Chickpeas simmered with ginger, garlic and spices.'},
        {'Green Curry', 'Coconut green curry with vegetables and basil.'},
        {'Lamb Rogan', 'Slow-cooked lamb with warm spices.'},
        {'Paneer Tikka', 'Grilled paneer in a smoky masala.'},
    }},
    {id = 'pasta', name = 'Pasta', art = 'pasta', places = {'Trattoria', 'Pasta Bar', 'Osteria'}, mains = {
        {'Spaghetti Meatballs', 'Beef and pork meatballs in tomato sauce.'},
        {'Carbonara', 'Egg, pecorino, black pepper and crispy pork.'},
        {'Pesto Linguine', 'Basil pesto, green beans and potatoes.'},
        {'Lasagna', 'Layers of ragù, béchamel and parmesan.'},
        {'Cacio e Pepe', 'Pecorino and cracked black pepper.'},
    }},
    {id = 'dumplings', name = 'Dumplings', art = 'dumplings', places = {'Dumpling House', 'Steam Basket', 'Dim Sum Hall'}, mains = {
        {'Pork Dumplings', 'Eight steamed dumplings with black vinegar.'},
        {'Shrimp Har Gow', 'Translucent wrappers around sweet shrimp.'},
        {'Vegetable Potstickers', 'Pan-fried until golden and crisp.'},
        {'Soup Dumplings', 'Six dumplings full of hot broth.'},
    }},
    {id = 'desserts', name = 'Desserts', art = 'ice-cream', places = {'Creamery', 'Sweet Shop', 'Gelato Bar'}, mains = {
        {'Three Scoop Cone', 'Vanilla, strawberry and chocolate.'},
        {'Brownie Sundae', 'Warm brownie, vanilla gelato and fudge.'},
        {'Pistachio Gelato', 'Made every morning in small batches.'},
        {'Mango Sorbet', 'Bright, cold and dairy free.'},
    }},
    {id = 'donuts', name = 'Donuts', art = 'donut', places = {'Donut Shop', 'Bakehouse', 'Morning Glaze'}, mains = {
        {'Strawberry Sprinkle', 'A pink glaze with rainbow sprinkles.'},
        {'Boston Cream', 'Vanilla custard and a chocolate top.'},
        {'Maple Bar', 'A long donut with a maple glaze.'},
        {'Cinnamon Sugar', 'Warm, soft and rolled in cinnamon sugar.'},
    }},
    {id = 'coffee', name = 'Coffee', art = 'coffee', places = {'Coffee House', 'Roasters', 'Espresso Bar'}, mains = {
        {'Flat White', 'A double shot with silky milk.'},
        {'Oat Latte', 'Espresso and steamed oat milk.'},
        {'Cold Brew', 'Steeped for eighteen hours.'},
        {'Mocha', 'Espresso, chocolate and whipped cream.'},
    }},
    {id = 'juice', name = 'Juice', art = 'smoothie', places = {'Juice Bar', 'Smoothie Stop', 'Press House'}, mains = {
        {'Berry Blast', 'Strawberry, blueberry, banana and yogurt.'},
        {'Green Glow', 'Spinach, apple, cucumber and ginger.'},
        {'Tropical Sunrise', 'Mango, pineapple and passion fruit.'},
        {'Peanut Power', 'Banana, peanut butter, oats and milk.'},
    }},
    {id = 'poke', name = 'Poke', art = 'poke', places = {'Poke Bar', 'Island Bowls', 'Reef Kitchen'}, mains = {
        {'Salmon Poke', 'Salmon, avocado, edamame and sesame.'},
        {'Tuna Poke', 'Tuna, mango, cucumber and ponzu.'},
        {'Tofu Poke', 'Crispy tofu, seaweed salad and pickled ginger.'},
        {'Shrimp Poke', 'Garlic shrimp, corn and spicy mayo.'},
    }},
    {id = 'chicken', name = 'Fried Chicken', art = 'fried-chicken', places = {'Chicken Shack', 'Fry House', 'Crispy Coop'}, mains = {
        {'Eight Piece Bucket', 'Crispy chicken with two large sides.'},
        {'Hot Wings', 'Twelve wings in a sweet and fiery glaze.'},
        {'Chicken Tenders', 'Five tenders with honey mustard.'},
        {'Spicy Sandwich', 'A spicy fillet with pickles and slaw.'},
    }},
    {id = 'breakfast', name = 'Breakfast', art = 'pancakes', places = {'Diner', 'Pancake House', 'Brunch Club'}, mains = {
        {'Buttermilk Pancakes', 'A stack of three with butter and maple syrup.'},
        {'Blueberry Pancakes', 'Fresh blueberries and lemon cream.'},
        {'Eggs Benedict', 'Poached eggs and hollandaise on a muffin.'},
        {'French Toast', 'Brioche, berries and powdered sugar.'},
    }},
    {id = 'noodles', name = 'Noodles', art = 'noodles', places = {'Wok House', 'Noodle Box', 'Street Kitchen'}, mains = {
        {'Pad Thai', 'Rice noodles, tamarind, peanuts and lime.'},
        {'Chow Mein', 'Crispy egg noodles with vegetables.'},
        {'Dan Dan Noodles', 'Sesame, chili oil and minced pork.'},
        {'Garlic Udon', 'Thick udon in butter, garlic and soy.'},
    }},
    {id = 'grill', name = 'Grill', art = 'steak', places = {'Steakhouse', 'Chop House', 'Fire Grill'}, mains = {
        {'Ribeye Steak', 'Grilled to order with roasted potatoes.'},
        {'Half Chicken', 'Herb-roasted chicken with greens.'},
        {'Grilled Salmon', 'Lemon butter and charred broccoli.'},
        {'Veggie Skewers', 'Halloumi, peppers and zucchini.'},
    }},
}

Catalog.sides = {
    {'Fries', 'Crispy and lightly salted.', 'fried-chicken'},
    {'Garden Salad', 'Mixed greens and a lemon dressing.', 'salad'},
    {'Garlic Bread', 'Toasted with herb butter.', 'pizza'},
    {'Steamed Dumplings', 'Four pork dumplings.', 'dumplings'},
    {'Noodle Cup', 'Sesame noodles with scallions.', 'noodles'},
}

Catalog.drinks = {
    {'Lemonade', 'Freshly squeezed with mint.', 'smoothie'},
    {'Iced Coffee', 'Cold brew over ice.', 'coffee'},
    {'Berry Smoothie', 'Strawberry and banana.', 'smoothie'},
    {'Hot Tea', 'A pot of jasmine tea.', 'coffee'},
}

Catalog.desserts = {
    {'Ice Cream Cone', 'Two scoops of your choice.', 'ice-cream'},
    {'Glazed Donut', 'A classic, warm and soft.', 'donut'},
    {'Mini Pancakes', 'With maple syrup and butter.', 'pancakes'},
}

Catalog.sizes = {
    {id = 'regular', text = 'Regular', extra = 0},
    {id = 'large', text = 'Large', extra = 3},
    {id = 'family', text = 'Family', extra = 9},
}

Catalog.extras = {
    {id = 'cheese', text = 'Extra cheese', price = 1.5},
    {id = 'spicy', text = 'Make it spicy', price = 0},
    {id = 'sauce', text = 'Extra sauce', price = 0.75},
}

-- Formats an amount of money with two decimals.
function Catalog.money(amount)
    return string.format('$%.2f', amount)
end

function Catalog.cuisine(id)
    return Catalog.cuisineById[id]
end

function Catalog.restaurant(id)
    return Catalog.restaurantById[id]
end

-- Builds the restaurants once, the first time the module loads.
function Catalog.build()
    Catalog.cuisineById = {}
    for _, cuisine in ipairs(Catalog.cuisines) do
        Catalog.cuisineById[cuisine.id] = cuisine
    end
    Catalog.restaurants = {}
    Catalog.restaurantById = {}
    local random = Random(7)
    for index = 1, Catalog.restaurantCount do
        local cuisine = random:pick(Catalog.cuisines)
        local fee = random:chance(0.3) and 0 or random:pick({0.99, 1.49, 1.99, 2.49, 2.99, 3.99})
        local minutes = random:integer(3, 11) * 5
        local restaurant = {
            id = 'r' .. index,
            type = 'restaurant',
            name = random:pick(Catalog.prefixes) .. ' ' .. random:pick(cuisine.places),
            cuisine = cuisine.id,
            neighborhood = random:pick(Catalog.neighborhoods),
            rating = math.floor(random:number(3.6, 4.95) * 10 + 0.5) / 10,
            reviews = random:integer(12, 4800),
            minutes = minutes,
            fee = fee,
            distance = math.floor(random:number(0.3, 7.5) * 10 + 0.5) / 10,
            priceLevel = random:integer(1, 3),
            promo = random:chance(0.18) and random:pick({'20% off', 'Free drink', 'Buy one, get one', '$5 off over $25'}) or nil,
            block = {random:integer(1, 14), random:integer(1, 14)},
            image = 'dishes/' .. cuisine.art .. '.svg',
            seed = index,
        }
        restaurant.searchText = string.lower(restaurant.name .. ' ' .. cuisine.name .. ' ' .. restaurant.neighborhood)
        restaurant.title = restaurant.name
        restaurant.details = Catalog.details(restaurant)
        restaurant.ratingText = string.format('%.1f', restaurant.rating)
        restaurant.hasPromo = restaurant.promo ~= nil
        Catalog.restaurants[index] = restaurant
        Catalog.restaurantById[restaurant.id] = restaurant
    end
end

-- Returns the line under the name of a restaurant: its cuisine, its delivery time and its fee.
function Catalog.details(restaurant)
    local fee = restaurant.fee == 0 and 'Free delivery' or (Catalog.money(restaurant.fee) .. ' delivery')
    return string.format('%s · %d to %d min · %s', Catalog.cuisineById[restaurant.cuisine].name, restaurant.minutes, restaurant.minutes + 10, fee)
end

-- Returns the items of the menu of a restaurant for a collection: section headers and dishes. The menu is made once per restaurant from its seed.
function Catalog.menu(restaurant)
    if restaurant.menu then
        return restaurant.menu
    end
    local random = Random(restaurant.seed * 31)
    local cuisine = Catalog.cuisineById[restaurant.cuisine]
    local items, dishes = {}, {}
    local function section(id, title)
        items[#items + 1] = {id = restaurant.id .. '-' .. id, type = 'section', title = title, section = id}
    end
    local function dish(sectionId, entry, art, base, tag)
        local id = restaurant.id .. '-' .. sectionId .. '-' .. (#items + 1)
        local item = {
            id = id,
            type = 'dish',
            name = entry[1],
            description = entry[2],
            price = base + random:integer(0, 8) * 0.5 + 0.49,
            image = 'dishes/' .. art .. '.svg',
            restaurant = restaurant.id,
            tag = tag or (random:chance(0.2) and random:pick({'Spicy', 'Vegetarian', 'New'}) or ''),
        }
        item.priceText = Catalog.money(item.price)
        item.hasTag = item.tag ~= ''
        items[#items + 1] = item
        dishes[#dishes + 1] = item
        return item
    end
    section('popular', 'Popular')
    for index = 1, 3 do
        dish('popular', cuisine.mains[index], cuisine.art, 9, 'Popular')
    end
    section('mains', cuisine.name)
    for _, entry in ipairs(cuisine.mains) do
        dish('mains', entry, cuisine.art, 8)
    end
    section('sides', 'Sides')
    for index = 1, random:integer(3, #Catalog.sides) do
        local entry = Catalog.sides[index]
        dish('sides', entry, entry[3], 3)
    end
    section('drinks', 'Drinks')
    for index = 1, random:integer(2, #Catalog.drinks) do
        local entry = Catalog.drinks[index]
        dish('drinks', entry, entry[3], 2)
    end
    section('desserts', 'Desserts')
    for index = 1, random:integer(2, #Catalog.desserts) do
        local entry = Catalog.desserts[index]
        dish('desserts', entry, entry[3], 4)
    end
    restaurant.menu = items
    restaurant.dishes = dishes
    return items
end

-- Finds a dish by id in the menu of its restaurant.
function Catalog.dish(id)
    local restaurant = Catalog.restaurantById[string.match(id, '^(r%d+)')]
    Catalog.menu(restaurant)
    for _, item in ipairs(restaurant.dishes) do
        if item.id == id then
            return item
        end
    end
end

Catalog.build()

return Catalog
