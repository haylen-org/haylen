#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dwrite_2.h>

#include <string>
#include <string_view>

namespace haylen::platform {

// Hands DirectWrite the text whose font it maps, in one locale and from left to right. It lives on the stack of the lookup, which outlives every call DirectWrite makes into it, so it counts no references.
class WindowsFontText final : public IDWriteTextAnalysisSource {
  public:
    WindowsFontText(std::wstring_view mappedText, std::wstring mappedLocale);

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position, const WCHAR** textString, UINT32* textLength) override;
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position, const WCHAR** textString, UINT32* textLength) override;
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override;
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 position, UINT32* textLength, const WCHAR** localeName) override;
    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 position, UINT32* textLength, IDWriteNumberSubstitution** substitution) override;

  private:
    std::wstring_view text;
    std::wstring locale;
};

} // namespace haylen::platform
