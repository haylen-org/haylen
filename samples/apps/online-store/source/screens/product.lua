-- A product: a gallery of four pictures in the chosen color, the price with the old price struck through, the rating with the reviews in a sheet, the colors, the sizes, the quantity, the details and a shelf of related products. Adding to the bag opens a sheet that leads to the bag or back to shopping.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Bag = require('bag')
local Page = require('page')
local Shell = require('shell')
local Store = require('data.store')
local Widgets = require('widgets')
local Wishlist = require('wishlist')

local Product = haylen.class('Product', Page)

Product.galleryBackdrops = {'sand', 'sky', 'lilac', 'mint', 'peach', 'rose', 'lemon', 'slate'}

function Product:enter()
    local product = Store.product(self.params.product)
    self.product = product
    local category = Store.category(product.category)
    self.color = product.colors[1]
    self.size = #category.sizes == 1 and category.sizes[1] or nil
    self.quantity = 1
    local swatches = {gap = 8, wrap = true}
    for _, colorId in ipairs(product.colors) do
        local color = Store.color(colorId)
        swatches[#swatches + 1] = ui.chip{id = 'color-' .. colorId, text = color.name, selected = colorId == self.color, onChange = function()
            self:pickColor(colorId)
        end}
    end
    local sizes = {}
    for _, size in ipairs(category.sizes) do
        sizes[#sizes + 1] = {id = size, text = size}
    end
    local reviews = {gap = 14}
    for _, review in ipairs(Store.reviewsOf(product)) do
        reviews[#reviews + 1] = ui.column{gap = 2,
            ui.row{gap = 8, ui.avatar{name = review.author, size = 36}, ui.label{text = review.author, font = 'button', grow = 1}, ui.label{text = review.stars, font = 'caption', color = 'textMuted'}},
            ui.label{text = review.text},
        }
    end
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(category.name, {
            ui.button{id = 'wish', icon = Shell.icon(Wishlist.has(product) and 'heart-filled' or 'heart'), iconColor = 'accentText', variant = 'icon', tooltip = 'Wishlist', accessibilityLabel = 'Add to the wishlist', onClick = function()
                local saved = Wishlist.toggle(product)
                self.gui:set('wish', {icon = Shell.icon(saved and 'heart-filled' or 'heart')})
                Shell.notify(saved and 'Added to your wishlist.' or 'Removed from your wishlist.')
            end},
            Widgets.bagButton(self),
        }),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 1100, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.row{id = 'top', gap = 28, wrap = true, align = 'stretch',
                    ui.carousel{id = 'gallery', minWidth = 300, maxWidth = 500, grow = 1, aspectRatio = 0.9, children = self:galleryPages()},
                    ui.column{gap = 12, minWidth = 300, maxWidth = 500, grow = 1,
                        ui.label{text = product.name, font = 'title'},
                        ui.richText{text = product.priceMarkup, font = 'heading'},
                        ui.row{gap = 8,
                            ui.icon{image = Shell.icon('star'), size = 22, color = 'warning'},
                            ui.label{text = product.ratingText, color = 'textMuted', grow = 1},
                            ui.button{id = 'reviews', text = 'Read reviews', variant = 'link', onClick = function(event)
                                event.gui:set('reviewDialog', {open = true})
                            end},
                        },
                        ui.label{id = 'colorName', text = 'Color: ' .. Store.color(self.color).name, font = 'button'},
                        ui.row(swatches),
                        ui.label{text = 'Size', font = 'button', visible = #sizes > 1},
                        ui.segmentedControl{id = 'size', items = sizes, visible = #sizes > 1, onChange = function(event)
                            self.size = event.value
                        end},
                        ui.row{gap = 12,
                            ui.stepper{id = 'quantity', value = 1, min = 1, max = 10, width = 200, onChange = function(event)
                                self.quantity = event.value
                            end},
                            ui.button{id = 'add', text = 'Add to bag', icon = Shell.icon('bag'), variant = 'primary', grow = 1, onClick = function()
                                self:addToBag()
                            end},
                        },
                        ui.label{id = 'sizeHint', text = 'Pick a size first.', color = 'dangerText', visible = false},
                        ui.row{gap = 8, ui.icon{image = Shell.icon('truck'), size = 22, color = 'textMuted'}, ui.label{text = 'Free shipping over $75 and free returns for 30 days.', font = 'caption', color = 'textMuted'}},
                        ui.accordion{id = 'details', items = {{id = 'about', text = 'About this product'}, {id = 'care', text = 'Materials and care'}, {id = 'returns', text = 'Shipping and returns'}},
                            ui.label{text = 'Made to last and easy to wear, with a clean shape that goes with everything in your closet.'},
                            ui.label{text = 'Organic or recycled materials. Wash cold and dry flat, or wipe with a soft cloth.'},
                            ui.label{text = 'Ships in one to two days. Send it back within 30 days in its box for a full refund.'},
                        },
                    },
                },
                ui.sectionTitle{text = 'You may also like', margin = {12, 0, 0, 0}},
                ui.collection{id = 'related', axis = 'horizontal', scrollbar = false, gap = 16, rememberFocus = true, types = {product = Widgets.productCard(210)}, onSelect = function(event)
                    Shell.open(require('screens.product'), {product = event.item})
                end},
            },
        },
        ui.dialog{id = 'addedDialog', title = 'Added to your bag', message = '',
            buttons = {{id = 'shop', text = 'Keep shopping'}, {id = 'bag', text = 'View bag', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'bag' then
                    Shell.open(require('screens.bag'))
                end
            end,
            ui.row{gap = 14, Widgets.picture({width = 96, height = 96, radius = 14, inset = 10, image = product.image, backdrop = product.backdrop, imageId = 'addedImage'}), ui.label{id = 'addedText', text = '', grow = 1}},
        },
        ui.dialog{id = 'reviewDialog', title = 'Reviews', message = product.ratingText, buttons = {{id = 'close', text = 'Close'}},
            ui.column(reviews),
        },
    })
    Widgets.showCount(self.gui)
    self.gui:collection('related'):setItems(Store.related(product, 12))
