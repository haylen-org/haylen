-- A product: a gallery of four pictures in the chosen color, the name and the price in the serif, the rating with the reviews in a sheet, the colors as round swatches, the sizes, the quantity, the details and a shelf of related products, with the share button in its bar. The gallery stands beside the details on wide windows and above them on phones, and adding to the bag opens a sheet that leads to the bag or back to shopping.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Page = require('page')
local Routes = require('routes')
local Shell = require('shell')
local Store = require('data.store')
local Style = require('style')
local Widgets = require('widgets')
local Wishlist = require('wishlist')

local Product = haylen.class('Product', Page)

Product.galleryBackdrops = {'sand', 'sky', 'lilac', 'mint', 'peach', 'rose', 'lemon', 'slate'}

-- What went into the bag, in a sheet as tall as its content, which hands back whether the person wants to see the bag.
local Added = haylen.class('Added', ui.Screen)

function Added:init(product, color, text)
    self.product, self.color, self.text = product, color, text
end

function Added:build()
    local space = Style.space
    local product = self.product
    return ui.column{gap = space.m, padding = {space.m, space.l, space.l, space.l},
        ui.label{text = 'Added to your bag', font = 'heading'},
        ui.row{gap = space.m, Widgets.picture({width = 104, height = 124, image = product.image, backdrop = product.backdrop, tint = self.color.value, inset = space.s}), ui.label{text = self.text, grow = 1}},
        ui.row{gap = space.s,
            ui.button{id = 'shop', text = 'Keep shopping', grow = 1, onClick = function() self.presentation:dismiss() end},
            ui.button{id = 'bag', text = 'View bag', variant = 'primary', grow = 1, autofocus = true, onClick = function() self.presentation:dismiss({result = 'bag'}) end},
        },
    }
end

-- The reviews of a product in a sheet that rests at half the screen and opens to nearly all of it.
local Reviews = haylen.class('Reviews', ui.Screen)

function Reviews:init(page)
    self.page = page
end

function Reviews:build()
    local space = Style.space
    return ui.column{gap = space.s, padding = {space.xs, space.l, space.l, space.l},
        ui.row{gap = space.s, ui.label{text = 'Reviews', font = 'heading', grow = 1}, ui.button{id = 'close', text = 'Close', onClick = function() self.presentation:dismiss() end}},
        ui.label{text = self.page.product.ratingText, color = 'textMuted'},
        ui.scroll{grow = 1, height = 0, ui.column(self.page:reviews())},
    }
end

function Product:tree()
    local product = Store.product(self.params.product)
    self.product = product
    self.category = Store.category(product.category)
    self.color = product.colors[1]
    self.size = #self.category.sizes == 1 and self.category.sizes[1] or nil
    self.quantity = 1
    return ui.column{gap = 0,
        Shell.header(self.category.name, {
            ui.button{id = 'share', icon = Shell.icon('share'), iconColor = 'text', variant = 'icon', tooltip = 'Share', accessibilityLabel = 'Share the product', onClick = function()
                self:share()
            end},
            ui.button{id = 'wish', icon = Shell.icon(Wishlist.has(product) and 'heart-filled' or 'heart'), iconColor = 'text', variant = 'icon', tooltip = 'Save', accessibilityLabel = 'Save the product', onClick = function()
                self:toggleSaved()
            end},
            Shell.bagButton(self, 'headerBag'),
        }),
        ui.scroll{grow = 1, height = 0,
            Shell.centered(Style.size.page, ui.column{gap = Style.space.m, padding = {Style.space.l, Style.space.l, Style.space.xxl, Style.space.l},
                ui.row{id = 'top', gap = Style.space.xl, align = 'stretch'},
                Widgets.section('You may also like'),
                ui.collection{id = 'related', axis = 'horizontal', gap = Style.space.m, rememberFocus = true, types = {product = Widgets.productCard(Style.size.card)}, onSelect = function(event)
                    Widgets.openProduct(event.item)
                end},
            }),
        },
    }
end

function Product:start()
    self.gui:collection('related'):setItems(Store.related(self.product, 12))
    events.on('wishlistChanged', function(changed)
        if changed == self.product then
            self.gui:set('wish', {icon = Shell.icon(Wishlist.has(self.product) and 'heart-filled' or 'heart')})
        end
    end, {owner = self})
end

-- Lays the gallery and the details side by side on wide windows and one above the other on phones.
function Product:layoutChanged(wide)
    local gallery = ui.carousel{id = 'gallery', aspectRatio = 0.9, align = 'stretch', grow = wide and 1 or 0, maxWidth = 640, children = self:galleryPages()}
    local details = self:details()
    if wide then
        details.width = 500
        self.gui:replaceChildren('top', {ui.column{grow = 1, gallery}, details})
    else
        details.grow = 1
        self.gui:replaceChildren('top', {ui.column{grow = 1, gap = Style.space.l, gallery, details}})
    end
    self:pickColor(self.color)
    if self.size then
        self:pickSize(self.size)
    end
end

