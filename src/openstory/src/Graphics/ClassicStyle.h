//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Geometry.h"
#include "Texture.h"

#include <algorithm>
#include <string>

// Panels and buttons drawn in code where the NX art doesn't fit, in the same
// navy glass and gold as the HUD. Bundled art (ui/*.png) is used where the
// APK has it; plain boxes in the same colors stand in otherwise.
namespace ms
{
	namespace classic
	{
		struct Art
		{
			Texture window[9];
			Texture button[3];
			Texture button_lit[3];
			Texture cell[3];
			Texture cell_lit[3];
			Texture input[3];
			Texture close;

			Art()
			{
				const char* rows[3] = { "t", "m", "b" };
				const char* cols[3] = { "l", "c", "r" };
				const char* ends[3] = { "l", "m", "r" };

				for (size_t j = 0; j < 3; j++)
					for (size_t i = 0; i < 3; i++)
						window[j * 3 + i] = load(std::string("window_") + rows[j] + cols[i]);

				for (size_t i = 0; i < 3; i++)
				{
					button[i] = load(std::string("button_") + ends[i]);
					button_lit[i] = load(std::string("button_lit_") + ends[i]);
					cell[i] = load(std::string("cell_") + ends[i]);
					cell_lit[i] = load(std::string("cell_lit_") + ends[i]);
					input[i] = load(std::string("input_") + ends[i]);
				}

				close = load("close");
			}

			static Texture load(const std::string& name)
			{
				return Texture::from_file("ui/" + name + ".png", Point<int16_t>(0, 0));
			}

			static bool valid(const Texture* parts, size_t count)
			{
				return std::all_of(parts, parts + count, [](const Texture& t) { return t.is_valid(); });
			}
		};

		inline const Art& art()
		{
			static Art loaded;
			return loaded;
		}

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

		inline void stretch(const Texture& t, int16_t x, int16_t y, int16_t w, int16_t h)
		{
			if (w > 0 && h > 0)
				t.draw(DrawArgument(Point<int16_t>(x, y), Point<int16_t>(w, h)));
		}

		// Caps keep their shape at height h; the middle stretches.
		inline void three_slice(const Texture* parts, Point<int16_t> tl, int16_t w, int16_t h)
		{
			int16_t l = static_cast<int16_t>(parts[0].width() * h / std::max<int16_t>(parts[0].height(), 1));
			int16_t r = static_cast<int16_t>(parts[2].width() * h / std::max<int16_t>(parts[2].height(), 1));
			l = std::min<int16_t>(l, w / 2);
			r = std::min<int16_t>(r, w / 2);

			stretch(parts[0], tl.x(), tl.y(), l, h);
			stretch(parts[1], tl.x() + l, tl.y(), w - l - r, h);
			stretch(parts[2], tl.x() + w - r, tl.y(), r, h);
		}

		// A window: the gold-framed navy panel with a title band on top.
		// Short strips (toasts, the typing bar) get a plain navy box with a
		// gold rim, since the framed art needs some height.
		inline void panel(Point<int16_t> tl, int16_t w, int16_t h)
		{
			int16_t x = tl.x();
			int16_t y = tl.y();
			const Art& a = art();

			box(x + 2, y + 3, w, h, Color::Name::BLACK, 0.3f);

			if (h >= 80 && w >= 80 && Art::valid(a.window, 9))
			{
				// The art is at 3x
				int16_t l = a.window[0].width() / 3;
				int16_t r = a.window[2].width() / 3;
				int16_t t = a.window[0].height() / 3;
				int16_t b = a.window[6].height() / 3;
				int16_t xs[4] = { x, static_cast<int16_t>(x + l), static_cast<int16_t>(x + w - r), static_cast<int16_t>(x + w) };
				int16_t ys[4] = { y, static_cast<int16_t>(y + t), static_cast<int16_t>(y + h - b), static_cast<int16_t>(y + h) };

				for (size_t j = 0; j < 3; j++)
					for (size_t i = 0; i < 3; i++)
						stretch(a.window[j * 3 + i], xs[i], ys[j], xs[i + 1] - xs[i], ys[j + 1] - ys[j]);

				return;
			}

			box(x + 1, y + 1, w - 2, h - 2, Color::Name::HUD_PANEL, 0.95f);
			box(x + 2, y + 2, w - 4, 1, Color::Name::WHITE, 0.18f);
			frame(x, y, w, h, Color::Name::HUD_EXP);
		}

		// An orange button; `lit` is the gold hovered, pressed or armed look
		inline void button(Point<int16_t> tl, int16_t w, int16_t h, bool lit = false)
		{
			const Art& a = art();
			const Texture* parts = lit ? a.button_lit : a.button;

			if (Art::valid(parts, 3))
			{
				three_slice(parts, tl, w, h);
				return;
			}

			int16_t x = tl.x();
			int16_t y = tl.y();

			box(x + 1, y + 2, w, h, Color::Name::BLACK, 0.25f);
			box(x + 1, y + 1, w - 2, h - 2, lit ? Color::Name::MAPLE_ORANGE_LIGHT : Color::Name::MAPLE_ORANGE);
			box(x + 2, y + 2, w - 4, (h - 4) / 2, Color::Name::WHITE, 0.25f);
			frame(x, y, w, h, Color::Name::MAPLE_BORDER);
		}

		// A list cell: navy with a dim gold rim, or the gold plate when selected
		inline void cell(Point<int16_t> tl, int16_t w, int16_t h, bool selected)
		{
			int16_t x = tl.x();
			int16_t y = tl.y();
			const Art& a = art();

			const Texture* parts = selected ? a.cell_lit : a.cell;

			if (Art::valid(parts, 3))
			{
				three_slice(parts, tl, w, h);
				return;
			}

			box(x + 1, y + 1, w - 2, h - 2, selected ? Color::Name::HUD_EXP : Color::Name::HUD_TRACK, 0.95f);
			box(x + 1, y + 1, w - 2, 1, Color::Name::WHITE, 0.15f);
			frame(x, y, w, h, Color::Name::HUD_EXP, selected ? 1.0f : 0.45f);
		}

		// A light text field in a gold rim with a gem on its left end;
		// returns false when the APK has no art for it
		inline bool input(Point<int16_t> tl, int16_t w, int16_t h)
		{
			const Art& a = art();

			if (!Art::valid(a.input, 3))
				return false;

			three_slice(a.input, tl, w, h);

			return true;
		}

		// The round red close button, or false when the APK has no art for it
		inline bool close_button(Point<int16_t> tl, int16_t size)
		{
			const Art& a = art();

			if (!a.close.is_valid())
				return false;

			int16_t h = static_cast<int16_t>(size * a.close.height() / std::max<int16_t>(a.close.width(), 1));
			stretch(a.close, tl.x(), tl.y(), size, h);

			return true;
		}
	}
}
