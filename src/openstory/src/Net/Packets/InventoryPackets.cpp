//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#include "InventoryPackets.h"

#include <nlnx/nx.hpp>

#include <string>

namespace ms
{
	namespace
	{
		// Boxes and pouches that hand out a random item list their prizes
		// under "reward" in the item data; the server opens them with its
		// item reward handler, and its use handler does nothing with them.
		bool has_reward(int32_t itemid)
		{
			std::string strid = "0" + std::to_string(itemid);
			std::string img = strid.substr(0, 4) + ".img";

			return nl::nx::item["Consume"][img][strid]["reward"].size() > 0;
		}
	}

	int16_t UseItemByTypePacket::opcode_for(int32_t itemid)
	{
		int32_t prefix = itemid / 10000;

		if (prefix == 202 && has_reward(itemid))
			return OutPacket::Opcode::USE_ITEM_REWARD;

		// Potions, food, return scrolls, cures and transformation
		// potions: the server's use handler
		if (prefix <= 203 || prefix == 205 || prefix == 221)
			return OutPacket::Opcode::USE_ITEM;

		switch (prefix)
		{
		case 210:
			return OutPacket::Opcode::USE_SUMMON_BAG;
		case 226:
			return OutPacket::Opcode::USE_MOUNT_FOOD;
		case 228:
		case 229:
			return OutPacket::Opcode::USE_SKILL_BOOK;
		case 237:
			return OutPacket::Opcode::USE_SOLOMON_ITEM;
		case 243:
			return OutPacket::Opcode::SCRIPTED_ITEM;
		default:
			return 0;
		}
	}
}
