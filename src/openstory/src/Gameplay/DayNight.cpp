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
#include "DayNight.h"

#include <algorithm>

namespace ms
{
	namespace DayNight
	{
		namespace
		{
			// Where each phase starts, in seconds into the cycle.
			constexpr int64_t DUSK_START = 60 * 60;
			constexpr int64_t NIGHT_START = 70 * 60;
			constexpr int64_t DAWN_START = 110 * 60;

			constexpr std::array<float, 3> DAY_TINT = { 1.0f, 1.0f, 1.0f };
			// Warm orange light, a little dimmer.
			constexpr std::array<float, 3> DUSK_TINT = { 1.0f, 0.80f, 0.66f };
			// Dark blue, but still easy to play in.
			constexpr std::array<float, 3> NIGHT_TINT = { 0.52f, 0.60f, 0.86f };

			int64_t seconds_into_cycle(int64_t unix_seconds)
			{
				int64_t t = unix_seconds % CYCLE_SECONDS;
				return t < 0 ? t + CYCLE_SECONDS : t;
			}

			std::array<float, 3> mix(const std::array<float, 3>& a, const std::array<float, 3>& b, float t)
			{
				// Ease in and out, so the change is hard to notice moment to moment.
				t = std::clamp(t, 0.0f, 1.0f);
				t = t * t * (3.0f - 2.0f * t);
				return { a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t };
			}
		}

		Phase phase(int64_t unix_seconds)
		{
			int64_t t = seconds_into_cycle(unix_seconds);

			if (t < DUSK_START)
				return Phase::DAY;
			if (t < NIGHT_START)
				return Phase::DUSK;
			if (t < DAWN_START)
				return Phase::NIGHT;
			return Phase::DAWN;
		}

		std::array<float, 3> tint(int64_t unix_seconds)
		{
			int64_t t = seconds_into_cycle(unix_seconds);
			float half_dusk = (NIGHT_START - DUSK_START) / 2.0f;

			switch (phase(unix_seconds))
			{
			case Phase::DAY:
				return DAY_TINT;
			case Phase::DUSK:
			{
				// Day to the warm light, then the warm light to night.
				float into = static_cast<float>(t - DUSK_START);
				if (into < half_dusk)
					return mix(DAY_TINT, DUSK_TINT, into / half_dusk);
				return mix(DUSK_TINT, NIGHT_TINT, (into - half_dusk) / half_dusk);
			}
			case Phase::NIGHT:
				return NIGHT_TINT;
			case Phase::DAWN:
			default:
			{
				float into = static_cast<float>(t - DAWN_START);
				float half_dawn = (CYCLE_SECONDS - DAWN_START) / 2.0f;
				if (into < half_dawn)
					return mix(NIGHT_TINT, DUSK_TINT, into / half_dawn);
				return mix(DUSK_TINT, DAY_TINT, (into - half_dawn) / half_dawn);
			}
			}
		}
	}
}
