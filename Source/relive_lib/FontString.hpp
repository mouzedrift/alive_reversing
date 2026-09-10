#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "Font.hpp"

// A parsed, UTF-8 piece of atlas-drawn text. Text is parsed once (in SetText/the constructor)
// into a sequence of literal-text runs and {MacroName} named-glyph references, instead of being
// re-scanned on every DrawString/MeasureTextWidth/SliceText call.
//
// Macro syntax: "{Button_A}" resolves to NamedGlyph::Button_A (see NamedGlyphFromMacroName for
// the full set of names); "{{" is an escape for a literal '{'. An unknown "{Whatever}" or an
// unterminated '{' is never fatal - it falls back to literal text (with a logged warning) so a
// bad string degrades gracefully instead of breaking rendering.
class FontString final
{
public:
    struct Segment final
    {
        bool mIsNamedGlyph = false;
        std::string mLiteralUtf8; // valid when !mIsNamedGlyph
        NamedGlyph mNamedGlyph = NamedGlyph::Button_A; // valid when mIsNamedGlyph
    };

    FontString() = default;
    explicit FontString(std::string_view utf8Text);

    void SetText(std::string_view utf8Text);

    const std::vector<Segment>& Segments() const { return mSegments; }
    const std::string& RawText() const { return mRawText; }

private:
    void Parse(std::string_view utf8Text);

    std::string mRawText;
    std::vector<Segment> mSegments;
};

// Looks up a macro name (without the surrounding braces), e.g. "Button_A" -> NamedGlyph::Button_A.
// Returns false if the name isn't recognised.
bool NamedGlyphFromMacroName(std::string_view name, NamedGlyph& out);
