-- The fonts of the text tests: the open-license TrueType and OpenType fonts under `content/fonts`, the bitmap fonts drawn for the project under `content/text`, and the families the tests build from them. Tests load what they draw with when they start and keep it, so the fonts leave memory with them.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local fonts = {}

-- The files with the options they load with. Lilita One bakes a wider distance field, so its outlines, glows and blurs reach further.
fonts.files = {
    crimson = {'fonts/crimson_text_regular.ttf'},
    crimsonBold = {'fonts/crimson_text_bold.ttf'},
    crimsonItalic = {'fonts/crimson_text_italic.ttf'},
    crimsonBoldItalic = {'fonts/crimson_text_bold_italic.ttf'},
    fira = {'fonts/fira_sans_regular.otf'},
    lilita = {'fonts/lilita_one_regular.ttf', {bakeSize = 64, spread = 16}},
    mono = {'fonts/space_mono_regular.ttf'},
    cjk = {'fonts/mplus_1p_regular.ttf'},
    symbols = {'fonts/noto_sans_symbols_2_regular.ttf'},
    arabic = {'fonts/noto_sans_arabic_regular.ttf'},
    hebrew = {'fonts/noto_sans_hebrew_regular.ttf'},
    devanagari = {'fonts/noto_sans_devanagari_regular.ttf'},
    thai = {'fonts/noto_sans_thai_regular.ttf'},
    pixel = {'text/haylen_pixel.fnt', {filter = 'nearest'}},
    pixelGold = {'text/haylen_pixel_gold.fnt', {filter = 'nearest'}},
}

function fonts.load(name)
    if name == 'lcd' then
        return assets.load('text/lcd_digits.png', 'gridFont', {characters = '0123456789:.-', cellWidth = 12, cellHeight = 20, advance = 13, baseline = 19, filter = 'nearest'})
    end
    local file = fonts.files[name]
    return assets.font(file[1], file[2])
end

-- Builds a family: Crimson Text with its real faces, a mono face and fallbacks for Chinese, Japanese and symbols, Fira Sans with the Noto fonts of Arabic, Hebrew, Devanagari and Thai and M PLUS 1p as fallbacks, or one face whose bold and italic are synthesized.
function fonts.family(name)
    if name == 'crimson' then
        return graphics.newFontFamily({
            regular = fonts.load('crimson'),
            bold = fonts.load('crimsonBold'),
            italic = fonts.load('crimsonItalic'),
            boldItalic = fonts.load('crimsonBoldItalic'),
            mono = fonts.load('mono'),
            fallbacks = {fonts.load('cjk'), fonts.load('symbols')},
        })
    elseif name == 'scripts' then
        return graphics.newFontFamily({
            regular = fonts.load('fira'),
            fallbacks = {fonts.load('arabic'), fonts.load('hebrew'), fonts.load('devanagari'), fonts.load('thai'), fonts.load('cjk')},
        })
    end
    return graphics.newFontFamily({regular = fonts.load(name)})
end

return fonts
