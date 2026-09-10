#include "stdafx.h"
#include "FontString.hpp"
#include "logger.hpp"
#include <unordered_map>

namespace
{
    const std::unordered_map<std::string_view, NamedGlyph>& NamedGlyphTable()
    {
        static const std::unordered_map<std::string_view, NamedGlyph> table = {
            {"Button_A", NamedGlyph::Button_A},
            {"Button_B", NamedGlyph::Button_B},
            {"Button_C", NamedGlyph::Button_C},
            {"Button_D", NamedGlyph::Button_D},
            {"Button_E", NamedGlyph::Button_E},
            {"Button_F", NamedGlyph::Button_F},
            {"Button_G", NamedGlyph::Button_G},
            {"Button_H", NamedGlyph::Button_H},
            {"Dpad", NamedGlyph::Dpad},
            {"Dpad_Up", NamedGlyph::Dpad_Up},
            {"Dpad_Down", NamedGlyph::Dpad_Down},
            {"Dpad_Left", NamedGlyph::Dpad_Left},
            {"Dpad_Right", NamedGlyph::Dpad_Right},
        };
        return table;
    }
} // namespace

bool NamedGlyphFromMacroName(std::string_view name, NamedGlyph& out)
{
    const auto& table = NamedGlyphTable();
    const auto it = table.find(name);
    if (it == table.end())
    {
        return false;
    }
    out = it->second;
    return true;
}

FontString::FontString(std::string_view utf8Text)
{
    SetText(utf8Text);
}

void FontString::SetText(std::string_view utf8Text)
{
    mRawText.assign(utf8Text);
    mSegments.clear();
    Parse(utf8Text);
}

void FontString::Parse(std::string_view utf8Text)
{
    std::string literal;
    size_t i = 0;
    const size_t len = utf8Text.size();

    const auto flushLiteral = [&]()
    {
        if (!literal.empty())
        {
            Segment seg;
            seg.mIsNamedGlyph = false;
            seg.mLiteralUtf8 = std::move(literal);
            mSegments.push_back(std::move(seg));
            literal.clear();
        }
    };

    while (i < len)
    {
        const char c = utf8Text[i];
        if (c == '{')
        {
            if (i + 1 < len && utf8Text[i + 1] == '{')
            {
                // "{{" escapes a literal '{'
                literal += '{';
                i += 2;
                continue;
            }

            const size_t closePos = utf8Text.find('}', i + 1);
            if (closePos == std::string_view::npos)
            {
                // Unterminated macro - never crash on bad data, just keep the rest as literal text.
                literal.append(utf8Text.substr(i));
                LOG_WARNING("FontString: unterminated '{' in \"%.*s\"", static_cast<int>(len), utf8Text.data());
                break;
            }

            const std::string_view macroName = utf8Text.substr(i + 1, closePos - i - 1);
            NamedGlyph namedGlyph = NamedGlyph::Button_A;
            if (NamedGlyphFromMacroName(macroName, namedGlyph))
            {
                flushLiteral();
                Segment seg;
                seg.mIsNamedGlyph = true;
                seg.mNamedGlyph = namedGlyph;
                mSegments.push_back(seg);
            }
            else
            {
                // Unknown macro name - fall back to literal text rather than dropping it.
                LOG_WARNING("FontString: unknown macro {%.*s}", static_cast<int>(macroName.size()), macroName.data());
                literal += '{';
                literal.append(macroName);
                literal += '}';
            }

            i = closePos + 1;
        }
        else
        {
            literal += c;
            ++i;
        }
    }

    flushLiteral();
}
