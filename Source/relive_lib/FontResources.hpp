#pragma once

#include <unordered_map>
#include <string>

struct Font_AtlasEntry;

extern const Font_AtlasEntry sDebugFontAtlas[];
extern u8 sDebugFont[8232];

extern std::unordered_map<std::string, Font_AtlasEntry> sPauseMenuFontAtlas;
extern std::unordered_map<std::string, Font_AtlasEntry> sLcdFontAtlas;
