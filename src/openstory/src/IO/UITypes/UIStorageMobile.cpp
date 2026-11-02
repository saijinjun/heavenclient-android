//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
// The storage keeper's full-screen layout for phones, built like the NPC
// shop's (UIShopMobile.cpp). Only built for Android; the desktop keeps the
// two-grid drag-and-drop window in UIStorage.cpp.
#ifdef __ANDROID__

#include "UIStorage.h"

#include "UINotice.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Graphics/Geometry.h"
#include "../../Util/Misc.h"

#include <algorithm>

namespace ms
{
	namespace
	{
		constexpr int16_t MARGIN = 12;
		constexpr int16_t TOP_H = 40;
		constexpr int16_t TAB_H = 32;
		constexpr int16_t ROW_H = 50;
		constexpr int16_t GAP = 6;
		constexpr int16_t FOOT_H = 40;
		constexpr int16_t DETAIL_W = 230;

		const InventoryType::Id TABS[UIStorage::MOBILE_TABS] = {
			InventoryType::Id::EQUIP, InventoryType::Id::USE, InventoryType::Id::SETUP,
			InventoryType::Id::ETC, InventoryType::Id::CASH
		};

		const char* TAB_NAMES[UIStorage::MOBILE_TABS] = { "Equip", "Use", "Set-up", "Etc", "Cash" };

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		void draw_centered(const Text& text, Rectangle<int16_t> box)
		{
			text.draw(Point<int16_t>(box.left() + box.width() / 2, box.top() + (box.height() - text.height()) / 2));
		}

		std::string with_commas(int64_t value)
		{
			std::string text = std::to_string(value);
			string_format::split_number(text);
			return text;
		}
	}

