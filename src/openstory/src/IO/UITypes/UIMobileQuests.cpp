//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileQuests.h"

#include "UINotice.h"
#include "UIQuestLog.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/QuestPackets.h"
#include "../../Util/Misc.h"

#include <algorithm>
#include <functional>

#include <nlnx/nx.hpp>

namespace ms
{
	namespace
	{
		const char* TAB_NAMES[] = { "In progress", "Available", "Done" };

		constexpr int16_t MARGIN = 12;
		constexpr int16_t TITLE_H = 26;
		constexpr int16_t TAB_H = 32;
		constexpr int16_t ROW_H = 40;
		constexpr int16_t GAP = 6;
		// Longer stories are cut so the details fit the panel
		constexpr size_t DESC_MAX = 360;

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		std::string quest_name(nl::node qnode)
		{
			if (!qnode || !qnode["name"])
				return "";

			std::string name = strip_quest_codes(qnode["name"].get_string());

			// Untranslated quests and the server's unnamed flag records are not
			// real quests; the classic log hides them too
			return has_cjk(name) ? "" : name;
		}

		std::string shorten(std::string text, size_t max)
		{
			if (text.size() <= max)
				return text;

			size_t cut = text.rfind(' ', max);
			text.resize(cut == std::string::npos ? max : cut);
			return text + " ...";
		}

		std::string npc_name(int32_t npcid)
		{
			if (npcid <= 0)
				return "";

			return strip_quest_codes(nl::nx::string["Npc.img"][std::to_string(npcid)]["name"].get_string());
		}

		// Cuts a name with "..." until it fits the width
		void fit(Text& text, int16_t width)
		{
			std::string value = text.get_text();

			while (text.width() > width && value.size() > 4)
			{
				value.resize(value.size() - 3);

				// Never cut a UTF-8 character in half
				while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80)
					value.pop_back();

				if (!value.empty() && static_cast<unsigned char>(value.back()) >= 0xC0)
					value.pop_back();

				text.change_text(value + "...");
			}
		}

