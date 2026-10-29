//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Geometry.h"

// Panels and buttons drawn in code where the NX art doesn't fit, styled
// after the classic MapleStory windows: parchment with brown borders and
// orange buttons.
namespace ms
{
	namespace classic
	{
		inline void box(int16_t x, int16_t y, int16_t w, int16_t h, Color::Name color, float alpha = 1.0f)
		{
			if (w > 0 && h > 0)
				ColorBox(w, h, color, alpha).draw(DrawArgument(Point<int16_t>(x, y)));
		}

		inline void frame(int16_t x, int16_t y, int16_t w, int16_t h, Color::Name color, float alpha = 1.0f)
		{
			box(x + 1, y, w - 2, 1, color, alpha);
			box(x + 1, y + h - 1, w - 2, 1, color, alpha);
			box(x, y + 1, 1, h - 2, color, alpha);
			box(x + w - 1, y + 1, 1, h - 2, color, alpha);
		}

		// A window: soft shadow, brown outline, parchment with a shaded foot
		inline void panel(Point<int16_t> tl, int16_t w, int16_t h)
		{
			int16_t x = tl.x();
			int16_t y = tl.y();

			box(x + 2, y + 3, w, h, Color::Name::BLACK, 0.25f);
			box(x + 1, y + 1, w - 2, h - 2, Color::Name::PARCHMENT);
			box(x + 1, y + h - 8, w - 2, 7, Color::Name::PARCHMENT_DARK);
			frame(x, y, w, h, Color::Name::MAPLE_BORDER);
			frame(x + 2, y + 2, w - 4, h - 4, Color::Name::WHITE, 0.6f);
		}

		// An orange button; `lit` is the hovered or pressed look
		inline void button(Point<int16_t> tl, int16_t w, int16_t h, bool lit = false)
		{
			int16_t x = tl.x();
			int16_t y = tl.y();

			box(x + 1, y + 2, w, h, Color::Name::BLACK, 0.25f);
			box(x + 1, y + 1, w - 2, h - 2, lit ? Color::Name::MAPLE_ORANGE_LIGHT : Color::Name::MAPLE_ORANGE);
			box(x + 2, y + 2, w - 4, (h - 4) / 2, Color::Name::WHITE, 0.25f);
			frame(x, y, w, h, Color::Name::MAPLE_BORDER);
		}

		// A list cell; the selected one is lit gold
		inline void cell(Point<int16_t> tl, int16_t w, int16_t h, bool selected)
		{
			int16_t x = tl.x();
			int16_t y = tl.y();

			box(x + 1, y + 1, w - 2, h - 2, selected ? Color::Name::HUD_EXP : Color::Name::PARCHMENT_DARK);
			box(x + 1, y + 1, w - 2, 1, Color::Name::WHITE, 0.7f);
			frame(x, y, w, h, selected ? Color::Name::MAPLE_ORANGE : Color::Name::MAPLE_BORDER);
		}
	}
}
