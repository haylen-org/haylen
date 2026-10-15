-- The sell tab: a form that picks the photo of the item and its backdrop, the title, the category, the condition, the price and a description, checks them and publishes the listing.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Market = require('data.market')
local Page = require('page')
local Shell = require('shell')
local Widgets = require('widgets')

local Sell = haylen.class('Sell', Page)

function Sell:enter()
    self.fields = {title = '', category = nil, condition = 'Good', price = 20, description = '', backdrop = 'mint'}
    local categories, conditions, backdrops, photos = {}, {}, {gap = 8, wrap = true}, {}
    for _, category in ipairs(Market.categories) do
        categories[#categories + 1] = {id = category.id, text = category.name}
        for _, good in ipairs(category.goods) do
            photos[#photos + 1] = ui.imageButton{id = 'photo-' .. good[1], image = 'goods/' .. good[1] .. '.svg', scale = 0.5, accessibilityLabel = good[2][1], onClick = function(event)
                self:pickPhoto(good[1], category.id)
                event.gui:set('photoDialog', {open = false})
            end}
        end
    end
    for _, condition in ipairs(Market.conditions) do
        conditions[#conditions + 1] = {id = condition, text = condition}
    end
    for _, backdrop in ipairs(Market.backdrops) do
        backdrops[#backdrops + 1] = ui.chip{id = 'backdrop-' .. backdrop, text = backdrop:sub(1, 1):upper() .. backdrop:sub(2), selected = backdrop == self.fields.backdrop, onChange = function()
            self:pickBackdrop(backdrop)
        end}
    end
    self:mountGui(Shell.frame('sell', ui.column{grow = 1, gap = 0,
        ui.row{padding = {14, 20, 8, 20}, ui.label{text = 'Sell an item', font = 'title'}},
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 760, align = 'center', gap = 14, padding = {4, 20, 28, 20},
                ui.card{padding = {12, 12, 12, 22}, style = {colors = {raised = ui.themeColor('accentBackground'):toHex()}},
                    ui.row{gap = 8,
                        ui.column{gap = 4, grow = 1,
                            ui.label{text = 'Give your things a second life', font = 'heading'},
                            ui.label{text = 'Neighbors find them on the map, and you agree on a time in the chat.', font = 'caption', color = 'textMuted'},
                        },
                        ui.image{image = 'images/hero.png', width = 180, height = 135, fit = 'contain', accessibilityHidden = true},
                    },
                },
                ui.row{gap = 18, align = 'stretch',
                    ui.stack{id = 'preview', width = 180, height = 180,
                        Widgets.picture({image = 'goods/camera.svg', backdrop = 'goods/backdrops/mint.svg', radius = 20, inset = 18}),
                    },
                    ui.column{gap = 10, grow = 1,
                        ui.formField{id = 'photoField', label = 'Photo', help = 'Pick what you sell and a backdrop.', required = true,
                            ui.button{id = 'choosePhoto', text = 'Choose a photo', icon = Shell.icon('camera'), iconColor = 'text', align = 'start', onClick = function(event)
                                event.gui:set('photoDialog', {open = true})
                            end},
                        },
                        ui.row(backdrops),
                    },
                },
                ui.formField{id = 'titleField', label = 'Title', required = true, help = 'Say what it is, its size or its color.',
                    ui.textField{id = 'title', placeholder = 'Yellow table lamp', maxLength = 60, autocapitalize = 'sentences', returnKey = 'next', onChange = function(event)
                        self.fields.title = event.value
                    end},
                },
                ui.formField{id = 'categoryField', label = 'Category', required = true,
                    ui.combo{id = 'category', items = categories, placeholder = 'Pick a category', onChange = function(event)
                        self.fields.category = event.value
                    end},
                },
                ui.formField{label = 'Condition', ui.segmentedControl{id = 'condition', items = conditions, selected = 'Good', onChange = function(event)
                    self.fields.condition = event.value
                end}},
                ui.formField{label = 'Price', help = 'A price of zero gives it away for free.', ui.numberField{id = 'price', value = 20, min = 0, max = 100000, step = 5, onChange = function(event)
                    self.fields.price = event.value
                end}},
                ui.formField{label = 'Description', ui.textArea{id = 'description', rows = 4, maxLength = 400, placeholder = 'How old it is, what comes with it and where to pick it up.', onChange = function(event)
                    self.fields.description = event.value
                end}},
                ui.button{id = 'publish', text = 'Publish the listing', variant = 'primary', align = 'stretch', onClick = function()
                    self:publish()
                end},
            },
        },
        ui.dialog{id = 'photoDialog', title = 'Choose a photo', buttons = {{id = 'close', text = 'Close'}},
            ui.grid{minColumnWidth = 110, columns = 6, gap = 10, alignItems = 'center', children = photos},
        },
    }))
end

function Sell:pickPhoto(art, category)
    self.fields.art = art
    if not self.fields.category then
        self.fields.category = category
        self.gui:set('category', {selected = category})
    end
    self:showPreview()
    self.gui:set('photoField', {error = ''})
end

function Sell:pickBackdrop(backdrop)
    if self.fields.backdrop ~= backdrop then
        self.gui:set('backdrop-' .. self.fields.backdrop, {selected = false})
    end
    self.fields.backdrop = backdrop
    self.gui:set('backdrop-' .. backdrop, {selected = true})
    self:showPreview()
end

function Sell:showPreview()
    self.gui:replaceChildren('preview', {Widgets.picture({image = 'goods/' .. (self.fields.art or 'camera') .. '.svg', backdrop = 'goods/backdrops/' .. self.fields.backdrop .. '.svg', radius = 20, inset = 18})})
end

-- Checks the form, shows what is missing next to its field, and publishes the listing when nothing is.
function Sell:publish()
    local fields = self.fields
    local problems = {
        photoField = not fields.art and 'Choose a photo of the item.' or '',
        titleField = #fields.title:gsub('%s', '') < 3 and 'Write a title of at least three letters.' or '',
        categoryField = not fields.category and 'Pick the category of the item.' or '',
    }
    local valid = true
    for id, problem in pairs(problems) do
        self.gui:set(id, {error = problem})
        valid = valid and problem == ''
    end
    if not valid then
        Shell.notify('Some fields need a look before you publish.', 'warning')
        return
    end
    local listing = Market.publish(fields)
    Shell.notify('Your listing is live.')
    Shell.showTab('profile')
    Shell.open(require('screens.listing'), {listing = listing.id})
end

return Sell