		int16_t player_level()
		{
			return Stage::get().get_player().get_stats().get_stat(MapleStat::Id::LEVEL);
		}
	}

	UIMobileQuests::UIMobileQuests(const QuestLog& ql) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), questlog(ql)
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Quests");

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, TAB_NAMES[i]);

		close_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "X");
		classic_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, "Classic view");
		giveup_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Give up");
		empty_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap a quest to see it here", 220);
		empty_list = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "No quests here yet");
		needed_title = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "Still needed");
		rewards_title = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "Rewards");

		load();

		// Open on what the player can start when nothing is in progress
		if (entries[IN_PROGRESS].empty() && !entries[AVAILABLE].empty())
			tab = AVAILABLE;
	}

	UIMobileQuests::Layout UIMobileQuests::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 760);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = rect(px, py, pw, ph);
		l.close = rect(px + pw - 40, py + 4, 34, 34);
		l.classic = rect(px + MARGIN, py + 6, 96, 24);

		int16_t tab_y = py + TITLE_H + 14;
		int16_t tab_w = std::min<int16_t>(140, (pw - MARGIN * 2 - GAP * (NUM_TABS - 1)) / NUM_TABS);

		for (size_t i = 0; i < NUM_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		int16_t body_y = tab_y + TAB_H + 10;
		int16_t body_h = py + ph - MARGIN - body_y;
		int16_t list_w = (pw - MARGIN * 3) * 2 / 5;

		l.list = rect(px + MARGIN, body_y, list_w, body_h);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / (ROW_H + GAP));
		l.detail = rect(l.list.right() + MARGIN, body_y, px + pw - MARGIN - l.list.right() - MARGIN, body_h);
		l.giveup = rect(l.detail.right() - 130, l.detail.bottom() - 44, 120, 36);

		return l;
	}

	size_t UIMobileQuests::signature() const
	{
		// Changes whenever a quest starts, finishes, or records progress
		size_t sig = questlog.get_started().size() * 31 + questlog.get_completed().size();

		for (const auto& quest : questlog.get_started())
			sig = sig * 131 + std::hash<std::string>()(quest.second) + static_cast<size_t>(quest.first);

		return sig;
	}

	void UIMobileQuests::load()
	{
		for (auto& list : entries)
			list.clear();

		nl::node quest_info = nl::nx::quest["QuestInfo.img"];
		nl::node quest_check = nl::nx::quest["Check.img"];

		for (const auto& quest : questlog.get_started())
		{
			std::string name = quest_name(quest_info[std::to_string(quest.first)]);

			if (name.empty())
				continue;

			entries[IN_PROGRESS].push_back({ quest.first, 0, Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, name, 0, false), Text() });
		}

		for (const auto& quest : questlog.get_completed())
		{
			std::string name = quest_name(quest_info[std::to_string(quest.first)]);

			if (name.empty())
				continue;

			entries[DONE].push_back({ quest.first, 0, Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, name, 0, false), Text() });
		}

		load_available(entries[AVAILABLE]);

		// Long names are cut to the row, leaving room for the level note
		int16_t row_w = layout().list.width() - 8 - 20;

		for (auto& list : entries)
			for (Entry& entry : list)
				fit(entry.name, row_w - (entry.note.get_text().empty() ? 0 : 60));

		loaded_signature = signature();
		loaded_level = player_level();
		clamp_scroll();
	}

	void UIMobileQuests::load_available(std::vector<Entry>& out) const
	{
		// The quests an NPC would offer now: not started, the right level and
		// job, and their earlier quests done. The classic log's filters, less
		// its search and location options.
		nl::node quest_info = nl::nx::quest["QuestInfo.img"];
		nl::node quest_check = nl::nx::quest["Check.img"];

		const auto& started = questlog.get_started();
		const auto& completed = questlog.get_completed();
		int16_t level = player_level();
		int16_t job = Stage::get().get_player().get_stats().get_stat(MapleStat::Id::JOB);

		for (auto qnode : quest_info)
		{
			const std::string& qid_str = qnode.name();

			if (qid_str.empty() || qid_str.size() > 5 || !std::all_of(qid_str.begin(), qid_str.end(), ::isdigit))
				continue;

			int32_t qid_full = std::stoi(qid_str);

			if (qid_full > 32767)
				continue;

			int16_t qid = static_cast<int16_t>(qid_full);

			if (started.count(qid) || completed.count(qid) || qnode["autoStart"].get_integer() != 0)
				continue;

			nl::node start = quest_check[qid_str]["0"];

			if (!start || start["normalAutoStart"].get_integer() != 0 || !start["end"].get_string().empty() || start["npc"].get_integer() <= 0)
				continue;

			int32_t lvmin = start["lvmin"].get_integer();
			int32_t lvmax = start["lvmax"].get_integer();

			if ((lvmin > 0 && level < lvmin) || (lvmax > 0 && level > lvmax))
				continue;

			nl::node jobs = start["job"];

			if (jobs && jobs.size() > 0)
			{
				bool job_ok = false;

				for (auto j : jobs)
				{
					int32_t required = j.get_integer();

					// 100 also lets 110, 111 and 112 in
					if (required == 0 || required == job || (required > 0 && job / 100 == required / 100 && job >= required))
					{
						job_ok = true;
						break;
					}
				}

				if (!job_ok)
					continue;
			}

			bool prereqs_met = true;

			for (auto prereq : start["quest"])
			{
				int32_t id = prereq["id"].get_integer();
				int32_t state = prereq["state"].get_integer();

				if (id <= 0)
					continue;

				if ((state == 2 && !completed.count(static_cast<int16_t>(id))) || (state == 1 && !started.count(static_cast<int16_t>(id))))
				{
					prereqs_met = false;
					break;
				}
			}

			if (!prereqs_met)
				continue;

			std::string name = quest_name(qnode);

			if (name.empty())
				continue;

			std::string note = lvmin > 0 ? "Lv. " + std::to_string(lvmin) : "";
			out.push_back({ qid, lvmin, Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, name, 0, false), Text(Text::Font::A11M, Text::Alignment::RIGHT, Color::Name::SILVERCHALICE, note) });
		}

		// The ones closest to the character's level first
		std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return a.level > b.level; });
	}

	void UIMobileQuests::select(int16_t id)
	{
		selected = id;
		needed.clear();
		rewards.clear();

		if (id == 0)
			return;

		std::string qid = std::to_string(id);
		nl::node qnode = nl::nx::quest["QuestInfo.img"][qid];
		nl::node check = nl::nx::quest["Check.img"][qid];
		bool in_progress = tab == IN_PROGRESS;
		uint16_t width = static_cast<uint16_t>(layout().detail.width() - 20);

		name = Text(Text::Font::A15B, Text::Alignment::LEFT, Color::Name::HUD_EXP, quest_name(qnode), width, false);

		// Who to see: the one to hand it in to while in progress, else the one who gives it
		int32_t npcid = check[in_progress ? "1" : "0"]["npc"].get_integer();
		std::string who = npc_name(npcid);
		npc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, who.empty() ? "" : (tab == DONE ? "From " : "Talk to ") + who, width, false);

		std::string story;

		if (tab == DONE && qnode["2"])
			story = qnode["2"].get_string();
		else if (in_progress && qnode["1"])
			story = qnode["1"].get_string();
		else
			story = qnode["0"].get_string();

		story = shorten(strip_quest_codes(story), DESC_MAX);
		desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, story, width, false);

		if (in_progress)
		{
			nl::node end = check["1"];
			const Inventory& inventory = Stage::get().get_player().get_inventory();

			for (auto item : end["item"])
			{
				int32_t itemid = item["id"].get_integer();
				int32_t count = item["count"].get_integer();

				// Negative counts are items the quest takes away
				if (itemid <= 0 || count <= 0)
					continue;

				int32_t have = inventory.get_total_item_count(itemid);
				std::string item_name = ItemData::get(itemid).get_name();
				bool done = have >= count;

				needed.push_back({ Text(Text::Font::A12M, Text::Alignment::LEFT, done ? Color::Name::CHARTREUSE : Color::Name::WHITE,
					(item_name.empty() ? "Item " + std::to_string(itemid) : item_name) + "  " + std::to_string(std::min(have, count)) + " / " + std::to_string(count), width, false), done });
			}

			// Kill counts are kept as three digits per monster, in order
			std::string progress;
			auto iter = questlog.get_started().find(id);

			if (iter != questlog.get_started().end())
				progress = iter->second;

			size_t index = 0;

			for (auto mob : end["mob"])
			{
				int32_t mobid = mob["id"].get_integer();
				int32_t count = mob["count"].get_integer();

				if (mobid <= 0)
					continue;

				int32_t killed = 0;
				size_t offset = index * 3;

				if (offset + 3 <= progress.size() && std::all_of(progress.begin() + offset, progress.begin() + offset + 3, ::isdigit))
					killed = std::stoi(progress.substr(offset, 3));

				index++;

				std::string mob_name = nl::nx::string["Mob.img"][std::to_string(mobid)]["name"].get_string();
				bool done = killed >= count;

				needed.push_back({ Text(Text::Font::A12M, Text::Alignment::LEFT, done ? Color::Name::CHARTREUSE : Color::Name::WHITE,
					(mob_name.empty() ? "Monster " + std::to_string(mobid) : mob_name) + "  " + std::to_string(std::min(killed, count)) + " / " + std::to_string(count), width, false), done });
			}
		}

		if (tab != DONE)
		{
			nl::node act = nl::nx::quest["Act.img"][qid]["1"];

			if (int32_t exp = act["exp"].get_integer(); exp > 0)
			{
				std::string text = std::to_string(exp);
				string_format::split_number(text);
				rewards.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, "EXP  +" + text);
			}

			if (int32_t money = act["money"].get_integer(); money > 0)
			{
				std::string text = std::to_string(money);
				string_format::split_number(text);
				rewards.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, "Meso  +" + text);
			}

			if (int32_t fame = act["pop"].get_integer(); fame > 0)
				rewards.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, "Fame  +" + std::to_string(fame));

			for (auto item : act["item"])
			{
				int32_t itemid = item["id"].get_integer();
				int32_t count = item["count"].get_integer();

				// Skip items taken away and job-specific or random picks
				if (itemid <= 0 || count <= 0 || item["prop"].get_integer() != 0 || item["job"])
					continue;

				std::string item_name = ItemData::get(itemid).get_name();

				if (!item_name.empty())
					rewards.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, item_name + (count > 1 ? "  x" + std::to_string(count) : ""), width, false);
			}
		}
	}

	Rectangle<int16_t> UIMobileQuests::row_rect(const Layout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - first_row) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 8, ROW_H);
	}

	void UIMobileQuests::clamp_scroll()
	{
		Layout l = layout();
		int16_t max_first = std::max<int16_t>(0, static_cast<int16_t>(entries[tab].size()) - l.rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);
	}

	void UIMobileQuests::open_classic()
	{
		deactivate();
		UI::get().emplace<UIQuestLog>(questlog);
	}

	void UIMobileQuests::draw_label(const Text& text, Rectangle<int16_t> box) const
	{
		text.draw(Point<int16_t>(box.left() + box.width() / 2, box.top() + (box.height() - text.height()) / 2));
	}

	void UIMobileQuests::draw(float inter) const
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

		for (size_t i = 0; i < NUM_TABS; i++)
		{
			bool lit = i == tab;
			classic::cell(l.tabs[i].get_left_top(), l.tabs[i].width(), l.tabs[i].height(), lit);
			tab_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			draw_label(tab_labels[i], l.tabs[i]);
		}

		// The quest rows
		auto& list = entries[tab];
		int16_t last = std::min<int16_t>(static_cast<int16_t>(list.size()), first_row + l.rows);

		if (list.empty())
			draw_label(empty_list, rect(l.list.left(), l.list.top(), l.list.width(), 60));

		for (int16_t i = first_row; i < last; i++)
		{
			Entry& entry = list[i];
			Rectangle<int16_t> r = row_rect(l, i);
			bool lit = entry.id == selected;

			classic::cell(r.get_left_top(), r.width(), r.height(), lit);

			entry.name.change_color(lit ? Color::Name::HUD_PANEL : (tab == DONE ? Color::Name::SILVERCHALICE : Color::Name::WHITE));
			entry.name.draw(Point<int16_t>(r.left() + 10, r.top() + (r.height() - entry.name.height()) / 2));

			if (!entry.note.get_text().empty())
			{
				entry.note.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::SILVERCHALICE);
				entry.note.draw(Point<int16_t>(r.right() - 8, r.top() + (r.height() - entry.note.height()) / 2));
			}
		}

		int16_t count = static_cast<int16_t>(list.size());

		if (count > l.rows)
		{
			int16_t track_h = l.list.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / count);
			int16_t thumb_y = l.list.top() + (track_h - thumb_h) * first_row / std::max<int16_t>(1, count - l.rows);
			int16_t bar_x = l.list.right() - 4;

			classic::box(bar_x, l.list.top(), 4, track_h, Color::Name::HUD_TRACK, 0.9f);
			classic::box(bar_x, thumb_y, 4, thumb_h, Color::Name::HUD_EXP);
		}

		// The selected quest
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		if (selected == 0)
		{
			empty_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 20));
		}
		else
		{
			int16_t x = l.detail.left() + 10;
			int16_t y = l.detail.top() + 8;
			// Text past here would run under Give up or out of the box
			int16_t bottom = tab == IN_PROGRESS ? l.giveup.top() - 4 : l.detail.bottom() - 6;

			auto place = [&](const Text& text, int16_t gap)
			{
				if (text.get_text().empty() || y + text.height() > bottom)
					return;

				text.draw(Point<int16_t>(x, y));
				y += text.height() + gap;
			};

			place(name, 2);
			place(npc, 8);
			place(desc, 8);

			if (!needed.empty() && y + 20 < bottom)
			{
				place(needed_title, 2);

				for (const Line& line : needed)
					place(line.text, 1);

				y += 6;
			}

			if (!rewards.empty() && y + 20 < bottom)
			{
				place(rewards_title, 2);

				for (const Text& reward : rewards)
					place(reward, 1);
			}

			if (tab == IN_PROGRESS)
			{
				classic::button(l.giveup.get_left_top(), l.giveup.width(), l.giveup.height());
				draw_label(giveup_label, l.giveup);
			}
		}

		UIElement::draw(inter);
	}

	void UIMobileQuests::update()
	{
		UIElement::update();

		// A quest started, finished or moved on, or a level gained: read it all again
		if (signature() != loaded_signature || player_level() != loaded_level)
		{
			int16_t keep = selected;
			load();

			auto& list = entries[tab];
			bool still_listed = std::any_of(list.begin(), list.end(), [keep](const Entry& e) { return e.id == keep; });
			select(still_listed ? keep : 0);
		}
		else if (tab == IN_PROGRESS && selected != 0 && ++refresh_ticks >= 60)
		{
			// Items picked up or dropped change what is still needed
			refresh_ticks = 0;
			select(selected);
		}
	}

	Cursor::State UIMobileQuests::send_cursor(bool clicked, Point<int16_t> cursorpos)
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

		for (size_t i = 0; i < NUM_TABS; i++)
		{
			if (l.tabs[i].contains(cursorpos))
			{
				if (tab != i)
				{
					tab = i;
					first_row = 0;
					select(0);
				}

				return Cursor::State::CLICKING;
			}
		}

		if (l.list.contains(cursorpos))
		{
			auto& list = entries[tab];
			int16_t last = std::min<int16_t>(static_cast<int16_t>(list.size()), first_row + l.rows);

			for (int16_t i = first_row; i < last; i++)
			{
				if (row_rect(l, i).contains(cursorpos))
				{
					select(list[i].id);
					return Cursor::State::CLICKING;
				}
			}

			return Cursor::State::IDLE;
		}

		if (tab == IN_PROGRESS && selected != 0 && l.giveup.contains(cursorpos))
		{
			int16_t questid = selected;

			UI::get().emplace<UIYesNo>("Give up \"" + name.get_text() + "\"? Its progress will be lost.", [questid](bool yes)
			{
				if (yes)
					ForfeitQuestPacket(questid).dispatch();
			});

			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}

	void UIMobileQuests::send_scroll(double yoffset)
	{
		gesture.cancel();

		first_row += yoffset > 0 ? -1 : 1;
		clamp_scroll();
	}

	void UIMobileQuests::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIMobileQuests::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIMobileQuests::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileQuests::get_type() const
	{
		return TYPE;
	}
}
