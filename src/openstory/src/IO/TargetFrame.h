//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#pragma once

#include "../Constants.h"
#include "../Graphics/ClassicStyle.h"
#include "../Graphics/Text.h"

#include <algorithm>
#include <string>

// Phone HUD: a small card at the top of the screen naming the monster the
// player last hit, with its level and a big HP bar, so the fight reads at a
// glance on a small screen. It fades out a few seconds after the last hit.
namespace ms
{
	class TargetFrame
	{
	public:
		static TargetFrame& get()
		{
			static TargetFrame frame;
			return frame;
		}

		void hit(int32_t oid, const std::string& name, uint16_t level, int8_t percent, Color::Name danger)
		{
			if (oid != target || label.get_text().empty())
			{
				target = oid;
				label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, name);
				level_text = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::HUD_EXP, "Lv. " + std::to_string(level));
			}

			label.change_color(danger);
			hp = std::max<int8_t>(0, std::min<int8_t>(100, percent));
			ms_left = hp > 0 ? SHOW_MS : DEAD_MS;
		}

		void update()
		{
			if (ms_left > 0)
				ms_left -= std::min<int32_t>(ms_left, Constants::TIMESTEP);

			// The bar slides toward the real value instead of jumping
			if (shown_hp > hp)
				shown_hp = std::max<float>(hp, shown_hp - 1.5f);
			else
				shown_hp = hp;
		}

		void draw() const
		{
			if (ms_left <= 0 || target == 0)
				return;

			float alpha = ms_left < FADE_MS ? static_cast<float>(ms_left) / FADE_MS : 1.0f;

			int16_t w = WIDTH;
			int16_t h = HEIGHT;
			int16_t x = (Constants::Constants::get().get_viewwidth() - w) / 2;
			int16_t y = 6;

			// Summer's plate (gold-rimmed navy pill with a slime badge on the
			// left), or a plain navy card in a gold rim without it
			const classic::Art& art = classic::art();
			int16_t left = 10;

			if (classic::Art::valid(art.target, 3))
			{
				int16_t l = static_cast<int16_t>(art.target[0].width() * h / std::max<int16_t>(art.target[0].height(), 1));
				int16_t r = static_cast<int16_t>(art.target[2].width() * h / std::max<int16_t>(art.target[2].height(), 1));

				classic::stretch(art.target[0], x, y, l, h, alpha);
				classic::stretch(art.target[1], x + l, y, w - l - r, h, alpha);
				classic::stretch(art.target[2], x + w - r, y, r, h, alpha);

				// Text and bar start right of the badge
				left = h + 4;
			}
			else
			{
				classic::box(x + 2, y + 3, w, h, Color::Name::BLACK, 0.3f * alpha);
				classic::box(x, y, w, h, Color::Name::HUD_PANEL, 0.88f * alpha);
				classic::frame(x, y, w, h, Color::Name::HUD_EXP, alpha);
				classic::box(x + 1, y + 1, w - 2, 1, Color::Name::WHITE, 0.18f * alpha);
			}

			label.draw(DrawArgument(Point<int16_t>(x + left, y + 5), alpha));
			level_text.draw(DrawArgument(Point<int16_t>(x + w - 16, y + 6), alpha));

			// HP bar: a dark track, the red fill, and a lighter band on top
			int16_t bx = x + left;
			int16_t by = y + h - 15;
			int16_t bw = w - left - 16;
			int16_t fill = static_cast<int16_t>(bw * shown_hp / 100.0f);

			classic::box(bx, by, bw, 8, Color::Name::BLACK, 0.6f * alpha);

			if (fill > 0)
			{
				classic::box(bx, by, fill, 8, Color::Name::RED, alpha);
				classic::box(bx, by, fill, 3, Color::Name::WHITE, 0.25f * alpha);
			}

			classic::frame(bx - 1, by - 1, bw + 2, 10, Color::Name::HUD_EXP, 0.5f * alpha);
		}

		// The target left the map or the player changed maps
		void clear()
		{
			target = 0;
			ms_left = 0;
		}

		void forget(int32_t oid)
		{
			if (oid == target)
				ms_left = std::min<int32_t>(ms_left, DEAD_MS);
		}

	private:
		static constexpr int16_t WIDTH = 260;
		static constexpr int16_t HEIGHT = 42;
		static constexpr int32_t SHOW_MS = 6000;
		static constexpr int32_t DEAD_MS = 900;
		static constexpr int32_t FADE_MS = 600;

		int32_t target = 0;
		int32_t ms_left = 0;
		int8_t hp = 0;
		float shown_hp = 0.0f;
		Text label;
		Text level_text;
	};
}
