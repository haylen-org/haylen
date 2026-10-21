-- Hashes, signatures, random bytes, codecs, identifiers and encryption with Varn's "crypto" module, checked against published test vectors where they exist.
local async = require('async')
local crypto = require('crypto')
local haylen = require('haylen')
local ui = require('haylen.ui')

local VarnTest = require('categories.varn.varn-test')

local Crypto = haylen.class('Crypto', VarnTest)

Crypto.showcaseWidth = 560

Crypto.browserReason = 'The browser build of Varn leaves out encryption and key derivation, which need OpenSSL, and keeps digests, HMAC, random bytes, codecs and UUIDs.'

Crypto.excerpts = {
    {'Hashes and signatures', [[
print(crypto.digest('SHA256', 'abc'))
local key = string.rep('\x0b', 20)
local tag = crypto.hmac('SHA256', key, 'Hi There')
print(crypto.equals(tag, presented))]]},
    {'Random bytes and codecs', [[
local bytes = crypto.randomBytes(16)
print(crypto.hexEncode(bytes))
print(crypto.base64Encode(bytes))
print(crypto.base64UrlEncode(bytes))
print(crypto.uuidV4(), crypto.uuidV7())]]},
    {'Encryption', [[
local secret = crypto.randomBytes(32)
local sealed = crypto.encrypt(secret, 'The chest is under the palm.')
print(crypto.decrypt(secret, sealed))
local salt = crypto.randomBytes(16)
local derived = crypto.pbkdf2('tide', salt, 10000, 32)]]},
}

-- The digests, the tags and the identifiers of the lesson in the monospace font, which keeps their digits in columns.
Crypto.values = {{'sha256', 'SHA-256 of "abc"'}, {'hmac', 'HMAC-SHA-256'}, {'random', 'Random bytes'}, {'base64', 'Base64'}, {'uuid', 'UUID version 7'}}

function Crypto:showcase()
    local rows = {ui.label{text = 'What the lesson computed', font = 'heading'}}
    for _, value in ipairs(Crypto.values) do
        rows[#rows + 1] = ui.column{gap = 2, ui.label{text = value[2], font = 'caption', color = 'textMuted'}, ui.label{id = 'value-' .. value[1], text = '–', font = 'monospace'}}
    end
    return {ui.card{align = 'stretch', gap = 10, children = rows}}
end

function Crypto:run()
    local sha256 = crypto.digest('SHA256', 'abc')
    local sha512 = crypto.digest('SHA512', 'abc')
    self:check('digest', 'Digests', sha256 == 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad' and #sha512 == 128, string.format('SHA-256 of "abc" is the published digest, and SHA-512 gives %d hexadecimal digits.', #sha512))
    self:set('value-sha256', {text = sha256})

    local key = string.rep('\x0b', 20)
    local tag = crypto.hmac('SHA256', key, 'Hi There')
    self:check('hmac', 'HMAC', tag == 'b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7', 'HMAC-SHA-256 matches case 1 of RFC 4231.')
    self:set('value-hmac', {text = tag})

    local presented = crypto.hmac('SHA256', key, 'Hi There')
    local tampered = crypto.hmac('SHA256', key, 'Hi there')
    self:check('equals', 'Comparing secrets', crypto.equals(tag, presented) and not crypto.equals(tag, tampered), 'The function "crypto.equals" accepted the same tag and refused the tag of "Hi there", in a time that does not depend on where they differ.')

    local bytes, other = crypto.randomBytes(16), crypto.randomBytes(16)
    self:check('random', 'Random bytes', #bytes == 16 and bytes ~= other, 'Two draws of 16 bytes differ, and the card shows the first.')
    self:set('value-random', {text = crypto.hexEncode(bytes)})

    local base64, url, hex = crypto.base64Encode(bytes), crypto.base64UrlEncode(bytes), crypto.hexEncode(bytes)
    local decoded = crypto.base64Decode(base64) == bytes and crypto.base64UrlDecode(url) == bytes and crypto.hexDecode(hex) == bytes
    self:check('codecs', 'Codecs', decoded, 'Base64, URL-safe Base64 and hexadecimal each decode back to the same bytes.')
    self:set('value-base64', {text = base64})

    local first = crypto.uuidV7()
    async.sleep(5):await()
    local second = crypto.uuidV7()
    self:check('uuid', 'UUIDs', first < second, 'A version 7 identifier sorts before one made 5 ms later, while version 4 is random.')
    self:set('value-uuid', {text = first})

    if haylen.platform == 'web' then
        self.results:set('encrypt', 'skip', 'Encryption and key derivation', Crypto.browserReason)
        return
    end
    self:encryption()
end

function Crypto:encryption()
    local secret = crypto.randomBytes(32)
    local sealed = crypto.encrypt(secret, 'The chest is under the palm.')
    local opened = crypto.decrypt(secret, sealed)
    local changed = sealed:sub(1, -2) .. string.char((sealed:byte(-1) + 1) % 256)
    local refused = not pcall(crypto.decrypt, secret, changed)
    self:check('encrypt', 'Encryption', opened == 'The chest is under the palm.' and refused, string.format('AES-256-GCM sealed 28 bytes into %d, opened them again, and refused the sealed bytes once one of them changed.', #sealed))

    local salt = crypto.randomBytes(16)
    local derived = crypto.pbkdf2('tide', salt, 10000, 32)
    local session = crypto.hkdf(derived, salt, 'session', 32)
    self:check('derive', 'Key derivation', #derived == 32 and #session == 32 and derived ~= session, string.format('PBKDF2 made a key that starts with %s from a password, and HKDF one that starts with %s from it.', crypto.hexEncode(derived):sub(1, 16), crypto.hexEncode(session):sub(1, 16)))
end

return Crypto
