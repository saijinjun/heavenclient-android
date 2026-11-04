//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Licensed under the GNU Affero General Public License v3 or later.			//
//////////////////////////////////////////////////////////////////////////////////
#include "Haptics.h"

#include "../../platform/shared/PlatformConfig.h"
#include "../Configuration.h"

#if defined(PLATFORM_SDL)
#include "Window.h"

#include <SDL.h>
#endif

namespace ms
{
	namespace Haptics
	{
		Pulse pulse_for(Event event)
		{
			switch (event)
			{
			case Event::LEVEL_UP:
				return { 0.8f, 260 };
			case Event::DEATH:
				return { 1.0f, 500 };
			case Event::BIG_HIT:
				return { 0.6f, 80 };
			case Event::SCROLL_SUCCESS:
				return { 0.5f, 120 };
			case Event::SCROLL_DESTROYED:
				return { 1.0f, 350 };
			}

			return { 0.0f, 0 };
		}

		bool is_big_hit(int32_t damage, int32_t max_hp)
		{
			if (damage <= 0 || max_hp <= 0)
				return false;

			// 15% of max HP in one hit
			return static_cast<int64_t>(damage) * 100 >= static_cast<int64_t>(max_hp) * 15;
		}

		void play(Event event)
		{
#if defined(PLATFORM_SDL)
			if (!Setting<EventVibration>::get().load())
				return;

			// Mobs can hit several times a second; don't buzz nonstop.
			static uint32_t last_hit = 0;

			if (event == Event::BIG_HIT)
			{
				uint32_t now = SDL_GetTicks();

				if (last_hit != 0 && now - last_hit < 400)
					return;

				last_hit = now;
			}

			Pulse pulse = pulse_for(event);
			Window::get().rumble(pulse.strength, pulse.ms);
#else
			(void)event;
#endif
		}
	}
}
