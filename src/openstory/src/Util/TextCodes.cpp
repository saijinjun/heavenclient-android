#include "TextCodes.h"

#include "Misc.h"

#include "../Data/ItemData.h"

#include <nlnx/nx.hpp>

#include <cctype>
#include <unordered_map>

namespace ms
{
	namespace TextCodes
	{
		namespace
		{
			// Parses the id part of a code. Scripts sometimes write a colon
			// after it (`#t4031172:#`, Nexon's own form) or stray text, so
			// only the leading digits count.
			bool parse_id(const std::string& payload, int32_t& id)
			{
				size_t digits = 0;

				while (digits < payload.size() && std::isdigit(static_cast<unsigned char>(payload[digits])))
					digits++;

				// More than 9 digits doesn't fit a game id (and would overflow)
				if (digits == 0 || digits > 9)
					return false;

				id = std::stoi(payload.substr(0, digits));

				return true;
			}
		}

		std::string map_name(int32_t mapid)
		{
			// Map.img groups maps by region. The region guess covers almost
			// every map, but a few regions don't follow the id ranges, so a
			// miss falls back to looking through every region once and
			// remembering the answer.
			std::string key = std::to_string(mapid);
			nl::node strings = nl::nx::string["Map.img"];
			std::string name = strings[NxHelper::Map::get_map_category(mapid)][key]["mapName"].get_string();

			if (!name.empty())
				return name;

			static std::unordered_map<int32_t, std::string> found;
			auto iter = found.find(mapid);

			if (iter != found.end())
				return iter->second;

			for (nl::node region : strings)
			{
				name = region[key]["mapName"].get_string();

				if (!name.empty())
					break;
			}

			found[mapid] = name;

			return name;
		}

		std::string name_for(char code, const std::string& payload)
		{
			int32_t id = 0;

			if (!parse_id(payload, id))
				return "";

			switch (code)
			{
			case 'm':
				return map_name(id);
			case 'p':
				// String keys carry no leading zeros, so rebuild the key from
				// the number rather than using the script's spelling.
				return nl::nx::string["Npc.img"][std::to_string(id)]["name"].get_string();
			case 'o':
				return nl::nx::string["Mob.img"][std::to_string(id)]["name"].get_string();
			case 't':
			case 'z':
				if (!ItemData::exists(id))
					return "";

				return ItemData::get(id).get_name();
			default:
				return "";
			}
		}

		std::string label_for_path(const std::string& path)
		{
			// The quest banners scripts use. 4 heads the reward list; 7 and 8
			// mark mesos and EXP lines, which already say "mesos" / "exp"
			// after the amount, so a plus sign is all they need. Pictures
			// with nothing to say in words are dropped.
			static const std::unordered_map<std::string, std::string> labels = {
				{ "UI/UIWindow.img/Quest/reward", "Reward:" },
				{ "UI/UIWindow.img/QuestIcon/4/0", "Reward:" },
				{ "UI/UIWindow.img/QuestIcon/7/0", "+" },
				{ "UI/UIWindow.img/QuestIcon/8/0", "+" }
			};

			auto iter = labels.find(path);

			return iter == labels.end() ? "" : iter->second;
		}
	}
}
