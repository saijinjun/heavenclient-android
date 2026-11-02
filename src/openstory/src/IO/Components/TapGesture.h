//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Geometry.h"

#include <cstdlib>

namespace ms
{
	// Turns press, move and release into taps for the full-screen touch
	// panels. A tap counts when the finger lifts close to where it landed,
	// so a swipe to scroll never presses whatever it started on.
	class TapGesture
	{
	public:
		// Feed every cursor event; true when a tap just finished, with `at`
		// set to where the finger landed.
		bool update(bool down, Point<int16_t> pos, Point<int16_t>& at)
		{
			if (down)
			{
				if (!pressed)
				{
					pressed = true;
					moved = false;
					start = pos;
				}
				else if (std::abs(pos.x() - start.x()) > SLOP || std::abs(pos.y() - start.y()) > SLOP)
				{
					moved = true;
				}

				return false;
			}

			if (!pressed)
				return false;

			pressed = false;

			if (moved)
				return false;

			at = start;
			return true;
		}

		// The list scrolled: this touch is a swipe, not a tap
		void cancel()
		{
			moved = true;
		}

		// The panel lost the cursor (a dialog opened over it)
		void reset()
		{
			pressed = false;
			moved = false;
		}

		bool is_pressed() const
		{
			return pressed;
		}

	private:
		static constexpr int16_t SLOP = 18;

		bool pressed = false;
		bool moved = false;
		Point<int16_t> start;
	};
}
