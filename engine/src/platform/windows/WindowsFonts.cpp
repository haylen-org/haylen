#include "platform/windows/WindowsFonts.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dwrite_2.h>
#include <wrl/client.h>

#include <string>

#include "platform/windows/WindowsFontText.hpp"
#include "platform/windows/WindowsText.hpp"

namespace haylen::platform {

using Microsoft::WRL::ComPtr;

// The fallback of the system maps the character from Segoe UI, the font of the interface of Windows, to the face that draws it, whose only file is a local file of the system fonts.
std::optional<SystemFont> WindowsFonts::find(char32_t codePoint) {
    ComPtr<IDWriteFactory2> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2), reinterpret_cast<IUnknown**>(factory.GetAddressOf())))) {
        return std::nullopt;
    }
    ComPtr<IDWriteFontFallback> fallback;
    ComPtr<IDWriteFontCollection> collection;
    if (FAILED(factory->GetSystemFontFallback(&fallback)) || FAILED(factory->GetSystemFontCollection(&collection))) {
        return std::nullopt;
    }

    std::wstring text;
    if (codePoint > 0xFFFFU) {
        const char32_t shifted = codePoint - 0x10000U;
        text = {static_cast<wchar_t>(0xD800U + (shifted >> 10U)), static_cast<wchar_t>(0xDC00U + (shifted & 0x3FFU))};
    } else {
        text = {static_cast<wchar_t>(codePoint)};
    }
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
    GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH);
    WindowsFontText source(text, locale);
    UINT32 mapped = 0;
    ComPtr<IDWriteFont> font;
    FLOAT scale = 1.0F;
    if (FAILED(fallback->MapCharacters(&source, 0, static_cast<UINT32>(text.size()), collection.Get(), L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped, &font, &scale)) || font == nullptr) {
        return std::nullopt;
    }

    ComPtr<IDWriteFontFace> face;
    UINT32 files = 1;
    ComPtr<IDWriteFontFile> file;
    if (FAILED(font->CreateFontFace(&face)) || FAILED(face->GetFiles(&files, &file)) || files != 1) {
        return std::nullopt;
    }
    const void* key = nullptr;
    UINT32 keySize = 0;
    ComPtr<IDWriteFontFileLoader> loader;
    ComPtr<IDWriteLocalFontFileLoader> local;
    if (FAILED(file->GetReferenceKey(&key, &keySize)) || FAILED(file->GetLoader(&loader)) || FAILED(loader.As(&local))) {
        return std::nullopt;
    }
    UINT32 pathLength = 0;
    if (FAILED(local->GetFilePathLengthFromKey(key, keySize, &pathLength))) {
        return std::nullopt;
    }
    std::wstring path(pathLength + 1, L'\0');
    if (FAILED(local->GetFilePathFromKey(key, keySize, path.data(), pathLength + 1))) {
        return std::nullopt;
    }
    path.resize(pathLength);

    const int index = static_cast<int>(face->GetIndex());
    return SystemFont{.name = WindowsText::toUtf8(path) + "#" + std::to_string(index), .file = std::filesystem::path(path), .face = index};
}

} // namespace haylen::platform
