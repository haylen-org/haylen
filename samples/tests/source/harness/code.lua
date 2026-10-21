-- The markup of the code card of a test: the Lua code with its keywords, strings, numbers, comments and the names of the functions it calls in colors of the active theme. Every line of the code is a paragraph whose wrapped lines hang under its start, and a line never breaks inside a string or the indentation of a line.
local ui = require('haylen.ui')

local code = {}

code.keywords = {}
for word in ('and break do else elseif end for function goto if in local not or repeat return then until while'):gmatch('%a+') do
    code.keywords[word] = true
end
code.literals = {['true'] = true, ['false'] = true, ['nil'] = true}

-- The theme color roles of the kinds of tokens.
code.roles = {keyword = 'accentText', string = 'successText', number = 'warningText', literal = 'warningText', comment = 'textMuted', call = 'informationText'}

-- A space that never ends a line, which keeps strings and indentation whole.
code.joiner = '\u{A0}'

-- Wrapped lines start this many columns past the indentation of their line, and a level of the hanging indent of rich text spans one and a half ems, which Space Mono fills with about two and a half columns.
code.hangColumns = 4
code.columnsPerLevel = 1.5 / 0.612

-- Returns the markup of the source in the colors of the active theme.
function code.markup(source)
    local colors = {}
    for kind, role in pairs(code.roles) do
        colors[kind] = ui.themeColor(role):toHex()
    end
    local lines = {}
    for _, line in ipairs(code.tokenize(source)) do
        local parts = {}
        for _, token in ipairs(line.tokens) do
            local text = code.escape(token.text, token.kind == 'string' or token.kind == 'indent')
            parts[#parts + 1] = colors[token.kind] and string.format('[color=%s]%s[/color]', colors[token.kind], text) or text
        end
        local hang = (line.indent + code.hangColumns) / code.columnsPerLevel
        lines[#lines + 1] = string.format('[p hang=%.2f]%s[/p]', hang, table.concat(parts))
    end
    return table.concat(lines)
end

-- Writes the brackets of the code as the tags of rich text, and joins the spaces of a string or of the indentation so no line breaks there.
function code.escape(text, joined)
    text = text:gsub('[%[%]]', {['['] = '[lb]', [']'] = '[rb]'})
    if joined then
        text = text:gsub(' ', code.joiner)
    end
    return text
end

-- Splits the source into lines of tokens, each with its kind, where a string or a comment that spans lines becomes a token on each of them.
function code.tokenize(source)
    local lines = {{tokens = {}, indent = 0}}
    local function add(kind, text)
        local first = true
        for piece in (text .. '\n'):gmatch('(.-)\n') do
            if not first then
                lines[#lines + 1] = {tokens = {}, indent = 0}
            end
            first = false
            if piece ~= '' then
                local tokens = lines[#lines].tokens
                tokens[#tokens + 1] = {kind = kind, text = piece}
            end
        end
    end

    local position = 1
    local length = #source
    while position <= length do
        local current = lines[#lines]
        local rest = source:sub(position)
        local indent = #current.tokens == 0 and rest:match('^ +')
        local long = rest:match('^%-%-%[(=*)%[') or rest:match('^%[(=*)%[')
        if indent then
            current.indent = #indent
            add('indent', indent)
            position = position + #indent
        elseif long then
            local opening = rest:match('^%-%-') and '--[' .. long .. '[' or '[' .. long .. '['
            local closing = ']' .. long .. ']'
            local finish = select(2, source:find(closing, position + #opening, true)) or length
            add(opening:sub(1, 2) == '--' and 'comment' or 'string', source:sub(position, finish))
            position = finish + 1
        elseif rest:match('^%-%-') then
            local comment = rest:match('^[^\n]*')
            add('comment', comment)
            position = position + #comment
        elseif rest:match('^[\'"]') then
            local quote = rest:sub(1, 1)
            local finish = position + 1
            while finish <= length and source:sub(finish, finish) ~= quote and source:sub(finish, finish) ~= '\n' do
                finish = finish + (source:sub(finish, finish) == '\\' and 2 or 1)
            end
            add('string', source:sub(position, math.min(finish, length)))
            position = finish + 1
        elseif rest:match('^0[xX]%x') or rest:match('^%d') or rest:match('^%.%d') then
            local number = rest:match('^0[xX][%x%.]+[pP]?[%+%-]?%d*') or rest:match('^%d*%.?%d*[eE][%+%-]?%d+') or rest:match('^%d*%.?%d+') or rest:match('^%d+')
            add('number', number)
            position = position + #number
        elseif rest:match('^[%a_]') then
            local word = rest:match('^[%a_][%w_]*')
            local after = source:sub(position + #word):match('^%s*(.?)')
            local named = source:sub(1, position - 1):match('function%s+$') ~= nil
            local kind = code.keywords[word] and 'keyword' or code.literals[word] and 'literal' or (after == '(' or after == '{' or named) and 'call' or nil
            add(kind, word)
            position = position + #word
        else
            local other = rest:match('^[^%w_\'"%-%[\n]+') or rest:sub(1, 1)
            add(nil, other)
            position = position + #other
        end
    end
    return lines
end

return code
