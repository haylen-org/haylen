#import "platform/apple/AppleFonts.hpp"

#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "text/GlyphTables.hpp"

namespace haylen::platform {

std::mutex& AppleFonts::lock = *new std::mutex();
std::map<std::string, CTFontDescriptorRef, std::less<>>& AppleFonts::descriptors = *new std::map<std::string, CTFontDescriptorRef, std::less<>>();

// The cascade of CoreText lists, after the font of the interface of the system, the faces that draw each script in the languages the person prefers. The first face of that order that holds the character and whose glyphs the engine draws answers, so a face whose outlines are in a table the engine does not read, such as the Chinese faces of recent versions of macOS, gives way to the next face of the cascade that holds the character. The faces of the interface have names that CoreText refuses to open again, so the descriptor of each face that answers stays for its read.
std::optional<SystemFont> AppleFonts::find(char32_t codePoint) {
    @autoreleasepool {
        UniChar units[2];
        const bool pair = CFStringGetSurrogatePairForLongCharacter(static_cast<UTF32Char>(codePoint), units);
        const CFIndex length = pair ? 2 : 1;
        if (!pair) {
            units[0] = static_cast<UniChar>(codePoint);
        }

        id base = CFBridgingRelease(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 0, nullptr));
        NSArray* languages = [NSLocale preferredLanguages];
        NSArray* cascade = CFBridgingRelease(CTFontCopyDefaultCascadeListForLanguages((__bridge CTFontRef)base, (__bridge CFArrayRef)languages));
        NSMutableArray* candidates = [NSMutableArray arrayWithObject:CFBridgingRelease(CTFontCopyFontDescriptor((__bridge CTFontRef)base))];
        [candidates addObjectsFromArray:cascade];

        for (id descriptor in candidates) {
            id font = CFBridgingRelease(CTFontCreateWithFontDescriptor((__bridge CTFontDescriptorRef)descriptor, 12.0, nullptr));
            CGGlyph glyphs[2] = {0, 0};
            if (font == nil || !CTFontGetGlyphsForCharacters((__bridge CTFontRef)font, units, glyphs, length)) {
                continue;
            }
            NSString* name = CFBridgingRelease(CTFontCopyPostScriptName((__bridge CTFontRef)font));
            if (name == nil || [name isEqualToString:@(kLastResort)] || !text::GlyphTables::isDrawable(copyTags((__bridge CTFontRef)font))) {
                continue;
            }

            const std::scoped_lock guard(lock);
            const auto [entry, added] = descriptors.try_emplace(name.UTF8String, nullptr);
            if (added) {
                entry->second = static_cast<CTFontDescriptorRef>(CFRetain((__bridge CFTypeRef)descriptor));
            }
            return SystemFont{.name = entry->first};
        }
        return std::nullopt;
    }
}

// The file holds the offset table of a font with the version of its outlines, the records of the tables in the order of their tags, and each table on a boundary of four bytes, as the OpenType format lays them out.
std::vector<std::uint8_t> AppleFonts::read(const SystemFont& font) {
    @autoreleasepool {
        CTFontDescriptorRef descriptor = nullptr;
        {
            const std::scoped_lock guard(lock);
            const auto found = descriptors.find(font.name);
            descriptor = found != descriptors.end() ? found->second : nullptr;
        }
        id face = descriptor != nullptr ? CFBridgingRelease(CTFontCreateWithFontDescriptor(descriptor, 12.0, nullptr)) : nil;
        const std::vector<std::uint32_t> tags = face != nil ? copyTags((__bridge CTFontRef)face) : std::vector<std::uint32_t>{};
        if (tags.empty()) {
            throw std::runtime_error("The system named no font \"" + font.name + "\" whose tables it hands over.");
        }

        std::vector<std::pair<std::uint32_t, NSData*>> tables;
        for (const std::uint32_t value : tags) {
            NSData* data = CFBridgingRelease(CTFontCopyTable((__bridge CTFontRef)face, value, kCTFontTableOptionNoOptions));
            if (data != nil) {
                tables.emplace_back(value, data);
            }
        }
        std::ranges::sort(tables, {}, &std::pair<std::uint32_t, NSData*>::first);

        constexpr std::size_t kHeader = 12;
        constexpr std::size_t kRecord = 16;
        const bool compact = std::ranges::any_of(tables, [](const auto& table) { return table.first == kCTFontTableCFF; });
        std::size_t size = kHeader + tables.size() * kRecord;
        for (const auto& [tag, data] : tables) {
            size += (data.length + 3) & ~std::size_t{3};
        }
        std::vector<std::uint8_t> bytes(size, 0);

        std::size_t power = 1;
        std::size_t exponent = 0;
        while (power * 2 <= tables.size()) {
            power *= 2;
            ++exponent;
        }
        writeBigEndian(bytes, 0, compact ? 0x4F54544FU : 0x00010000U, 4);
        writeBigEndian(bytes, 4, static_cast<std::uint32_t>(tables.size()), 2);
        writeBigEndian(bytes, 6, static_cast<std::uint32_t>(power * kRecord), 2);
        writeBigEndian(bytes, 8, static_cast<std::uint32_t>(exponent), 2);
        writeBigEndian(bytes, 10, static_cast<std::uint32_t>((tables.size() - power) * kRecord), 2);

        std::size_t offset = kHeader + tables.size() * kRecord;
        for (std::size_t index = 0; index < tables.size(); ++index) {
            const auto& [tag, data] = tables[index];
            std::copy_n(static_cast<const std::uint8_t*>(data.bytes), data.length, bytes.begin() + static_cast<std::ptrdiff_t>(offset));
            std::uint32_t checksum = 0;
            for (std::size_t word = offset; word < offset + data.length; word += 4) {
                checksum += (static_cast<std::uint32_t>(bytes[word]) << 24U) | (static_cast<std::uint32_t>(bytes[word + 1]) << 16U) | (static_cast<std::uint32_t>(bytes[word + 2]) << 8U) | bytes[word + 3];
            }

            const std::size_t record = kHeader + index * kRecord;
            writeBigEndian(bytes, record, tag, 4);
            writeBigEndian(bytes, record + 4, checksum, 4);
            writeBigEndian(bytes, record + 8, static_cast<std::uint32_t>(offset), 4);
            writeBigEndian(bytes, record + 12, static_cast<std::uint32_t>(data.length), 4);
            offset += (data.length + 3) & ~std::size_t{3};
        }
        return bytes;
    }
}

// CoreText lists the tags of the tables as plain integers in the array, not as objects.
std::vector<std::uint32_t> AppleFonts::copyTags(CTFontRef font) {
    std::vector<std::uint32_t> tags;
    const CFArrayRef available = CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions);
    if (available == nullptr) {
        return tags;
    }
    for (CFIndex index = 0; index < CFArrayGetCount(available); ++index) {
        tags.push_back(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(CFArrayGetValueAtIndex(available, index))));
    }
    CFRelease(available);
    return tags;
}

void AppleFonts::writeBigEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, std::size_t width) {
    for (std::size_t index = 0; index < width; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8U * (width - 1 - index)));
    }
}

} // namespace haylen::platform
