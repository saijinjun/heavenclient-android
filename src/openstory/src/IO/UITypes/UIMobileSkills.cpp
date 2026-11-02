//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileSkills.h"

#include "UISkillBook.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Character/SkillId.h"
#include "../../Data/JobData.h"
#include "../../Data/SkillData.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Net/Packets/PlayerPackets.h"

#include <algorithm>
#include <tuple>
#include <unordered_set>

namespace ms
{
	namespace
	{
		constexpr int16_t MARGIN = 12;
		constexpr int16_t TITLE_H = 26;
		constexpr int16_t TAB_H = 32;
		constexpr int16_t DETAIL_W = 220;
		constexpr int16_t FOOT_H = 30;
		constexpr int16_t ROW_H = 50;
		constexpr int16_t GAP = 6;
		constexpr int16_t SLOT_H = 34;

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		void set_text(Text& text, const std::string& value)
		{
			if (text.get_text() != value)
				text.change_text(value);
		}

		// Three Snails, Recovery and Nimble Feet share the beginner's SP
		bool beginner_skill(int32_t id)
		{
			int32_t base = id % 10000;
			return base == 1000 || base == 1001 || base == 1002;
		}
	}

	UIMobileSkills::UIMobileSkills(const CharStats& st, const SkillBook& sb) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), stats(st), skillbook(sb)
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Skills");

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE);

		sp_text = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP);
		plus_label = Text(Text::Font::A18M, Text::Alignment::CENTER, Color::Name::WHITE, "+");
		close_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "X");
		classic_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, "Classic view");
		empty_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap a skill to see it here", DETAIL_W - 20);
		slot_hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE, "Put on quick slot");

		for (size_t i = 0; i < NUM_SLOTS; i++)
			slot_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, std::to_string(i + 1));

		name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		level_text = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE);
		desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", DETAIL_W - 20);
		level_desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, "", DETAIL_W - 20);

		shown_job = stats.get_stat(MapleStat::Id::JOB);
		job.change_job(shown_job);
		tab = num_tabs() - 1;

		rebuild();
		update();
	}

	UIMobileSkills::Layout UIMobileSkills::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 720);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = rect(px, py, pw, ph);
		l.close = rect(px + pw - 40, py + 4, 34, 34);
		l.classic = rect(px + MARGIN, py + 6, 96, 24);

		int16_t tab_y = py + TITLE_H + 14;
		int16_t n = static_cast<int16_t>(num_tabs());
		int16_t tab_w = std::min<int16_t>(150, (pw - MARGIN * 2 - GAP * (n - 1)) / n);

		for (size_t i = 0; i < NUM_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		int16_t body_y = tab_y + TAB_H + 10;
		int16_t body_h = py + ph - FOOT_H - 8 - body_y;

		l.list = rect(px + MARGIN, body_y, pw - MARGIN * 3 - DETAIL_W, body_h);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / (ROW_H + GAP));
		l.detail = rect(px + pw - MARGIN - DETAIL_W, body_y, DETAIL_W, body_h);

		int16_t slot_w = (DETAIL_W - 20 - GAP * 2) / 3;

		for (size_t i = 0; i < NUM_SLOTS; i++)
		{
			int16_t col = static_cast<int16_t>(i % 3);
			int16_t row = static_cast<int16_t>(i / 3);
			int16_t y = l.detail.bottom() - 8 - (2 - row) * SLOT_H - (1 - row) * GAP;
			l.slots[i] = rect(l.detail.left() + 10 + col * (slot_w + GAP), y, slot_w, SLOT_H);
		}

		return l;
	}

	size_t UIMobileSkills::num_tabs() const
	{
		return std::min<size_t>(NUM_TABS, static_cast<size_t>(job.get_level()) + 1);
	}

	Job::Level UIMobileSkills::tab_level() const
	{
		return static_cast<Job::Level>(Job::Level::BEGINNER + tab);
	}

	void UIMobileSkills::rebuild()
	{
		Job::Level level = tab_level();
		uint16_t subid = job.get_subjob(level);
		const JobData& data = JobData::get(subid);

		for (size_t i = 0; i < num_tabs(); i++)
			set_text(tab_labels[i], JobData::get(job.get_subjob(static_cast<Job::Level>(Job::Level::BEGINNER + i))).get_name());

		// Same rules as the classic skill book: the job's own skill tree plus
		// anything the server granted with that prefix; common skills on the
		// beginner tab; hidden skills only once learned.
		std::vector<Row> fresh;
		std::unordered_set<int32_t> seen;

		auto push = [&](int32_t sid)
		{
			bool common_on_beginner = sid / 10000 == 0 && level == Job::Level::BEGINNER;

			if (sid / 10000 != static_cast<int32_t>(subid) && !common_on_beginner)
				return;

			int32_t learned = skillbook.get_level(sid);

			if (SkillData::get(sid).is_invisible() && learned == 0)
				return;

			if (!seen.insert(sid).second)
				return;

			int32_t master = skillbook.get_masterlevel(sid);

			if (master == 0)
				master = SkillData::get(sid).get_masterlevel();

			fresh.push_back({ sid, learned, master });
		};

		for (int32_t sid : data.get_skills())
			push(sid);

		for (auto& entry : skillbook.get_entries())
			push(entry.first);

		rows = std::move(fresh);
		clamp_scroll();
	}

	int16_t UIMobileSkills::remaining_sp() const
	{
		if (tab_level() != Job::Level::BEGINNER)
			return stats.get_stat(MapleStat::Id::SP);

		int16_t level = stats.get_stat(MapleStat::Id::LEVEL);
		int16_t sp = level >= 7 ? 6 : level - 1;

		for (const Row& row : rows)
			if (beginner_skill(row.id))
				sp -= static_cast<int16_t>(row.level);

		return std::max<int16_t>(0, sp);
	}

	bool UIMobileSkills::can_raise(const Row& row) const
	{
		if (remaining_sp() <= 0 || row.level >= row.masterlevel)
			return false;

		if (row.id % 10000 == SkillId::Id::ANGEL_BLESSING)
			return false;

		std::unordered_map<int32_t, int32_t> required = skillbook.collect_required(row.id);

		if (required.empty())
			required = SkillData::get(row.id).get_reqskills();

		for (auto& req : required)
			if (skillbook.get_level(req.first) < req.second)
				return false;

		return true;
	}

	bool UIMobileSkills::can_bind(const Row& row) const
	{
		return row.level > 0 && !SkillData::get(row.id).is_passive();
	}

	const UIMobileSkills::Row* UIMobileSkills::selected_row() const
	{
		for (const Row& row : rows)
			if (row.id == selected)
				return &row;

		return nullptr;
	}

	Rectangle<int16_t> UIMobileSkills::row_rect(const Layout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - first_row) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 8, ROW_H);
	}

	Rectangle<int16_t> UIMobileSkills::plus_rect(const Rectangle<int16_t>& row) const
	{
		return rect(row.right() - 52, row.top() + 5, 46, ROW_H - 10);
	}

	void UIMobileSkills::clamp_scroll()
	{
		Layout l = layout();
		int16_t max_first = std::max<int16_t>(0, static_cast<int16_t>(rows.size()) - l.rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);
	}

	int16_t UIMobileSkills::bound_slot(int32_t skill_id) const
	{
		const Keyboard& keyboard = UI::get().get_keyboard();
		const auto& keys = keyboard.get_quickslot_keys();

		for (size_t i = 0; i < NUM_SLOTS && i < keys.size(); i++)
		{
			Keyboard::Mapping mapping = keyboard.get_maple_mapping(keys[i]);

			if (mapping.type == KeyType::Id::SKILL && mapping.action == skill_id)
				return static_cast<int16_t>(i);
		}

		return -1;
	}

	void UIMobileSkills::bind(size_t slot, int32_t skill_id) const
	{
		// The touch quick slots show the quickslot bar's keys, so this is the
		// same binding a drag onto a slot makes.
		Keyboard& keyboard = UI::get().get_keyboard();
		const auto& keys = keyboard.get_quickslot_keys();

		if (slot >= keys.size())
			return;

		uint8_t key = keys[slot];
		keyboard.assign(key, static_cast<uint8_t>(KeyType::Id::SKILL), skill_id);

		std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updates;
		updates.emplace_back(static_cast<KeyConfig::Key>(key), KeyType::Id::SKILL, skill_id);
		ChangeKeyMapPacket(updates).dispatch();
	}

	void UIMobileSkills::open_classic()
	{
		deactivate();
		UI::get().emplace<UISkillBook>(stats, skillbook);
	}

	void UIMobileSkills::draw_icon(int32_t skill_id, bool learned, Rectangle<int16_t> box) const
	{
		const SkillData& data = SkillData::get(skill_id);
		const Texture& icon = data.get_icon(learned ? SkillData::Icon::NORMAL : SkillData::Icon::DISABLED);

		if (!icon.is_valid())
			return;

		Point<int16_t> dim = icon.get_dimensions();
		float scale = std::min(box.width() * 0.85f / std::max<int16_t>(dim.x(), 1), box.height() * 0.85f / std::max<int16_t>(dim.y(), 1));
		scale = std::min(scale, 2.0f);

		Point<int16_t> origin = icon.get_origin();
		int16_t x = box.left() + (box.width() - static_cast<int16_t>(dim.x() * scale)) / 2 + static_cast<int16_t>(origin.x() * scale);
		int16_t y = box.top() + (box.height() - static_cast<int16_t>(dim.y() * scale)) / 2 + static_cast<int16_t>(origin.y() * scale);

		icon.draw(DrawArgument(Point<int16_t>(x, y), scale, scale));
	}

	void UIMobileSkills::draw_label(const Text& text, Rectangle<int16_t> box) const
	{
		text.draw(Point<int16_t>(box.left() + box.width() / 2, box.top() + (box.height() - text.height()) / 2));
	}

	Text& UIMobileSkills::row_name(int32_t skill_id) const
	{
		auto iter = names.find(skill_id);

		if (iter == names.end())
			iter = names.emplace(skill_id, Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE, SkillData::get(skill_id).get_name())).first;

		return iter->second;
	}

	void UIMobileSkills::draw(float inter) const
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
			draw_label(close_label, l.close);
		}

		classic::cell(l.classic.get_left_top(), l.classic.width(), l.classic.height(), false);
		draw_label(classic_label, l.classic);

		for (size_t i = 0; i < num_tabs(); i++)
		{
			bool lit = i == tab;
			classic::cell(l.tabs[i].get_left_top(), l.tabs[i].width(), l.tabs[i].height(), lit);
			tab_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			draw_label(tab_labels[i], l.tabs[i]);
		}

		// The skill rows
		int16_t last = std::min<int16_t>(static_cast<int16_t>(rows.size()), first_row + l.rows);

		for (int16_t i = first_row; i < last; i++)
		{
			const Row& row = rows[i];
			Rectangle<int16_t> r = row_rect(l, i);

			classic::cell(r.get_left_top(), r.width(), r.height(), row.id == selected);
			draw_icon(row.id, row.level > 0, rect(r.left() + 3, r.top() + 3, ROW_H - 6, ROW_H - 6));

			Text& name_text = row_name(row.id);
			name_text.change_color(row.id == selected ? Color::Name::HUD_PANEL : (row.level > 0 ? Color::Name::WHITE : Color::Name::SILVERCHALICE));
			name_text.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 6));

			auto& lv = levels[row.id];

			if (lv.first != row.level || lv.second.get_text().empty())
			{
				lv.first = row.level;
				lv.second = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE, "Lv. " + std::to_string(row.level) + " / " + std::to_string(row.masterlevel));
			}

			lv.second.change_color(row.id == selected ? Color::Name::HUD_PANEL : Color::Name::SILVERCHALICE);
			lv.second.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 27));

			if (can_raise(row))
			{
				Rectangle<int16_t> p = plus_rect(r);
				classic::button(p.get_left_top(), p.width(), p.height(), true);
				draw_label(plus_label, p);
			}
		}

		// A thin bar beside the rows shows where the list is
		int16_t count = static_cast<int16_t>(rows.size());

		if (count > l.rows)
		{
			int16_t track_h = l.list.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / count);
			int16_t thumb_y = l.list.top() + (track_h - thumb_h) * first_row / std::max<int16_t>(1, count - l.rows);
			int16_t bar_x = l.list.right() - 4;

			classic::box(bar_x, l.list.top(), 4, track_h, Color::Name::HUD_TRACK, 0.9f);
			classic::box(bar_x, thumb_y, 4, thumb_h, Color::Name::HUD_EXP);
		}

		sp_text.draw(Point<int16_t>(l.list.left() + 4, l.panel.bottom() - FOOT_H - 2));

		// The selected skill
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		const Row* row = selected_row();

		if (!row)
		{
			empty_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 20));
		}
		else
		{
			if (shown_skill != row->id || shown_level != row->level)
			{
				const SkillData& data = SkillData::get(row->id);
				shown_skill = row->id;
				shown_level = row->level;

				name.change_text(data.get_name());
				level_text.change_text("Level " + std::to_string(row->level) + " of " + std::to_string(row->masterlevel) + (data.is_passive() ? "  (passive)" : ""));
				desc.change_text(data.get_desc());

				// What it does now, or at level 1 when not learned yet
				int32_t shown = std::max(row->level, 1);
				level_desc.change_text((row->level > 0 ? "" : "At level 1: ") + data.get_level_desc(shown));
			}

			Rectangle<int16_t> big = rect(l.detail.left() + 10, l.detail.top() + 10, 56, 56);
			draw_icon(row->id, row->level > 0, big);

			name.draw(Point<int16_t>(big.right() + 8, big.top() + 4));
			level_text.draw(Point<int16_t>(big.right() + 8, big.top() + 8 + name.height()));

			// Long texts are left out rather than run under the slot buttons
			int16_t bottom = can_bind(*row) ? l.slots[0].top() - 22 : l.detail.bottom() - 6;
			int16_t y = big.bottom() + 8;

			if (y + desc.height() <= bottom)
			{
				desc.draw(Point<int16_t>(l.detail.left() + 10, y));
				y += desc.height() + 6;
			}

			if (y + level_desc.height() <= bottom)
				level_desc.draw(Point<int16_t>(l.detail.left() + 10, y));

			if (can_bind(*row))
			{
				int16_t bound = bound_slot(row->id);
				slot_hint.draw(Point<int16_t>(l.detail.left() + 10, l.slots[0].top() - 18));

				for (size_t i = 0; i < NUM_SLOTS; i++)
				{
					const Rectangle<int16_t>& s = l.slots[i];
					bool lit = static_cast<int16_t>(i) == bound;
					classic::button(s.get_left_top(), s.width(), s.height(), lit);
					draw_label(slot_labels[i], s);
				}
			}
		}

		UIElement::draw(inter);
	}

	void UIMobileSkills::update()
	{
		UIElement::update();

		// A job advancement while the panel is open adds a tab
		uint16_t job_id = stats.get_stat(MapleStat::Id::JOB);

		if (job_id != shown_job)
		{
			shown_job = job_id;
			job.change_job(job_id);
			tab = num_tabs() - 1;
			first_row = 0;
			selected = 0;
		}

		// Levels change as SP is spent; the list is short, so read it again
		rebuild();

		set_text(sp_text, (tab_level() == Job::Level::BEGINNER ? "Beginner SP: " : "Skill points: ") + std::to_string(remaining_sp()));
	}

	Cursor::State UIMobileSkills::send_cursor(bool clicked, Point<int16_t> cursorpos)
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

		if (l.classic.contains(cursorpos))
		{
			open_classic();
			return Cursor::State::IDLE;
		}

		for (size_t i = 0; i < num_tabs(); i++)
		{
			if (l.tabs[i].contains(cursorpos))
			{
				if (tab != i)
				{
					tab = i;
					selected = 0;
					first_row = 0;
					rebuild();
				}

				return Cursor::State::CLICKING;
			}
		}

		if (l.list.contains(cursorpos))
		{
			int16_t last = std::min<int16_t>(static_cast<int16_t>(rows.size()), first_row + l.rows);

			for (int16_t i = first_row; i < last; i++)
			{
				Rectangle<int16_t> r = row_rect(l, i);

				if (!r.contains(cursorpos))
					continue;

				const Row& row = rows[i];
				selected = row.id;

				if (plus_rect(r).contains(cursorpos) && can_raise(row))
					SpendSpPacket(row.id).dispatch();

				return Cursor::State::CLICKING;
			}

			return Cursor::State::IDLE;
		}

		const Row* row = selected_row();

		if (row && can_bind(*row))
		{
			for (size_t i = 0; i < NUM_SLOTS; i++)
			{
				if (l.slots[i].contains(cursorpos))
				{
					bind(i, row->id);
					return Cursor::State::CLICKING;
				}
			}
		}

		return Cursor::State::IDLE;
	}

	void UIMobileSkills::send_scroll(double yoffset)
	{
		gesture.cancel();

		first_row += yoffset > 0 ? -1 : 1;
		clamp_scroll();
	}

	void UIMobileSkills::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIMobileSkills::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIMobileSkills::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileSkills::get_type() const
	{
		return TYPE;
	}
}
