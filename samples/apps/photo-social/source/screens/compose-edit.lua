-- The second step of a new post: the picture with its crop and filter, a switch between the whole picture and a close crop, and the filters, each shown on the middle of the picture.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Compose = require('compose')
local Page = require('page')
local Photos = require('photos')
local Shell = require('shell')
local Style = require('style')

local ComposeEdit = haylen.class('ComposeEdit', Page)

ComposeEdit.crops = {{id = 'full', text = 'Whole picture'}, {id = 'close', text = 'Close up'}}

function ComposeEdit:enter()
    self:mountGui(ui.column{gap = 0, onCancel = Shell.back,
        Shell.header('Edit', {
            ui.button{id = 'next', text = 'Next', variant = 'link', onClick = function()
                Shell.open(require('screens.compose-share'))
            end},
        }),
        ui.column{id = 'body', grow = 1, gap = 0},
    })
    scene.spawn(self, function()
        Compose.prepare():await()
        self:showPreview(true)
    end)
end

function ComposeEdit:layoutChanged(wide)
    local space = Style.space
    local preview = ui.image{id = 'preview', image = self.ready and Compose.preview or Compose.look(), fit = 'cover', aspectRatio = 1, accessibilityLabel = 'The picture of the post'}
    local tools = ui.column{gap = space.m, padding = wide and 0 or {space.m, 0},
        ui.row{padding = {0, space.m}, ui.segmentedControl{id = 'crop', items = ComposeEdit.crops, selected = Compose.crop, grow = 1, onChange = function(event)
            self:change('crop', event.value)
        end}},
        ui.label{text = 'Filters', font = 'button', color = 'textMuted', margin = {0, space.m}},
        ui.collection{id = 'filters', axis = wide and 'vertical' or 'horizontal', layout = wide and 'grid' or 'linear', lanes = 3, grow = wide and 1 or 0, scrollbar = false, gap = space.s, padding = {0, space.m}, selection = 'single', selected = {Compose.filter}, rememberFocus = true,
            types = {filter = {template = ui.column{gap = space.xs, width = wide and 'auto' or 128, padding = space.xxs,
                ui.image{part = 'thumb', bind = {image = 'thumb'}, image = Compose.look(), fit = 'cover', aspectRatio = 1, width = wide and 'auto' or 120, align = 'stretch', radius = Style.radius.control},
                ui.label{part = 'name', bind = {text = 'name'}, font = 'caption', textAlign = 'center', align = 'stretch', wrap = false},
            }}},
            onSelect = function(event)
                self:change('filter', event.item)
            end,
        },
    }
    local body
    if wide then
        preview.grow, preview.align = 1, 'center'
        tools.width = 460
        tools.align = 'stretch'
        body = ui.row{grow = 1, gap = space.l, padding = space.l, align = 'stretch', preview, tools}
    else
        preview.align = 'stretch'
        body = ui.column{grow = 1, gap = 0, preview, tools}
    end
    self.gui:replaceChildren('body', {body})
    local items = {}
    for index, filter in ipairs(Photos.filters) do
        items[index] = {id = filter.id, type = 'filter', name = filter.name, thumb = Compose.thumbName(filter.id)}
    end
    self.gui:collection('filters'):setItems(items)
end

-- Changes the crop or the filter, and shows the picture with them once its preview is made.
function ComposeEdit:change(key, value)
    Compose[key] = value
    if not self.ready then
        self.gui:set('preview', {image = Compose.look()})
        return
    end
    scene.spawn(self, function()
        self:showPreview(Compose.render():await())
    end)
end

function ComposeEdit:showPreview(ready)
    if ready then
        self.ready = true
        self.gui:set('preview', {image = Compose.preview})
    end
end

return ComposeEdit
