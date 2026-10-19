-- A listing: the gallery of its pictures, the price, the condition, the description, the seller, a map of the pickup area drawn into a render target that the UI shows as a picture, and the buttons that message the seller or make an offer.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local math2d = require('haylen.math')
local ui = require('haylen.ui')

local Chats = require('chats')
local City = require('city')
local Market = require('data.market')
local Page = require('page')
local Saved = require('saved')
local Shell = require('shell')
local Widgets = require('widgets')

local Listing = haylen.class('Listing', Page)

Listing.galleryBackdrops = {'sky', 'peach', 'lemon', 'lilac', 'mint', 'rose', 'sand', 'slate'}
-- Pixels of the render target of the pickup map, drawn with the linear filter at about twice the size it shows.
Listing.mapSize = {960, 440}

function Listing:enter()
    local listing = Market.listing(self.params.listing)
    self.listing = listing
    local seller = Market.seller(listing.seller)
    self.mapName = '@pickup-' .. listing.id
    self.mapTarget = graphics.newRenderTarget(Listing.mapSize[1], Listing.mapSize[2], {filter = 'linear'})
    ui.setImage(self.mapName, self.mapTarget)
    self.mapCamera = graphics2d.newCamera()
    self.mapCamera.position = {listing.x + 40, listing.y - 30}
    local mine = listing.seller == 'me'
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header(Market.category(listing.category).name, {
            ui.button{id = 'save', icon = Shell.icon(Saved.has(listing) and 'heart-filled' or 'heart'), iconColor = 'accentText', variant = 'icon', tooltip = 'Save', accessibilityLabel = 'Save', onClick = function()
                local saved = Saved.toggle(listing)
                self.gui:set('save', {icon = Shell.icon(saved and 'heart-filled' or 'heart')})
                Shell.notify(saved and 'Saved. Find it in the saved tab.' or 'Removed from your saved listings.')
            end},
            ui.button{id = 'share', icon = Shell.icon('share'), variant = 'icon', tooltip = 'Share', accessibilityLabel = 'Share', onClick = function()
                Shell.notify('The link to this listing is ready to share.', 'information')
            end},
        }),
        ui.scroll{grow = 1, height = 0,
            ui.column{maxWidth = 860, align = 'center', gap = 12, padding = {4, 20, 24, 20},
                ui.carousel{id = 'gallery', align = 'stretch', aspectRatio = 4 / 3, maxHeight = 520, children = self:galleryPages()},
                ui.row{gap = 10, margin = {6, 0, 0, 0},
                    ui.label{text = listing.priceText, font = 'title', color = 'accentText', grow = 1},
                    ui.badge{text = listing.condition, tone = 'accent'},
                },
                ui.label{text = listing.title, font = 'heading'},
                ui.row{gap = 6, ui.icon{image = Shell.icon('pin'), size = 20, color = 'textMuted'}, ui.label{text = listing.place .. ' · ' .. listing.views .. ' views', font = 'caption', color = 'textMuted'}},
                ui.label{text = listing.description},
                ui.sectionTitle{text = 'Seller', margin = {8, 0, 0, 0}},
                ui.card{
                    ui.row{gap = 14,
                        ui.avatar{name = seller.name, size = 60},
                        ui.column{gap = 0, grow = 1,
                            ui.label{text = seller.name, font = 'button', wrap = false},
                            ui.label{text = string.format('%s stars · %d sales · %s', seller.rating, seller.sales, seller.joined), font = 'caption', color = 'textMuted'},
                        },
                        ui.button{id = 'seller', text = 'Profile', visible = not mine, onClick = function()
                            Shell.open(require('screens.seller'), {seller = seller.id})
                        end},
                    },
                },
                ui.sectionTitle{text = 'Pickup area', margin = {8, 0, 0, 0}},
                ui.image{id = 'pickup', image = self.mapName, fit = 'cover', align = 'stretch', aspectRatio = Listing.mapSize[1] / Listing.mapSize[2], radius = 20, accessibilityLabel = 'Map of the pickup area'},
                ui.label{text = 'The exact address comes in the chat once you agree on a time.', font = 'caption', color = 'textMuted'},
            },
        },
        ui.divider{},
        ui.panel{padding = {12, 20, 14, 20}, style = Shell.flatBar,
            Shell.centered(860, ui.row{wrap = true, gap = 12, visible = not mine,
                ui.button{id = 'message', text = 'Message', icon = Shell.icon('chat'), iconColor = 'text', grow = 1, onClick = function()
                    self:openChat()
                end},
                ui.button{id = 'offer', text = 'Make an offer', icon = Shell.icon('tag'), variant = 'primary', grow = 1, onClick = function()
                    self:openOffer()
                end},
            }),
            ui.label{text = 'This is your listing. Buyers write to you in the inbox.', visible = mine, color = 'textMuted', textAlign = 'center', align = 'stretch'},
        },
        ui.dialog{id = 'offerDialog', title = 'Make an offer', message = 'The seller sees your price and your message in the chat.',
            buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'send', text = 'Send the offer', variant = 'primary'}},
            onAnswer = function(event)
                if event.button == 'send' then
                    self:sendOffer()
                end
            end,
            ui.formField{label = 'Your price', ui.numberField{id = 'offerPrice', value = 0, min = 0, max = 100000, step = 5, onChange = function(event)
                self.offerPrice = event.value
            end}},
            ui.formField{label = 'Message', ui.textArea{id = 'offerMessage', rows = 3, placeholder = 'When could I pick it up?', onChange = function(event)
                self.offerMessage = event.value
            end}},
        },
    })