function Product:details()
    local space = Style.space
    local product, category = self.product, self.category
    local swatches = {gap = space.xxs, wrap = true}
    for _, colorId in ipairs(product.colors) do
        local color = Store.color(colorId)
        swatches[#swatches + 1] = ui.imageButton{id = 'color-' .. colorId, image = Widgets.swatchName(colorId, false), accessibilityLabel = color.name, tooltip = color.name, onClick = function()
            self:pickColor(colorId)
        end}
    end
    local sizes = {gap = space.xs, wrap = true, visible = #category.sizes > 1}
    for _, size in ipairs(category.sizes) do
        sizes[#sizes + 1] = ui.button{id = 'size-' .. size, text = size, minWidth = 76, onClick = function()
            self:pickSize(size)
        end}
    end
    return ui.column{gap = space.m,
        ui.column{gap = space.xs,
            ui.label{text = product.name, font = 'title'},
            ui.richText{text = product.priceMarkup, font = 'heading'},
            ui.row{gap = space.xs,
                ui.icon{image = Shell.icon('star'), size = 24, color = 'warning'},
                ui.label{text = product.ratingText, color = 'textMuted', grow = 1},
                ui.button{id = 'reviews', text = 'Read reviews', variant = 'link', style = {metrics = {controlPaddingX = 0}}, onClick = function()
                    self:openReviews()
                end},
            },
        },
        ui.divider{},
        ui.label{id = 'colorName', text = '', font = 'button'},
        ui.row(swatches),
        ui.label{text = 'Size', font = 'button', visible = #category.sizes > 1, margin = {space.xs, 0, 0, 0}},
        ui.row(sizes),
        ui.label{id = 'sizeHint', text = 'Pick a size first.', color = 'dangerText', visible = false},
        ui.row{gap = space.s, margin = {space.xs, 0, 0, 0},
            ui.stepper{id = 'quantity', value = 1, min = 1, max = 10, width = 200, accessibilityLabel = 'Quantity', onChange = function(event)
                self.quantity = event.value
            end},
            ui.button{id = 'add', text = 'Add to bag', variant = 'primary', grow = 1, onClick = function()
                self:addToBag()
            end},
        },
        ui.row{gap = space.s, ui.icon{image = Shell.icon('truck'), size = 26, color = 'textMuted'}, ui.label{text = 'Free shipping over $75 and free returns for 30 days.', font = 'caption', color = 'textMuted', grow = 1}},
        ui.accordion{id = 'details', items = {{id = 'about', text = 'About this product'}, {id = 'care', text = 'Materials and care'}, {id = 'returns', text = 'Shipping and returns'}},
            ui.label{text = 'Made to last and easy to wear, with a clean shape that goes with everything in your closet.', color = 'textMuted'},
            ui.label{text = 'Organic or recycled materials. Wash cold and dry flat, or wipe with a soft cloth.', color = 'textMuted'},
            ui.label{text = 'Ships in one or two days. Send it back within 30 days in its box for a full refund.', color = 'textMuted'},
        },
    }
end

function Product:reviews()
    local list = {gap = Style.space.m}
    for _, review in ipairs(Store.reviewsOf(self.product)) do
        list[#list + 1] = ui.column{gap = Style.space.xxs,
            ui.row{gap = Style.space.s, ui.avatar{name = review.author, size = 44}, ui.label{text = review.author, font = 'button', grow = 1}, ui.label{text = review.stars, font = 'caption', color = 'textMuted'}},
            ui.label{text = review.text},
        }
    end
    return list
end

-- Four pictures of the product in its color: over its backdrop, closer, and over two other backdrops.
function Product:galleryPages()
    local product = self.product
    local seed = tonumber(product.id:sub(2))
    local pages = {}
    for page = 1, 4 do
        local backdrop = page == 1 and product.backdrop or 'products/backdrops/' .. Product.galleryBackdrops[(seed + page) % #Product.galleryBackdrops + 1] .. '.svg'
        pages[page] = Widgets.picture({image = product.image, backdrop = backdrop, tint = Store.color(self.color).value, radius = Style.radius.card, inset = ({48, 8, 72, 28})[page], imageId = 'gallery-' .. page})
    end
    return pages
end

function Product:pickColor(colorId)
    local previous = self.color
    self.color = colorId
    if self.gui:has('color-' .. previous) then
        self.gui:set('color-' .. previous, {image = Widgets.swatchName(previous, false)})
    end
    self.gui:set('color-' .. colorId, {image = Widgets.swatchName(colorId, true)})
    local color = Store.color(colorId)
    self.gui:set('colorName', {text = 'Color: ' .. color.name})
    for page = 1, 4 do
        self.gui:set('gallery-' .. page, {tint = color.value})
    end
end

function Product:pickSize(size)
    if self.size and self.gui:has('size-' .. self.size) then
        self.gui:set('size-' .. self.size, {variant = 'default'})
    end
    self.size = size
    self.gui:set('size-' .. size, {variant = 'primary'})
    self.gui:set('sizeHint', {visible = false})
end

-- Saves the product for later or takes it out of the saved products, which a snackbar offers to undo, and the heart follows the list.
function Product:toggleSaved()
    local product = self.product
    if Wishlist.toggle(product) then
        Shell.notify('Saved for later.')
        return
    end
    Shell.offerUndo('Removed from your saved products.', function()
        Wishlist.toggle(product)
    end)
end

-- Offers the links of the product in an action sheet that points at the share button.
function Product:share()
    self.shareSheet = Shell.share(self.product.name, Routes.product(self.product), {gui = self.gui, id = 'share'})
end

function Product:addToBag()
    if not self.size then
        self.gui:set('sizeHint', {visible = true})
        self.gui:command('size-' .. self.category.sizes[1], 'focus')
        return
    end
    Bag.add(self.product, self.color, self.size, self.quantity)
    local color = Store.color(self.color)
    local text = string.format('%s in %s', self.product.name, string.lower(color.name)) .. (self.size ~= 'One size' and ', size ' .. self.size or '') .. ', quantity ' .. self.quantity .. '.'
    self.addedSheet = ui.present(Added(self.product, color, text), {detents = {'fit'}, owner = self, onDismiss = function(event)
        if event.result == 'bag' then
            Shell.openBag()
        end
    end})
end

function Product:openReviews()
    self.reviewsSheet = ui.present(Reviews(self), {detents = {'medium', 'large'}, owner = self})
end

return Product
