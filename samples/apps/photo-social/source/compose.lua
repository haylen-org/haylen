-- The post being made: the picture, its crop and its filter, and the images of its steps. The previews are made on the task pool from the decoded picture and named for the UI with ui.setImage, so changing the filter never holds up a frame.
local async = require('async')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

local Photos = require('photos')

local Compose = {}

Compose.preview = '@compose-preview'
-- Pixels on each side of the middle of the picture that the small previews of the filters show.
Compose.thumbSize = 384
Compose.made = 0

function Compose.begin()
    Compose.photo, Compose.crop, Compose.filter = Photos.list[1].id, 'full', 'normal'
end

function Compose.thumbName(filter)
    return '@compose-filter-' .. filter
end

-- The look of the chosen picture with its crop, which shows until the preview with the filter is ready.
function Compose.look()
    return Photos.look(Compose.photo, Compose.crop)
end

-- Decodes the chosen picture and makes the small previews of every filter, which show the color of the picture until they are ready, then the preview of the post.
function Compose.prepare()
    local photo = Compose.photo
    Compose.image, Compose.texture = nil, nil
    local stand = Photos.texture(graphics.newImage(2, 2, Photos.byId[photo].tone))
    for _, filter in ipairs(Photos.filters) do
        ui.setImage(Compose.thumbName(filter.id), stand)
    end
    return async.promise(function()
        local image = Photos.decode(photo):await()
        if Compose.photo ~= photo then
            return
        end
        Compose.image = image
        local half = (Photos.size - Compose.thumbSize) // 2
        local middle = image:crop({half, half, Compose.thumbSize, Compose.thumbSize})
        for _, filter in ipairs(Photos.filters) do
            local thumb = middle:applyAsync(filter.effects):await()
            ui.setImage(Compose.thumbName(filter.id), Photos.texture(thumb))
        end
        Compose.render():await()
    end)
end

-- Makes the preview of the post with the chosen crop and filter, and resolves with whether it is the one the post shows now.
function Compose.render()
    local photo, crop, filter = Compose.photo, Compose.crop, Compose.filter
    return async.promise(function()
        local image = Compose.image
        if not image then
            return false
        end
        local area = crop == 'close' and Photos.closeArea(Photos.byId[photo]) or {0, 0, Photos.size, Photos.size}
        local made = image:crop(area):applyAsync(Photos.filterById[filter].effects):await()
        if Compose.photo ~= photo or Compose.crop ~= crop or Compose.filter ~= filter then
            return false
        end
        Compose.texture = Photos.texture(made)
        ui.setImage(Compose.preview, Compose.texture)
        return true
    end)
end

-- Names the finished picture for the post and returns the name, or the look of the picture while its preview is still being made.
function Compose.finish()
    if not Compose.texture then
        return Compose.look()
    end
    Compose.made = Compose.made + 1
    local name = '@post-made-' .. Compose.made
    ui.setImage(name, Compose.texture)
    return name
end

return Compose
