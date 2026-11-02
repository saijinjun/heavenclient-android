//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileStats.h"

#include "UINotice.h"

#include "../UI.h"

#include "../../Character/ExpTable.h"
#include "../../Constants.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/PlayerPackets.h"

#include <algorithm>

namespace ms
{
	namespace
	{
		const char* INFO_NAMES[] = { "HP", "MP", "EXP", "Fame", "Damage", "Job" };
		const char* MAIN_NAMES[] = { "STR", "DEX", "INT", "LUK" };

		const MapleStat::Id MAIN_STATS[] = {
			MapleStat::Id::STR, MapleStat::Id::DEX, MapleStat::Id::INT, MapleStat::Id::LUK
		};

		const EquipStat::Id MAIN_TOTALS[] = {
			EquipStat::Id::STR, EquipStat::Id::DEX, EquipStat::Id::INT, EquipStat::Id::LUK
		};

		constexpr int16_t MARGIN = 16;
		constexpr int16_t ROW_H = 40;

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		void set_text(Text& text, const std::string& value)
		{
			if (text.get_text() != value)
				text.change_text(value);
		}
	}

	UIMobileStats::UIMobileStats(const CharStats& st) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), stats(st)
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Character");
		header = Text(Text::Font::A15B, Text::Alignment::LEFT, Color::Name::HUD_EXP);

		for (size_t i = 0; i < NUM_INFO; i++)
		{
			info_names[i] = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::SILVERCHALICE, INFO_NAMES[i]);
			info_values[i] = Text(Text::Font::A13B, Text::Alignment::RIGHT, Color::Name::WHITE);
		}

		for (size_t i = 0; i < NUM_MAIN; i++)
		{
			main_names[i] = Text(Text::Font::A15B, Text::Alignment::LEFT, Color::Name::HUD_EXP, MAIN_NAMES[i]);
			main_values[i] = Text(Text::Font::A13B, Text::Alignment::RIGHT, Color::Name::WHITE);
		}

		ap_text = Text(Text::Font::A15B, Text::Alignment::LEFT, Color::Name::WHITE);
		plus_label = Text(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::WHITE, "+");
		auto_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Auto assign");
		close_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "X");

		update();
	}

	UIMobileStats::Layout UIMobileStats::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 640);
		int16_t ph = std::min<int16_t>(vh - 24, 400);
		int16_t px = (vw - pw) / 2;
		int16_t py = (vh - ph) / 2;

		l.panel = rect(px, py, pw, ph);
		l.close = rect(px + pw - 40, py + 4, 34, 34);

		int16_t top = py + 72;
		int16_t col_w = (pw - MARGIN * 3) / 2;
		int16_t left = px + MARGIN;
		int16_t right = left + col_w + MARGIN;

		for (size_t i = 0; i < NUM_INFO; i++)
			l.info[i] = rect(left, top + static_cast<int16_t>(i) * (ROW_H - 6), col_w, ROW_H - 10);

		l.ap = rect(right, top, col_w, ROW_H - 10);

		for (size_t i = 0; i < NUM_MAIN; i++)
		{
			int16_t y = top + static_cast<int16_t>(i + 1) * ROW_H;
			l.main[i] = rect(right, y, col_w - 50, ROW_H - 6);
			l.plus[i] = rect(right + col_w - 44, y, 44, ROW_H - 6);
		}

		l.autoassign = rect(right, top + static_cast<int16_t>(NUM_MAIN + 1) * ROW_H + 4, col_w, 36);

		return l;
	}

	void UIMobileStats::update()
	{
		UIElement::update();

		set_text(header, stats.get_name() + "   Lv. " + std::to_string(stats.get_stat(MapleStat::Id::LEVEL)));

		set_text(info_values[0], std::to_string(stats.get_stat(MapleStat::Id::HP)) + " / " + std::to_string(stats.get_total(EquipStat::Id::HP)));
		set_text(info_values[1], std::to_string(stats.get_stat(MapleStat::Id::MP)) + " / " + std::to_string(stats.get_total(EquipStat::Id::MP)));

		int16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		int64_t needed = level < 200 ? ExpTable::values[level] : 0;
		int64_t percent = needed > 0 ? stats.get_exp() * 10000 / needed : 0;
		set_text(info_values[2], std::to_string(percent / 100) + "." + (percent % 100 < 10 ? "0" : "") + std::to_string(percent % 100) + "%");

		set_text(info_values[3], std::to_string(stats.get_honor()));
		set_text(info_values[4], std::to_string(stats.get_mindamage()) + " ~ " + std::to_string(stats.get_maxdamage()));
		set_text(info_values[5], stats.get_jobname());

		for (size_t i = 0; i < NUM_MAIN; i++)
		{
			int32_t base = stats.get_stat(MAIN_STATS[i]);
			int32_t total = stats.get_total(MAIN_TOTALS[i]);
			std::string value = std::to_string(total);

			if (total != base)
				value += "  (" + std::to_string(base) + " + " + std::to_string(total - base) + ")";

			set_text(main_values[i], value);
		}

		set_text(ap_text, "AP to spend: " + std::to_string(stats.get_stat(MapleStat::Id::AP)));
	}

	void UIMobileStats::draw(float inter) const
	{
		Layout l = layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());
		title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 6));

		if (!classic::close_button(l.close.get_left_top(), l.close.width()))
		{
			classic::button(l.close.get_left_top(), l.close.width(), l.close.height());
			close_label.draw(Point<int16_t>(l.close.left() + l.close.width() / 2, l.close.top() + 8));
		}

		header.draw(Point<int16_t>(l.panel.left() + MARGIN, l.panel.top() + 40));

		for (size_t i = 0; i < NUM_INFO; i++)
		{
			const Rectangle<int16_t>& r = l.info[i];
			classic::cell(r.get_left_top(), r.width(), r.height(), false);
			info_names[i].draw(Point<int16_t>(r.left() + 10, r.top() + (r.height() - info_names[i].height()) / 2));
			info_values[i].draw(Point<int16_t>(r.right() - 10, r.top() + (r.height() - info_values[i].height()) / 2));
		}

		bool has_ap = stats.get_stat(MapleStat::Id::AP) > 0;

		ap_text.draw(Point<int16_t>(l.ap.left() + 4, l.ap.top() + (l.ap.height() - ap_text.height()) / 2));

		for (size_t i = 0; i < NUM_MAIN; i++)
		{
			const Rectangle<int16_t>& r = l.main[i];
			classic::cell(r.get_left_top(), r.width(), r.height(), false);
			main_names[i].draw(Point<int16_t>(r.left() + 10, r.top() + (r.height() - main_names[i].height()) / 2));
			main_values[i].draw(Point<int16_t>(r.right() - 10, r.top() + (r.height() - main_values[i].height()) / 2));

			if (has_ap)
			{
				const Rectangle<int16_t>& p = l.plus[i];
				classic::button(p.get_left_top(), p.width(), p.height(), true);
				plus_label.draw(Point<int16_t>(p.left() + p.width() / 2, p.top() + (p.height() - plus_label.height()) / 2));
			}
		}

		if (has_ap)
		{
			const Rectangle<int16_t>& a = l.autoassign;
			classic::button(a.get_left_top(), a.width(), a.height());
			auto_label.draw(Point<int16_t>(a.left() + a.width() / 2, a.top() + (a.height() - auto_label.height()) / 2));
		}

		UIElement::draw(inter);
	}

	void UIMobileStats::spend(MapleStat::Id stat) const
	{
		// The UI waits for the server's stat update, as the classic window does
		SpendApPacket(stat).dispatch();
		UI::get().disable();
	}

	void UIMobileStats::auto_assign() const
	{
		uint16_t ap = stats.get_stat(MapleStat::Id::AP);
		const Player& player = Stage::get().get_player();
		EquipStat::Id primary = stats.get_job().get_primary(player.get_weapontype());

		MapleStat::Id target = MapleStat::Id::STR;
		std::string name = "STR";

		for (size_t i = 0; i < NUM_MAIN; i++)
		{
			if (MAIN_TOTALS[i] == primary)
			{
				target = MAIN_STATS[i];
				name = MAIN_NAMES[i];
			}
		}

		std::string message = "Put all " + std::to_string(ap) + " AP into " + name + "?";

		UI::get().emplace<UIYesNo>(message, [ap, target](bool yes)
		{
			if (!yes)
				return;

			for (uint16_t i = 0; i < ap; i++)
				SpendApPacket(target).dispatch();

			UI::get().disable();
		});
	}

	Cursor::State UIMobileStats::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Stay on top and keep the touches after a dialog over this closes
		UI::get().bring_to_front(TYPE, true);

		// Act when the finger lifts where it landed, so a swipe to scroll
		// never presses what it started on
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		cursorpos = at;

		Layout l = layout();

		if (l.close.contains(cursorpos) || !l.panel.contains(cursorpos))
		{
			deactivate();
			return Cursor::State::IDLE;
		}

		if (stats.get_stat(MapleStat::Id::AP) > 0)
		{
			for (size_t i = 0; i < NUM_MAIN; i++)
			{
				if (l.plus[i].contains(cursorpos))
				{
					spend(MAIN_STATS[i]);
					return Cursor::State::CLICKING;
				}
			}

			if (l.autoassign.contains(cursorpos))
			{
				auto_assign();
				return Cursor::State::CLICKING;
			}
		}

		return Cursor::State::IDLE;
	}

	void UIMobileStats::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIMobileStats::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIMobileStats::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileStats::get_type() const
	{
		return TYPE;
	}
}