	void UIStorage::init_mobile()
	{
		m_title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Storage");
		m_close = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Close");
		m_take_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Take out");
		m_store_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Store");
		m_sort_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Sort");
		m_meso_in = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Deposit mesos");
		m_meso_out = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Withdraw mesos");

		for (size_t i = 0; i < MOBILE_TABS; i++)
			m_tabs[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, TAB_NAMES[i]);

		m_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap an item, then tap it again or press the button", DETAIL_W - 20);
		m_empty = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Nothing here");
		m_action = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE);
		m_detail_name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		m_detail_desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", DETAIL_W - 20);
		m_detail_count = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", DETAIL_W - 20);
		m_footer = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::HUD_EXP);
	}

	UIStorage::MobileLayout UIStorage::mobile_layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		MobileLayout l;
		int16_t pw = std::min<int16_t>(vw - 24, 760);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = rect(px, py, pw, ph);
		l.take_mode = rect(px + MARGIN, py + 8, 96, 30);
		l.store_mode = rect(l.take_mode.right() + GAP, py + 8, 96, 30);
		l.close = rect(px + pw - MARGIN - 96, py + 8, 96, 30);
		l.sort = rect(l.close.left() - GAP - 70, py + 8, 70, 30);

		int16_t tab_y = py + TOP_H + 8;
		int16_t tab_w = std::min<int16_t>(100, (pw - MARGIN * 2 - GAP * (MOBILE_TABS - 1)) / MOBILE_TABS);

		for (size_t i = 0; i < MOBILE_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		int16_t body_y = tab_y + TAB_H + 10;
		int16_t body_h = py + ph - FOOT_H - 8 - body_y;

		l.list = rect(px + MARGIN, body_y, pw - MARGIN * 3 - DETAIL_W, body_h);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / (ROW_H + GAP));
		l.detail = rect(px + pw - MARGIN - DETAIL_W, body_y, DETAIL_W, body_h);
		l.action = rect(l.detail.left() + 10, l.detail.bottom() - 50, DETAIL_W - 20, 40);

		int16_t foot_y = py + ph - FOOT_H;
		l.meso_out = rect(px + pw - MARGIN - 130, foot_y, 130, 32);
		l.meso_in = rect(l.meso_out.left() - GAP - 130, foot_y, 130, 32);

		return l;
	}

	int16_t UIStorage::mobile_count() const
	{
		return m_store_mode
			? static_cast<int16_t>(inventory_items.size())
			: static_cast<int16_t>(get_storage_tab(current_tab).size());
	}

	int32_t UIStorage::mobile_item(int16_t index) const
	{
		if (index < 0 || index >= mobile_count())
			return 0;

		return m_store_mode ? inventory_items[index].itemid : get_storage_tab(current_tab)[index].itemid;
	}

	int16_t UIStorage::mobile_item_count(int16_t index) const
	{
		if (index < 0 || index >= mobile_count())
			return 0;

		return m_store_mode ? inventory_items[index].count : get_storage_tab(current_tab)[index].count;
	}

	int16_t UIStorage::mobile_selection() const
	{
		// Taking out or storing shrinks the list under the selection
		return m_selected >= 0 && m_selected < mobile_count() ? m_selected : -1;
	}

	Rectangle<int16_t> UIStorage::mobile_row(const MobileLayout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - m_first) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 8, ROW_H);
	}

	void UIStorage::mobile_clamp()
	{
		MobileLayout l = mobile_layout();
		int16_t max_first = std::max<int16_t>(0, mobile_count() - l.rows);
		m_first = std::clamp<int16_t>(m_first, 0, max_first);
	}

	const Text& UIStorage::mobile_name(int32_t itemid) const
	{
		auto iter = m_names.find(itemid);

		if (iter == m_names.end())
		{
			Text text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE, ItemData::get(itemid).get_name());

			// Cut long names to the row
			std::string name = text.get_text();
			int16_t width = mobile_layout().list.width() - ROW_H - 70;

			while (text.width() > width && name.size() > 4)
			{
				name.resize(name.size() - 3);
				text.change_text(name + "...");
			}

			iter = m_names.emplace(itemid, std::move(text)).first;
		}

		return iter->second;
	}

	void UIStorage::mobile_draw_icon(int32_t itemid, Rectangle<int16_t> box) const
	{
		const Texture& icon = ItemData::get(itemid).get_icon(false);

		if (!icon.is_valid())
			return;

		Point<int16_t> dim = icon.get_dimensions();
		float scale = std::min(box.width() * 0.8f / std::max<int16_t>(dim.x(), 1), box.height() * 0.8f / std::max<int16_t>(dim.y(), 1));
		scale = std::min(scale, 2.5f);

		Point<int16_t> origin = icon.get_origin();
		int16_t x = box.left() + (box.width() - static_cast<int16_t>(dim.x() * scale)) / 2 + static_cast<int16_t>(origin.x() * scale);
		int16_t y = box.top() + (box.height() - static_cast<int16_t>(dim.y() * scale)) / 2 + static_cast<int16_t>(origin.y() * scale);

		icon.draw(DrawArgument(Point<int16_t>(x, y), scale, scale));
	}

	void UIStorage::mobile_act(int16_t index)
	{
		if (index < 0 || index >= mobile_count())
			return;

		if (m_store_mode)
		{
			const InventoryEntry& entry = inventory_items[index];

			// Stacks ask how many first
			request_store(entry.slot, entry.itemid, entry.count);
			return;
		}

		const ItemEntry& entry = get_storage_tab(current_tab)[index];

		if (current_tab != InventoryType::Id::EQUIP && entry.count > 1)
		{
			auto on_enter = [this, index](int32_t quantity)
			{
				request_take_out_partial(index, static_cast<int16_t>(quantity));
			};

			UI::get().emplace<UIEnterNumber>("How many would you like to take out?", on_enter, entry.count, entry.count, -41);
			return;
		}

		request_take_out(index);
	}

	void UIStorage::draw_mobile(float) const
	{
		MobileLayout l = mobile_layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());
		m_title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 10));

		classic::cell(l.take_mode.get_left_top(), l.take_mode.width(), l.take_mode.height(), !m_store_mode);
		classic::cell(l.store_mode.get_left_top(), l.store_mode.width(), l.store_mode.height(), m_store_mode);
		m_take_label.change_color(!m_store_mode ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		m_store_label.change_color(m_store_mode ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		draw_centered(m_take_label, l.take_mode);
		draw_centered(m_store_label, l.store_mode);

		if (!m_store_mode)
		{
			classic::button(l.sort.get_left_top(), l.sort.width(), l.sort.height());
			draw_centered(m_sort_label, l.sort);
		}

		classic::button(l.close.get_left_top(), l.close.width(), l.close.height());
		draw_centered(m_close, l.close);

		for (size_t i = 0; i < MOBILE_TABS; i++)
		{
			bool lit = TABS[i] == current_tab;
			const Rectangle<int16_t>& r = l.tabs[i];

			classic::cell(r.get_left_top(), r.width(), r.height(), lit);
			m_tabs[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			draw_centered(m_tabs[i], r);
		}

		// The rows
		int16_t count = mobile_count();
		int16_t selection = mobile_selection();
		int16_t last = std::min<int16_t>(count, m_first + l.rows);

		if (count == 0)
			draw_centered(m_empty, rect(l.list.left(), l.list.top(), l.list.width(), 60));

		for (int16_t i = m_first; i < last; i++)
		{
			int32_t itemid = mobile_item(i);
			Rectangle<int16_t> r = mobile_row(l, i);
			bool lit = i == selection;

			classic::cell(r.get_left_top(), r.width(), r.height(), lit);
			mobile_draw_icon(itemid, rect(r.left() + 3, r.top() + 3, ROW_H - 6, ROW_H - 6));

			Text& name = const_cast<Text&>(mobile_name(itemid));
			name.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			name.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + (r.height() - name.height()) / 2));

			if (current_tab != InventoryType::Id::EQUIP)
			{
				int16_t have = mobile_item_count(i);
				auto& entry = m_counts[i];

				if (entry.first != have || entry.second.get_text().empty())
				{
					entry.first = have;
					entry.second = Text(Text::Font::A13B, Text::Alignment::RIGHT, Color::Name::WHITE, "x" + std::to_string(have));
				}

				entry.second.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
				entry.second.draw(Point<int16_t>(r.right() - 10, r.top() + (r.height() - entry.second.height()) / 2));
			}
		}

		if (count > l.rows)
		{
			int16_t track_h = l.list.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / count);
			int16_t thumb_y = l.list.top() + (track_h - thumb_h) * m_first / std::max<int16_t>(1, count - l.rows);
			int16_t bar_x = l.list.right() - 4;

			classic::box(bar_x, l.list.top(), 4, track_h, Color::Name::HUD_TRACK, 0.9f);
			classic::box(bar_x, thumb_y, 4, thumb_h, Color::Name::HUD_EXP);
		}

		// Footer: slots used and both meso counts, with the meso buttons
		size_t used = storage_equip.size() + storage_use.size() + storage_setup.size() + storage_etc.size() + storage_cash.size();
		std::string footer = "Slots " + std::to_string(used) + "/" + std::to_string(storage_slots)
			+ "   Stored " + with_commas(storage_meso) + "   Carrying " + with_commas(inventory.get_meso()) + " meso";

		if (m_footer.get_text() != footer)
			m_footer.change_text(footer);

		m_footer.draw(Point<int16_t>(l.list.left() + 4, l.meso_in.top() + (l.meso_in.height() - m_footer.height()) / 2));

		classic::button(l.meso_in.get_left_top(), l.meso_in.width(), l.meso_in.height());
		draw_centered(m_meso_in, l.meso_in);
		classic::button(l.meso_out.get_left_top(), l.meso_out.width(), l.meso_out.height());
		draw_centered(m_meso_out, l.meso_out);

		// The selected item
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		if (selection < 0)
		{
			m_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 20));
		}
		else
		{
			int32_t itemid = mobile_item(selection);
			const ItemData& data = ItemData::get(itemid);

			if (m_shown != itemid)
			{
				m_shown = itemid;
				m_detail_name.change_text(data.get_name());

				std::string desc = data.get_desc();

				if (desc.size() > 240)
				{
					size_t cut = desc.rfind(' ', 240);
					desc = desc.substr(0, cut == std::string::npos ? 240 : cut) + " ...";
				}

				m_detail_desc.change_text(desc);
			}

			std::string have = (m_store_mode ? "You carry " : "In storage: ") + with_commas(mobile_item_count(selection));

			if (m_detail_count.get_text() != have)
				m_detail_count.change_text(have);

			Rectangle<int16_t> big = rect(l.detail.left() + 10, l.detail.top() + 10, 64, 64);
			mobile_draw_icon(itemid, big);

			int16_t y = big.bottom() + 8;
			m_detail_name.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_name.height() + 4;
			m_detail_count.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_count.height() + 8;

			if (y + m_detail_desc.height() < l.action.top() - 6)
				m_detail_desc.draw(Point<int16_t>(l.detail.left() + 10, y));

			m_action.change_text(m_store_mode ? "Store" : "Take out");
			classic::button(l.action.get_left_top(), l.action.width(), l.action.height(), true);
			draw_centered(m_action, l.action);
		}

		// Not UIElement::draw: that would paint the desktop window's art and
		// buttons over this layout
	}

	Cursor::State UIStorage::send_cursor_mobile(bool clicked, Point<int16_t> cursorpos)
	{
		// Stay on top and keep the touches after a dialog over this closes
		UI::get().bring_to_front(TYPE, true);

		// Act when the finger lifts where it landed, so a swipe to scroll
		// never presses what it started on
		Point<int16_t> at;

		if (!m_gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		cursorpos = at;

		MobileLayout l = mobile_layout();

		if (l.close.contains(cursorpos))
		{
			close_with_packet();
			return Cursor::State::IDLE;
		}

		if (l.take_mode.contains(cursorpos) || l.store_mode.contains(cursorpos))
		{
			bool store = l.store_mode.contains(cursorpos);

			if (store != m_store_mode)
			{
				m_store_mode = store;
				m_first = 0;
				m_selected = -1;
				m_counts.clear();
			}

			return Cursor::State::CLICKING;
		}

		if (!m_store_mode && l.sort.contains(cursorpos))
		{
			request_sort();
			return Cursor::State::CLICKING;
		}

		if (l.meso_in.contains(cursorpos) || l.meso_out.contains(cursorpos))
		{
			request_meso_change(l.meso_in.contains(cursorpos));
			return Cursor::State::CLICKING;
		}

		for (size_t i = 0; i < MOBILE_TABS; i++)
		{
			if (!l.tabs[i].contains(cursorpos))
				continue;

			if (TABS[i] != current_tab)
			{
				change_tab(TABS[i]);
				m_first = 0;
				m_selected = -1;
				m_counts.clear();
			}

			return Cursor::State::CLICKING;
		}

		if (l.list.contains(cursorpos))
		{
			int16_t last = std::min<int16_t>(mobile_count(), m_first + l.rows);

			for (int16_t i = m_first; i < last; i++)
			{
				if (!mobile_row(l, i).contains(cursorpos))
					continue;

				// A second tap on the selected item takes it out or stores it
				if (m_selected == i)
					mobile_act(i);
				else
					m_selected = i;

				return Cursor::State::CLICKING;
			}

			return Cursor::State::IDLE;
		}

		if (mobile_selection() >= 0 && l.action.contains(cursorpos))
		{
			mobile_act(mobile_selection());
			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}
}

#endif
