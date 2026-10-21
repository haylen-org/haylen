-- The photos of the app: twelve painted pictures that every post, story and profile reuses. Each one shows whole or as a close crop around its subject with a filter, made on the task pool from the decoded picture and named for the UI with ui.setImage, so twelve files give twenty-four looks. Until a look is ready its name shows the mean color of its picture, so nothing waits on the frame thread.
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Photos = {}

-- Pixels on each side of the pictures, and of the close crop around their subject.
Photos.size = 768
Photos.closeSize = 512

-- The filters, as the effects of `image:applyAsync`, which the close crops use and the new post offers.
Photos.filters = {
    {id = 'normal', name = 'Normal', effects = {}},
    {id = 'warm', name = 'Warm', effects = {{effect = 'tint', color = '#FFFFB066', amount = 0.16}, {effect = 'adjust', saturation = 1.08}}},
    {id = 'cool', name = 'Cool', effects = {{effect = 'tint', color = '#FF5CA8FF', amount = 0.16}, {effect = 'adjust', contrast = 1.05}}},
    {id = 'vivid', name = 'Vivid', effects = {{effect = 'adjust', saturation = 1.35, contrast = 1.12}}},
    {id = 'fade', name = 'Fade', effects = {{effect = 'adjust', contrast = 0.82, brightness = 0.07, saturation = 0.82}}},
    {id = 'dusk', name = 'Dusk', effects = {{effect = 'tint', color = '#FFB25ADB', amount = 0.18}, {effect = 'adjust', contrast = 1.06}}},
    {id = 'mono', name = 'Mono', effects = {{effect = 'grayscale'}, {effect = 'adjust', contrast = 1.12}}},
}

-- The pictures: the file, the mean color that stands in while it loads, the subject the close crop centers on, as a share of the picture, and the filter of the close crop.
Photos.list = {
    {id = 'lake-sunrise', tone = '#FF7A7A8A', focus = {0.7, 0.76}, filter = 'warm'},
    {id = 'beach-umbrella', tone = '#FFB3B1AC', focus = {0.62, 0.72}, filter = 'vivid'},
    {id = 'noodle-soup', tone = '#FFA68350', focus = {0.42, 0.44}, filter = 'warm'},
    {id = 'pancakes', tone = '#FFB79670', focus = {0.7, 0.52}, filter = 'fade'},
    {id = 'cat-window', tone = '#FFAA8C60', focus = {0.36, 0.7}, filter = 'mono'},
    {id = 'dog-park', tone = '#FF9CA758', focus = {0.56, 0.42}, filter = 'vivid'},
    {id = 'city-tram', tone = '#FF9B5C41', focus = {0.47, 0.56}, filter = 'dusk'},
    {id = 'skyline-golden', tone = '#FF926E52', focus = {0.66, 0.32}, filter = 'cool'},
    {id = 'cafe-latte', tone = '#FFB29D7B', focus = {0.38, 0.4}, filter = 'fade'},
    {id = 'forest-trail', tone = '#FF7A7E4B', focus = {0.56, 0.62}, filter = 'cool'},
    {id = 'flower-market', tone = '#FF9C835F', focus = {0.66, 0.62}, filter = 'vivid'},
    {id = 'balloons-valley', tone = '#FF92A17E', focus = {0.68, 0.3}, filter = 'dusk'},
}

Photos.byId = {}
Photos.filterById = {}
for _, photo in ipairs(Photos.list) do
    Photos.byId[photo.id] = photo
end
for _, filter in ipairs(Photos.filters) do
    Photos.filterById[filter.id] = filter
end

function Photos.path(id)
    return 'photos/' .. id .. '.jpg'
end

-- The name of a look of a picture for the UI: `'full'` for the whole picture and `'close'` for the filtered crop around its subject.
function Photos.look(id, look)
    return '@photo-' .. id .. (look == 'close' and '-close' or '')
end

-- The area of the close crop of a picture, in its pixels.
function Photos.closeArea(photo)
    local half = Photos.closeSize / 2
    local x = math.max(0, math.min(Photos.size - Photos.closeSize, math.floor(photo.focus[1] * Photos.size - half)))
    local y = math.max(0, math.min(Photos.size - Photos.closeSize, math.floor(photo.focus[2] * Photos.size - half)))
    return {x, y, Photos.closeSize, Photos.closeSize}
end

function Photos.texture(image)
    return graphics.newTexture(image, {filter = 'linear', mipmaps = true})
end

-- Decodes a picture on the task pool and returns a promise of its image, which the asset cache does not keep.
function Photos.decode(id)
    return async.promise(function()
        local bytes = assets.bytesAsync(Photos.path(id)):await()
        return graphics.decodeImageAsync(bytes):await()
    end)
end

-- Names every look with the color of its picture at once, then makes the looks in the background in the order of the list.
function Photos.start()
    for _, photo in ipairs(Photos.list) do
        local stand = Photos.texture(graphics.newImage(2, 2, photo.tone))
        ui.setImage(Photos.look(photo.id, 'full'), stand)
        ui.setImage(Photos.look(photo.id, 'close'), stand)
    end
    async.spawn(function()
        for _, photo in ipairs(Photos.list) do
            local image = Photos.decode(photo.id):await()
            ui.setImage(Photos.look(photo.id, 'full'), Photos.texture(image))
            local close = image:crop(Photos.closeArea(photo)):applyAsync(Photos.filterById[photo.filter].effects):await()
            ui.setImage(Photos.look(photo.id, 'close'), Photos.texture(close))
        end
    end)
end

return Photos
