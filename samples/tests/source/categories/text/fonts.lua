-- The fonts of the text tests: the open-license TrueType and OpenType fonts under `content/fonts`, the bitmap fonts drawn for the project under `content/text`, and the families the tests build from them. Tests load what they draw with when they start and keep it, so the fonts leave memory with them.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')

local fonts = {}

-- The files with the options they load with. Lilita One bakes a wider distance field, so its outlines, glows and blurs reach further.
fonts.files = {
    crimson = {'fonts/crimson-text-regular.ttf'},
    crimsonBold = {'fonts/crimson-text-bold.ttf'},
    crimsonItalic = {'fonts/crimson-text-italic.ttf'},
    crimsonBoldItalic = {'fonts/crimson-text-bold-italic.ttf'},
    fira = {'fonts/fira-sans-regular.otf'},
    lilita = {'fonts/lilita-one-regular.ttf', {bakeSize = 64, spread = 16}},
    mono = {'fonts/space-mono-regular.ttf'},
    cjk = {'fonts/mplus-1p-regular.ttf'},
    symbols = {'fonts/noto-sans-symbols-2-regular.ttf'},
    arabic = {'fonts/noto-sans-arabic-regular.ttf'},
    hebrew = {'fonts/noto-sans-hebrew-regular.ttf'},
    devanagari = {'fonts/noto-sans-devanagari-regular.ttf'},
    thai = {'fonts/noto-sans-thai-regular.ttf'},
    pixel = {'text/haylen-pixel.fnt', {filter = 'nearest'}},
    pixelGold = {'text/haylen-pixel-gold.fnt', {filter = 'nearest'}},
}

function fonts.load(name)
    if name == 'lcd' then
        return assets.load('text/lcd-digits.png', 'gridFont', {characters = '0123456789:.-', cellWidth = 12, cellHeight = 20, advance = 13, baseline = 19, filter = 'nearest'})
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
