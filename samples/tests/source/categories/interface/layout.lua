-- The layout the interface tests share: rows of columns that start at the top, and titled cards that group the components of one kind.
local ui = require('haylen.ui')

local layout = {}

-- A row of columns that fills the test, each starting at the top unless it sets its own `align`.
function layout.columns(node)
    for _, child in ipairs(node) do
        child.align = child.align or 'start'
    end
    node.gap = node.gap or 24
    node.grow = node.grow or 1
    return ui.row(node)
end

-- A card with a title over the components in the array part of `node`.
function layout.section(title, node)
    table.insert(node, 1, ui.sectionTitle{text = title})
    node.gap = node.gap or 12
    return ui.card(node)
end

return layout
