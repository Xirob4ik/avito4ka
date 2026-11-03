#include "Fonts.h"

#include <BinaryData.h>

namespace clipline::ui::fonts
{
    namespace
    {
        FontLibrary* current = nullptr;

        juce::Typeface::Ptr load (const char* data, int size)
        {
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        }

        juce::Font make (const juce::Typeface::Ptr& face, float height, float tracking)
        {
            if (face == nullptr)
                return juce::Font (juce::FontOptions (height));

            return juce::Font (juce::FontOptions().withTypeface (face).withHeight (height).withKerningFactor (tracking));
        }
    }

    FontLibrary::FontLibrary()
        : regular     (load (BinaryData::GeistRegular_ttf,     BinaryData::GeistRegular_ttfSize)),
          medium      (load (BinaryData::GeistMedium_ttf,      BinaryData::GeistMedium_ttfSize)),
          semiBold    (load (BinaryData::GeistSemiBold_ttf,    BinaryData::GeistSemiBold_ttfSize)),
          monoRegular (load (BinaryData::GeistMonoRegular_ttf, BinaryData::GeistMonoRegular_ttfSize)),
          monoMedium  (load (BinaryData::GeistMonoMedium_ttf,  BinaryData::GeistMonoMedium_ttfSize))
    {
        current = this;
    }

    FontLibrary::~FontLibrary()
    {
        if (current == this)
            current = nullptr;
    }

    juce::Font sans (float height, Weight weight, float tracking)
    {
        if (current == nullptr)
            return juce::Font (juce::FontOptions (height));

        const auto& face = weight == Weight::Regular ? current->regular
                         : weight == Weight::Medium  ? current->medium
                                                     : current->semiBold;
        return make (face, height, tracking);
    }

    juce::Font mono (float height, bool medium)
    {
        if (current == nullptr)
            return juce::Font (juce::FontOptions (height));

        return make (medium ? current->monoMedium : current->monoRegular, height, 0.0f);
    }

    float textWidth (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }
}
