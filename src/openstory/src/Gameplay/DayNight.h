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
#pragma once

#include <array>
#include <cstdint>

namespace ms
{
	// The time of day, on a two-hour cycle shared with the server: 60
	// minutes of day, 10 of dusk, 40 of night and 10 of dawn. Both sides
	// count from the Unix epoch, so every player sees the same sky.
	namespace DayNight
	{
		enum class Phase : uint8_t
		{
			DAY,
			DUSK,
			NIGHT,
			DAWN
		};

		constexpr int64_t CYCLE_SECONDS = 2 * 60 * 60;

		Phase phase(int64_t unix_seconds);

		// The color to multiply the world by, blending smoothly across dusk
		// and dawn.
		std::array<float, 3> tint(int64_t unix_seconds);
	}
}