end

-- Four pictures of the product in its color: over its backdrop, closer, and over two other backdrops.
function Product:galleryPages()
    local product = self.product
    local seed = tonumber(product.id:sub(2))
    local pages = {}
    for page = 1, 4 do
        local backdrop = page == 1 and product.backdrop or 'products/backdrops/' .. Product.galleryBackdrops[(seed + page) % #Product.galleryBackdrops + 1] .. '.svg'
        pages[page] = Widgets.picture({image = product.image, backdrop = backdrop, tint = Store.color(self.color).value, radius = 24, inset = ({40, 4, 64, 24})[page], imageId = 'gallery-' .. page})
    end
    return pages
end

function Product:pickColor(colorId)
    if self.color ~= colorId then
        self.gui:set('color-' .. self.color, {selected = false})
    end
    self.color = colorId
    self.gui:set('color-' .. colorId, {selected = true})
    local color = Store.color(colorId)
    self.gui:set('colorName', {text = 'Color: ' .. color.name})
    for page = 1, 4 do
        self.gui:set('gallery-' .. page, {tint = color.value})
    end
end

function Product:addToBag()
    if not self.size then
        self.gui:set('sizeHint', {visible = true})
        self.gui:command('size', 'focus')
        return
    end
    self.gui:set('sizeHint', {visible = false})
    Bag.add(self.product, self.color, self.size, self.quantity)
    local color = Store.color(self.color)
    self.gui:set('addedImage', {tint = color.value})
    self.gui:set('addedText', {text = string.format('%s in %s', self.product.name, string.lower(color.name)) .. (self.size ~= 'One size' and ', size ' .. self.size or '') .. ', quantity ' .. self.quantity .. '.'})
    self.gui:set('addedDialog', {open = true})
end

return Product
