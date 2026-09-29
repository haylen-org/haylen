-- The fonts and font families of the sample, loaded on first use and shared by every test.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local fonts = {}

-- TrueType and OpenType files with the options they load with. Lilita One bakes a wider distance field, so its outlines, glows and blurs reach further.
local kFiles = {
    crimson = {'fonts/crimson_text_regular.ttf'},
    crimsonBold = {'fonts/crimson_text_bold.ttf'},
    crimsonItalic = {'fonts/crimson_text_italic.ttf'},
    crimsonBoldItalic = {'fonts/crimson_text_bold_italic.ttf'},
    fira = {'fonts/fira_sans_regular.otf'},
    lilita = {'fonts/lilita_one_regular.ttf', {bakeSize = 64, spread = 16}},
    mono = {'fonts/space_mono_regular.ttf'},
    cjk = {'fonts/mplus_1p_regular.ttf'},
    symbols = {'fonts/noto_sans_symbols_2_regular.ttf'},
    pixel = {'fonts/haylen_pixel.fnt', {filter = 'nearest'}},
    pixelGold = {'fonts/haylen_pixel_gold.fnt', {filter = 'nearest'}},
}

local loaded = {}
local families = {}

function fonts.get(name)
    if loaded[name] == nil then
        if name == 'lcd' then
            loaded[name] = assets.load('fonts/lcd_digits.png', 'gridFont', {characters = '0123456789:.-', cellWidth = 12, cellHeight = 20, advance = 13, baseline = 19, filter = 'nearest'})
        else
            local file = kFiles[name]
            loaded[name] = assets.font(file[1], file[2])
        end
    end
    return loaded[name]
end

-- The families of the sample: Crimson Text with its real faces, a mono face and fallbacks for CJK and symbols, and families of one face whose bold and italic are synthesized.
function fonts.family(name)
    if families[name] == nil then
        if name == 'crimson' then
            families[name] = graphics.newFontFamily({
                regular = fonts.get('crimson'),
                bold = fonts.get('crimsonBold'),
                italic = fonts.get('crimsonItalic'),
                boldItalic = fonts.get('crimsonBoldItalic'),
                mono = fonts.get('mono'),
                fallback = {fonts.get('cjk'), fonts.get('symbols')},
            })
        else
            families[name] = graphics.newFontFamily({regular = fonts.get(name)})
        end
    end
    return families[name]
end

return fonts
