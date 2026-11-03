//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Licensed under the GNU Affero General Public License v3 or later.			//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

namespace ms
{
	// Phone vibration on game events (SDL builds). Each event has its own
	// strength and length so they feel different in the hand. Off with the
	// EventVibration setting; touch-button taps are TouchVibration.
	// A no-op on desktop builds without a rumble device.
	namespace Haptics
	{
		enum class Event : uint8_t
		{
			LEVEL_UP,
			DEATH,
			BIG_HIT,
			SCROLL_SUCCESS,
			SCROLL_DESTROYED
		};

		struct Pulse
		{
			float strength;
			uint32_t ms;
		};

		// The pulse an event plays. Exposed for tests.
		Pulse pulse_for(Event event);

		// A hit counts as big at this share of max HP or more.
		bool is_big_hit(int32_t damage, int32_t max_hp);

		void play(Event event);
	}
}