end

-- Three pages of the same good on different backdrops and at different sizes, the way a seller takes a few photos.
function Listing:galleryPages()
    local listing = self.listing
    local seed = tonumber(string.sub(listing.id, 2)) or 1
    local pages = {}
    for page = 1, 3 do
        local backdrop = Listing.galleryBackdrops[(seed + page * 3) % #Listing.galleryBackdrops + 1]
        pages[page] = Widgets.picture({image = listing.image, backdrop = page == 1 and listing.backdrop or 'goods/backdrops/' .. backdrop .. '.svg', radius = 24, inset = ({28, 8, 60})[page]})
    end
    return pages
end

function Listing:openChat()
    Shell.open(require('screens.chat'), {conversation = Chats.about(self.listing).id})
end

function Listing:openOffer()
    self.offerPrice = math.floor(self.listing.price * 0.85 / 5 + 0.5) * 5
    self.offerMessage = ''
    self.gui:set('offerPrice', {value = self.offerPrice})
    self.gui:set('offerMessage', {value = ''})
    self.gui:set('offerDialog', {open = true})
end

function Listing:sendOffer()
    local conversation = Chats.about(self.listing)
    local text = string.format('I would like to offer %s.', Market.money(self.offerPrice))
    if self.offerMessage ~= '' then
        text = text .. ' ' .. self.offerMessage
    end
    Chats.send(conversation, text)
    Shell.open(require('screens.chat'), {conversation = conversation.id})
end

function Listing:exit()
    ui.removeImage(self.mapName)
end

-- Draws the pickup map into its render target: the streets around the listing and a soft circle around the place, never the exact point.
function Listing:render()
    Page.render(self)
    local dark = ui.theme() == 'marketDark'
    local palette = dark and City.dark or City.light
    graphics2d.beginTarget(self.mapTarget, self.mapCamera, {clear = palette.ground})
    Market.city:draw(palette, self.mapCamera:visibleBounds())
    local accent = ui.themeColor('accent')
    local listing = self.listing
    graphics2d.drawCircle(listing.x + 40, listing.y - 30, 150, math2d.color(accent.r, accent.g, accent.b, 0.22))
    graphics2d.drawRing(listing.x + 40, listing.y - 30, 150, 4, accent)
    graphics2d.drawShape({listing.x + 22, listing.y - 48, 36, 36}, {radius = 18, color = accent, borderWidth = 4, borderColor = '#FFFFFFFF'})
end

return Listing
