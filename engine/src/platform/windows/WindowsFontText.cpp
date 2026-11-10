#include "platform/windows/WindowsFontText.hpp"

#include <utility>

namespace haylen::platform {

WindowsFontText::WindowsFontText(std::wstring_view mappedText, std::wstring mappedLocale) : text(mappedText), locale(std::move(mappedLocale)) {}

HRESULT STDMETHODCALLTYPE WindowsFontText::QueryInterface(REFIID id, void** object) {
    if (id == __uuidof(IUnknown) || id == __uuidof(IDWriteTextAnalysisSource)) {
        *object = static_cast<IDWriteTextAnalysisSource*>(this);
        return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WindowsFontText::AddRef() {
    return 1;
}

ULONG STDMETHODCALLTYPE WindowsFontText::Release() {
    return 1;
}

HRESULT STDMETHODCALLTYPE WindowsFontText::GetTextAtPosition(UINT32 position, const WCHAR** textString, UINT32* textLength) {
    const bool inside = position < text.size();
    *textString = inside ? text.data() + position : nullptr;
    *textLength = inside ? static_cast<UINT32>(text.size() - position) : 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsFontText::GetTextBeforePosition(UINT32 position, const WCHAR** textString, UINT32* textLength) {
    const bool inside = position > 0 && position <= text.size();
    *textString = inside ? text.data() : nullptr;
    *textLength = inside ? position : 0;
    return S_OK;
}

DWRITE_READING_DIRECTION STDMETHODCALLTYPE WindowsFontText::GetParagraphReadingDirection() {
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}

HRESULT STDMETHODCALLTYPE WindowsFontText::GetLocaleName(UINT32 position, UINT32* textLength, const WCHAR** localeName) {
    *localeName = locale.c_str();
    *textLength = position < text.size() ? static_cast<UINT32>(text.size() - position) : 0;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsFontText::GetNumberSubstitution(UINT32 position, UINT32* textLength, IDWriteNumberSubstitution** substitution) {
    *substitution = nullptr;
    *textLength = position < text.size() ? static_cast<UINT32>(text.size() - position) : 0;
    return S_OK;
}

} // namespace haylen::platform
