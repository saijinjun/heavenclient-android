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
			load_art();

			bool fresh = oid != target;

			if (fresh || label.get_text().empty())
			{
				target = oid;
				label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, name);
				level_text = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::HUD_EXP, "Lv. " + std::to_string(level));
			}

			label.change_color(danger);
			int8_t next = std::max<int8_t>(0, std::min<int8_t>(100, percent));

			// A new target starts with no trail; a hit leaves the old value
			// behind as a pale strip that drains away
			if (fresh || next > hp)
				trail = next;
			else if (trail < hp)
				trail = hp;

			hp = next;
			percent_text.change_text(std::to_string(hp) + "%");
			ms_left = hp > 0 ? SHOW_MS : DEAD_MS;
		}

		void update()
		{
			if (ms_left > 0)
				ms_left -= std::min<int32_t>(ms_left, Constants::TIMESTEP);

			// The pale trail drains toward the real value
			if (trail > hp)
				trail = std::max<float>(hp, trail - 0.8f);
			else
				trail = hp;
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

			label.draw(DrawArgument(Point<int16_t>(x + left, y + 4), alpha));
			level_text.draw(DrawArgument(Point<int16_t>(x + w - 18, y + 5), alpha));

			// HP gauge: the same metal frame and glossy red fill as the
			// player's own HP bar, with a pale trail where the last hit
			// took HP away
			int16_t gx = x + left;
			int16_t gy = y + h - 19;
			int16_t gw = w - left - 18;
			int16_t gh = 13;
			int16_t fill_w = static_cast<int16_t>(gw * hp / 100);
			int16_t trail_w = static_cast<int16_t>(gw * trail / 100.0f);

			if (classic::Art::valid(frame_art, 3) && classic::Art::valid(fill_art, 3))
			{
				constexpr int16_t INSET = 2;
				int16_t iw = gw - INSET * 2;
				int16_t ih = gh - INSET * 2;
				fill_w = static_cast<int16_t>(iw * hp / 100);
				trail_w = static_cast<int16_t>(iw * trail / 100.0f);

				slice(frame_art, gx, gy, gw, gh, alpha);

				if (trail_w > fill_w)
					classic::box(gx + INSET + fill_w, gy + INSET, trail_w - fill_w, ih, Color::Name::WHITE, 0.5f * alpha);

				if (fill_w > 0)
					slice(fill_art, gx + INSET, gy + INSET, fill_w, ih, alpha);
			}
			else
			{
				classic::box(gx, gy, gw, gh, Color::Name::BLACK, 0.6f * alpha);

				if (trail_w > fill_w)
					classic::box(gx + fill_w, gy, trail_w - fill_w, gh, Color::Name::WHITE, 0.5f * alpha);

				if (fill_w > 0)
				{
					classic::box(gx, gy, fill_w, gh, Color::Name::HUD_HP, alpha);
					classic::box(gx, gy, fill_w, gh / 2, Color::Name::WHITE, 0.2f * alpha);
				}
			}

			percent_text.draw(DrawArgument(Point<int16_t>(gx + gw / 2, gy - 1), alpha));
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
		float trail = 0.0f;
		Text label;
		Text level_text;
		Text percent_text = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, "");
		Texture frame_art[3];
		Texture fill_art[3];
		bool art_loaded = false;

		void load_art()
		{
			if (art_loaded)
				return;

			art_loaded = true;
			const char* ends[3] = { "l", "m", "r" };

			for (size_t i = 0; i < 3; i++)
			{
				frame_art[i] = Texture::from_file(std::string("hud/gauge_frame_") + ends[i] + ".png", Point<int16_t>(0, 0));
				fill_art[i] = Texture::from_file(std::string("hud/fill_hp_") + ends[i] + ".png", Point<int16_t>(0, 0));
			}
		}

		// Caps keep their shape at height h; the middle stretches
		static void slice(const Texture* parts, int16_t x, int16_t y, int16_t w, int16_t h, float alpha)
		{
			int16_t l = static_cast<int16_t>(parts[0].width() * h / std::max<int16_t>(parts[0].height(), 1));
			int16_t r = static_cast<int16_t>(parts[2].width() * h / std::max<int16_t>(parts[2].height(), 1));
			l = std::min<int16_t>(l, w / 2);
			r = std::min<int16_t>(r, w / 2);

			classic::stretch(parts[0], x, y, l, h, alpha);
			classic::stretch(parts[1], x + l, y, w - l - r, h, alpha);
			classic::stretch(parts[2], x + w - r, y, r, h, alpha);
		}
	};
}
