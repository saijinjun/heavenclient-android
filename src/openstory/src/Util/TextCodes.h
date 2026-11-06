#pragma once

#include <cstdint>
#include <string>

namespace ms
{
	// Server and WZ text embeds game data as `#X<id>#` codes (#m map,
	// #p NPC, #o mob, #t / #z item). Every window that shows such text
	// resolves them here, so a name never falls back to the raw id in
	// one window while another one shows it correctly.
	namespace TextCodes
	{
		// The map's name from String Map.img, or empty when the game
		// files don't have it.
		std::string map_name(int32_t mapid);

		// The name a name code stands for, e.g. ('m', "100000000") gives
		// "Henesys". Empty when the code isn't a name code, the id isn't a
		// number, or the game files don't know it.
		std::string name_for(char code, const std::string& payload);

		// Words for the UI pictures scripts embed by path, such as
		// `#fUI/UIWindow.img/QuestIcon/4/0#` (the "Reward" banner). The
		// text layout only reserves a small icon slot, which these wide
		// banners can't fit, so they read better as words. Empty for
		// paths with no meaning worth showing.
		std::string label_for_path(const std::string& path);
	}
}
