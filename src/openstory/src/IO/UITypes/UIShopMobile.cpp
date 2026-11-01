//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
// The NPC shop's full-screen layout for phones. Only built for Android; the
// desktop keeps the classic window in UIShop.cpp.
#ifdef __ANDROID__

#include "UIShop.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/ClassicStyle.h"
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
		constexpr int16_t FOOT_H = 28;
		constexpr int16_t DETAIL_W = 230;

		const InventoryType::Id SELL_TABS[] = {
			InventoryType::Id::EQUIP, InventoryType::Id::USE, InventoryType::Id::SETUP,
			InventoryType::Id::ETC, InventoryType::Id::CASH
		};

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		const char* category_name(InventoryType::Id type)
		{
			switch (type)
			{
			case InventoryType::Id::EQUIP: return "Equip";
			case InventoryType::Id::USE: return "Use";
			case InventoryType::Id::SETUP: return "Set-up";
			case InventoryType::Id::ETC: return "Etc";
			case InventoryType::Id::CASH: return "Cash";
			default: return "All";
			}
		}

		void draw_centered(const Text& text, Rectangle<int16_t> box)
		{
			text.draw(Point<int16_t>(box.left() + box.width() / 2, box.top() + (box.height() - text.height()) / 2));
		}
	}

	void UIShop::init_mobile()
	{
		m_title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Shop");
		m_leave = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Leave shop");
		m_buy = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Buy");
		m_sell_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Sell");

		for (auto& tab : m_tabs)
			tab = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE);

		m_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap an item to see it here", DETAIL_W - 20);
		m_empty = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Nothing here");
		m_action = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE);
		m_detail_name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		m_detail_desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", DETAIL_W - 20);
		m_detail_price = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", DETAIL_W - 20);

		// The meso count sits at the bottom left
		mesolabel = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP);
	}

	bool UIShop::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	UIShop::MobileLayout UIShop::mobile_layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		MobileLayout l;
		int16_t pw = std::min<int16_t>(vw - 24, 760);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = rect(px, py, pw, ph);
		l.buy_mode = rect(px + MARGIN, py + 8, 84, 30);
		l.sell_mode = rect(l.buy_mode.right() + GAP, py + 8, 84, 30);
		l.leave = rect(px + pw - MARGIN - 116, py + 8, 116, 30);

		int16_t tab_y = py + TOP_H + 8;
		int16_t tab_w = std::min<int16_t>(100, (pw - MARGIN * 2 - GAP * (MOBILE_TABS - 1)) / MOBILE_TABS);

		for (size_t i = 0; i < MOBILE_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		int16_t body_y = tab_y + TAB_H + 10;
		int16_t body_h = py + ph - FOOT_H - 6 - body_y;

		l.list = rect(px + MARGIN, body_y, pw - MARGIN * 3 - DETAIL_W, body_h);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / (ROW_H + GAP));
		l.detail = rect(px + pw - MARGIN - DETAIL_W, body_y, DETAIL_W, body_h);
		l.action = rect(l.detail.left() + 10, l.detail.bottom() - 50, DETAIL_W - 20, 40);

		return l;
	}

	std::vector<InventoryType::Id> UIShop::mobile_categories() const
	{
		if (m_sell)
			return std::vector<InventoryType::Id>(std::begin(SELL_TABS), std::end(SELL_TABS));

		// All, then only the kinds this shop sells
		std::vector<InventoryType::Id> categories = { InventoryType::Id::NONE };

		for (InventoryType::Id type : SELL_TABS)
		{
			bool sold = std::any_of(buystate.all_items.begin(), buystate.all_items.end(),
				[type](const BuyItem& item) { return InventoryType::by_item_id(item.get_id()) == type; });

			if (sold)
				categories.push_back(type);
		}

		return categories;
	}

	int16_t UIShop::mobile_count() const
	{
		return m_sell ? static_cast<int16_t>(sellstate.items.size()) : static_cast<int16_t>(buystate.items.size());
	}

	int32_t UIShop::mobile_item(int16_t index) const
	{
		if (index < 0 || index >= mobile_count())
			return 0;

		return m_sell ? sellstate.items[index].get_id() : buystate.items[index].get_id();
	}

	int16_t UIShop::mobile_selection() const
	{
		int16_t selection = m_sell ? sellstate.selection : buystate.selection;

		// Selling can shrink the list under the selection
		return selection >= 0 && selection < mobile_count() ? selection : -1;
	}

	Rectangle<int16_t> UIShop::mobile_row(const MobileLayout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - m_first) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 8, ROW_H);
	}

	void UIShop::mobile_clamp()
	{
		MobileLayout l = mobile_layout();
		int16_t max_first = std::max<int16_t>(0, mobile_count() - l.rows);
		m_first = std::clamp<int16_t>(m_first, 0, max_first);
	}

	const Text& UIShop::mobile_name(int32_t itemid) const
	{
		auto iter = m_names.find(itemid);

		if (iter == m_names.end())
		{
			Text text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE, ItemData::get(itemid).get_name());

			// Cut long names to the row
			std::string name = text.get_text();
			int16_t width = mobile_layout().list.width() - ROW_H - 90;

			while (text.width() > width && name.size() > 4)
			{
				name.resize(name.size() - 3);
				text.change_text(name + "...");
			}

			iter = m_names.emplace(itemid, std::move(text)).first;
		}

		return iter->second;
	}

	const Text& UIShop::mobile_price(int16_t index) const
	{
		int32_t itemid = mobile_item(index);
		int32_t price;
		std::string suffix = " meso";

		if (m_sell)
		{
			price = ItemData::get(itemid).get_price();
		}
		else
		{
			const BuyItem& item = buystate.items[index];
			price = item.get_price() == 0 && item.get_chargeprice() > 0 ? item.get_chargeprice() : item.get_price();

			if (currency_item > 0)
				suffix = " " + currency_name;
		}

		// Selling prices and buying prices are kept apart
		auto key = std::make_pair(itemid, m_sell ? -1 - price : price);
		auto iter = m_prices.find(key);

		if (iter == m_prices.end())
		{
			std::string text = std::to_string(price);
			string_format::split_number(text);
			iter = m_prices.emplace(key, Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::HUD_EXP, text + suffix)).first;
		}

		return iter->second;
	}

	void UIShop::mobile_draw_icon(int32_t itemid, Rectangle<int16_t> box) const
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

	void UIShop::draw_mobile(float alpha) const
	{
		MobileLayout l = mobile_layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());
		m_title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 10));

		classic::cell(l.buy_mode.get_left_top(), l.buy_mode.width(), l.buy_mode.height(), !m_sell);
		classic::cell(l.sell_mode.get_left_top(), l.sell_mode.width(), l.sell_mode.height(), m_sell);
		m_buy.change_color(!m_sell ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		m_sell_label.change_color(m_sell ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		draw_centered(m_buy, l.buy_mode);
		draw_centered(m_sell_label, l.sell_mode);

		classic::button(l.leave.get_left_top(), l.leave.width(), l.leave.height());
		draw_centered(m_leave, l.leave);

		// Category tabs
		std::vector<InventoryType::Id> categories = mobile_categories();
		InventoryType::Id current = m_sell ? sellstate.tab : buystate.filter;

		for (size_t i = 0; i < categories.size() && i < MOBILE_TABS; i++)
		{
			bool lit = categories[i] == current;
			const Rectangle<int16_t>& r = l.tabs[i];

			if (m_tabs[i].get_text() != category_name(categories[i]))
				m_tabs[i].change_text(category_name(categories[i]));

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
			name.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 6));

			Text& price = const_cast<Text&>(mobile_price(i));
			price.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::HUD_EXP);
			price.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 27));

			// How many of it the character has, for things that stack
			if (m_sell && sellstate.tab != InventoryType::Id::EQUIP)
			{
				int16_t have = sellstate.items[i].get_sellable();
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

		mesolabel.draw(Point<int16_t>(l.list.left() + 4, l.panel.bottom() - FOOT_H));

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

			std::string each = (m_sell ? "Sells for " : "Costs ") + mobile_price(selection).get_text() + (m_sell ? " each" : "");

			if (m_detail_price.get_text() != each)
				m_detail_price.change_text(each);

			Rectangle<int16_t> big = rect(l.detail.left() + 10, l.detail.top() + 10, 64, 64);
			mobile_draw_icon(itemid, big);

			int16_t y = big.bottom() + 8;
			m_detail_name.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_name.height() + 4;
			m_detail_price.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_price.height() + 8;

			if (y + m_detail_desc.height() < l.action.top() - 6)
				m_detail_desc.draw(Point<int16_t>(l.detail.left() + 10, y));

			m_action.change_text(m_sell ? "Sell" : "Buy");
			classic::button(l.action.get_left_top(), l.action.width(), l.action.height(), true);
			draw_centered(m_action, l.action);
		}

		UIElement::draw(alpha);
	}

	Cursor::State UIShop::send_cursor_mobile(bool clicked, Point<int16_t> cursorpos)
	{
		// The game sends clicked on every move while a finger is down; act on
		// the moment it lands only.
		bool tap = clicked && !m_was_down;
		m_was_down = clicked;

		if (!tap)
			return Cursor::State::IDLE;

		MobileLayout l = mobile_layout();

		if (l.leave.contains(cursorpos))
		{
			exit_shop();
			return Cursor::State::IDLE;
		}

		if (l.buy_mode.contains(cursorpos) || l.sell_mode.contains(cursorpos))
		{
			bool sell = l.sell_mode.contains(cursorpos);

			if (sell != m_sell)
			{
				m_sell = sell;
				m_first = 0;
				m_counts.clear();
				buystate.selection = -1;
				sellstate.selection = -1;
			}

			return Cursor::State::CLICKING;
		}

		std::vector<InventoryType::Id> categories = mobile_categories();

		for (size_t i = 0; i < categories.size() && i < MOBILE_TABS; i++)
		{
			if (!l.tabs[i].contains(cursorpos))
				continue;

			if (m_sell)
				changeselltab(categories[i]);
			else
				changebuytab(categories[i]);

			sellstate.selection = -1;
			m_first = 0;
			m_counts.clear();

			return Cursor::State::CLICKING;
		}

		if (l.list.contains(cursorpos))
		{
			int16_t last = std::min<int16_t>(mobile_count(), m_first + l.rows);

			for (int16_t i = m_first; i < last; i++)
			{
				if (!mobile_row(l, i).contains(cursorpos))
					continue;

				// A second tap on the selected item buys or sells it
				if (m_sell)
				{
					if (sellstate.selection == i)
						sellstate.sell(false);
					else
						sellstate.selection = i;

					buystate.selection = -1;
				}
				else
				{
					if (buystate.selection == i)
						buystate.buy();
					else
						buystate.selection = i;

					sellstate.selection = -1;
				}

				return Cursor::State::CLICKING;
			}

			return Cursor::State::IDLE;
		}

		if (mobile_selection() >= 0 && l.action.contains(cursorpos))
		{
			if (m_sell)
				sellstate.sell(false);
			else
				buystate.buy();

			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}
}

#endif
