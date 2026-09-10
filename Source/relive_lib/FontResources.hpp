#pragma once

#include <unordered_map>
#include <string>
#include "Font.hpp"

struct Font_AtlasEntry;

extern const Font_AtlasEntry sDebugFontAtlas[];
extern u8 sDebugFont[8232];

extern std::unordered_map<GlyphId, Font_AtlasEntry, GlyphIdHash> sPauseMenuFontAtlas;
extern std::unordered_map<GlyphId, Font_AtlasEntry, GlyphIdHash> sLcdFontAtlas;
