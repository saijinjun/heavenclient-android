//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIDuey.h"

#include "UINotice.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Graphics/Geometry.h"
#include "../../Net/Packets/DueyPackets.h"
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

		const InventoryType::Id TABS[] = {
			InventoryType::Id::EQUIP, InventoryType::Id::USE, InventoryType::Id::SETUP,
			InventoryType::Id::ETC, InventoryType::Id::CASH
		};

		const char* TAB_NAMES[] = { "Equip", "Use", "Set-up", "Etc", "Cash" };

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

	int32_t UIDuey::fee_for(int32_t mesos)
	{
		int64_t m = mesos;
		int64_t tax = 0;

		if (m >= 100000000)
			tax = m * 6 / 100;
		else if (m >= 25000000)
			tax = m * 5 / 100;
		else if (m >= 10000000)
			tax = m * 4 / 100;
		else if (m >= 5000000)
			tax = m * 3 / 100;
		else if (m >= 1000000)
			tax = m * 18 / 1000;
		else if (m >= 100000)
			tax = m * 8 / 1000;

		return static_cast<int32_t>(5000 + tax);
	}

	UIDuey::UIDuey(const Inventory& inv, std::vector<Package> pkgs)
		: UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), inventory(inv), packages(std::move(pkgs))
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Duey");
		close_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Close");
		recv_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Receive");
		send_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Send");
		mesos_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Send mesos");

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, TAB_NAMES[i]);

		hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap a row to see it here", DETAIL_W - 20);
		empty = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Nothing here");
		action_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE);
		action2_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
		detail_name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		detail_line = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", DETAIL_W - 20);
		detail_note = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", DETAIL_W - 20);
		footer = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::HUD_EXP);

		refresh_inventory();
		active = true;
	}

	UIDuey::Layout UIDuey::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 760);
		int16_t ph = std::min<int16_t>(vh - 24, 560);
		int16_t px = (vw - pw) / 2;
		int16_t py = (vh - ph) / 2;

		l.panel = rect(px, py, pw, ph);
		l.recv_mode = rect(px + MARGIN, py + 8, 96, 30);
		l.send_mode = rect(l.recv_mode.right() + GAP, py + 8, 96, 30);
		l.close = rect(px + pw - MARGIN - 96, py + 8, 96, 30);

		int16_t tab_y = py + TOP_H + 8;
		int16_t tab_w = std::min<int16_t>(100, (pw - MARGIN * 2 - GAP * (NUM_TABS - 1)) / NUM_TABS);

		for (size_t i = 0; i < NUM_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		// Receive mode has no tabs, so its list starts higher
		int16_t body_y = send_mode ? tab_y + TAB_H + 10 : tab_y;
		int16_t body_h = py + ph - FOOT_H - 8 - body_y;

		l.list = rect(px + MARGIN, body_y, pw - MARGIN * 3 - DETAIL_W, body_h);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / (ROW_H + GAP));
		l.detail = rect(px + pw - MARGIN - DETAIL_W, body_y, DETAIL_W, body_h);
		l.action = rect(l.detail.left() + 10, l.detail.bottom() - 50, DETAIL_W - 20, 40);
		l.action2 = rect(l.detail.left() + 10, l.action.top() - 38, DETAIL_W - 20, 32);
		l.send_mesos = rect(px + pw - MARGIN - 130, py + ph - FOOT_H, 130, 32);

		return l;
	}

	void UIDuey::refresh_inventory()
	{
		items.clear();

		int16_t slotmax = inventory.get_slotmax(tab);

		for (int16_t slot = 1; slot <= slotmax; slot++)
		{
			int32_t itemid = inventory.get_item_id(tab, slot);

			if (itemid == 0)
				continue;

			int16_t n = tab == InventoryType::Id::EQUIP ? 1 : inventory.get_item_count(tab, slot);
			items.push_back({ slot, itemid, n });
		}

		subtitles.clear();
		clamp_scroll();
	}

	int16_t UIDuey::count() const
	{
		return send_mode ? static_cast<int16_t>(items.size()) : static_cast<int16_t>(packages.size());
	}

	int16_t UIDuey::selection() const
	{
		return selected >= 0 && selected < count() ? selected : -1;
	}

	Rectangle<int16_t> UIDuey::row_rect(const Layout& l, int16_t index) const
	{
		int16_t y = l.list.top() + (index - first_row) * (ROW_H + GAP);
		return rect(l.list.left(), y, l.list.width() - 8, ROW_H);
	}

	void UIDuey::clamp_scroll()
	{
		Layout l = layout();
		int16_t max_first = std::max<int16_t>(0, count() - l.rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);
	}

	void UIDuey::draw_icon(int32_t itemid, Rectangle<int16_t> box) const
	{
		if (itemid == 0)
			return;

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

	const Text& UIDuey::item_name(int32_t itemid) const
	{
		auto iter = names.find(itemid);

		if (iter == names.end())
		{
			std::string name = itemid == 0 ? "Mesos" : ItemData::get(itemid).get_name();
			Text text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::WHITE, name);
			int16_t width = layout().list.width() - ROW_H - 70;

			while (text.width() > width && name.size() > 4)
			{
				name.resize(name.size() - 3);
				text.change_text(name + "...");
			}

			iter = names.emplace(itemid, std::move(text)).first;
		}

		return iter->second;
	}

	void UIDuey::draw(float) const
	{
		Layout l = layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 0.45f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());
		title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 10));

		classic::cell(l.recv_mode.get_left_top(), l.recv_mode.width(), l.recv_mode.height(), !send_mode);
		classic::cell(l.send_mode.get_left_top(), l.send_mode.width(), l.send_mode.height(), send_mode);
		recv_label.change_color(!send_mode ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		send_label.change_color(send_mode ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		draw_centered(recv_label, l.recv_mode);
		draw_centered(send_label, l.send_mode);

		classic::button(l.close.get_left_top(), l.close.width(), l.close.height());
		draw_centered(close_label, l.close);

		if (send_mode)
		{
			for (size_t i = 0; i < NUM_TABS; i++)
			{
				bool lit = TABS[i] == tab;
				const Rectangle<int16_t>& r = l.tabs[i];

				classic::cell(r.get_left_top(), r.width(), r.height(), lit);
				tab_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
				draw_centered(tab_labels[i], r);
			}
		}

		// The rows
		int16_t n = count();
		int16_t sel = selection();
		int16_t last = std::min<int16_t>(n, first_row + l.rows);

		if (n == 0)
			draw_centered(empty, rect(l.list.left(), l.list.top(), l.list.width(), 60));

		for (int16_t i = first_row; i < last; i++)
		{
			Rectangle<int16_t> r = row_rect(l, i);
			bool lit = i == sel;
			int32_t itemid = send_mode ? items[i].itemid : packages[i].itemid;

			classic::cell(r.get_left_top(), r.width(), r.height(), lit);
			draw_icon(itemid, rect(r.left() + 3, r.top() + 3, ROW_H - 6, ROW_H - 6));

			Text& name = const_cast<Text&>(item_name(itemid));
			name.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);

			// Second line: who sent it and what else is inside, or how many you carry
			std::string sub;
			int16_t amount = 0;

			if (send_mode)
			{
				amount = items[i].count;
			}
			else
			{
				const Package& p = packages[i];
				amount = p.itemid != 0 ? p.count : 0;
				sub = "From " + p.sender;

				if (p.itemid != 0 && p.mesos > 0)
					sub += " + " + with_commas(p.mesos) + " meso";
				else if (p.itemid == 0)
					sub += ": " + with_commas(p.mesos) + " meso";
			}

			if (sub.empty())
			{
				name.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + (r.height() - name.height()) / 2));
			}
			else
			{
				name.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 6));

				auto& entry = subtitles[i];
				int32_t key = static_cast<int32_t>(std::hash<std::string>()(sub));

				if (entry.first != key || entry.second.get_text().empty())
				{
					entry.first = key;
					entry.second = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::HUD_EXP, sub);
				}

				entry.second.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::HUD_EXP);
				entry.second.draw(Point<int16_t>(r.left() + ROW_H + 4, r.top() + 27));
			}

			if (amount > 1)
			{
				Text x(Text::Font::A13B, Text::Alignment::RIGHT, lit ? Color::Name::HUD_PANEL : Color::Name::WHITE, "x" + std::to_string(amount));
				x.draw(Point<int16_t>(r.right() - 10, r.top() + (r.height() - x.height()) / 2));
			}
		}

		if (n > l.rows)
		{
			int16_t track_h = l.list.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / n);
			int16_t thumb_y = l.list.top() + (track_h - thumb_h) * first_row / std::max<int16_t>(1, n - l.rows);
			int16_t bar_x = l.list.right() - 4;

			classic::box(bar_x, l.list.top(), 4, track_h, Color::Name::HUD_TRACK, 0.9f);
			classic::box(bar_x, thumb_y, 4, thumb_h, Color::Name::HUD_EXP);
		}

		// Footer
		std::string foot = send_mode
			? "Carrying " + with_commas(inventory.get_meso()) + " meso. Delivery costs 5,000 + tax"
			: std::to_string(packages.size()) + " package(s) waiting";

		if (footer.get_text() != foot)
			footer.change_text(foot);

		footer.draw(Point<int16_t>(l.list.left() + 4, l.send_mesos.top() + (l.send_mesos.height() - footer.height()) / 2));

		if (send_mode)
		{
			classic::button(l.send_mesos.get_left_top(), l.send_mesos.width(), l.send_mesos.height());
			draw_centered(mesos_label, l.send_mesos);
		}

		// The selected row
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		if (sel < 0)
		{
			hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 20));
			return;
		}

		int32_t itemid = send_mode ? items[sel].itemid : packages[sel].itemid;
		std::string name = itemid == 0 ? "Mesos" : ItemData::get(itemid).get_name();
		std::string line;
		std::string note;

		if (send_mode)
		{
			line = "You carry " + with_commas(items[sel].count);
			note = "Send it to any character by name, including your own.";
		}
		else
		{
			const Package& p = packages[sel];
			line = "From " + p.sender;

			if (p.itemid != 0 && p.count > 1)
				line += ", x" + std::to_string(p.count);

			if (p.mesos > 0)
				line += ", " + with_commas(p.mesos) + " meso";

			note = p.message;
		}

		if (detail_name.get_text() != name)
			detail_name.change_text(name);

		if (detail_line.get_text() != line)
			detail_line.change_text(line);

		if (detail_note.get_text() != note)
			detail_note.change_text(note);

		Rectangle<int16_t> big = rect(l.detail.left() + 10, l.detail.top() + 10, 64, 64);
		draw_icon(itemid, big);

		int16_t y = big.bottom() + 8;
		detail_name.draw(Point<int16_t>(l.detail.left() + 10, y));
		y += detail_name.height() + 4;
		detail_line.draw(Point<int16_t>(l.detail.left() + 10, y));
		y += detail_line.height() + 8;

		if (!note.empty() && y + detail_note.height() < l.action2.top() - 6)
			detail_note.draw(Point<int16_t>(l.detail.left() + 10, y));

		if (!send_mode)
		{
			action2_label.change_text("Delete");
			classic::button(l.action2.get_left_top(), l.action2.width(), l.action2.height());
			draw_centered(action2_label, l.action2);
		}

		action_label.change_text(send_mode ? "Send" : "Claim");
		classic::button(l.action.get_left_top(), l.action.width(), l.action.height(), true);
		draw_centered(action_label, l.action);
	}

	void UIDuey::update()
	{
		// Keep the Send list in step with the inventory
		if (send_mode && ++refresh_ticks >= 30)
		{
			refresh_ticks = 0;
			refresh_inventory();
		}
	}

	void UIDuey::ask_recipient(int8_t invtype, int16_t slot, int16_t amount, int32_t mesos, const std::string& what)
	{
		UI::get().emplace<UIEnterText>("Send " + what + " to which character?", [invtype, slot, amount, mesos, what](const std::string& to)
		{
			if (to.empty())
				return;

			int32_t fee = fee_for(mesos);
			std::string question = "Send " + what + " to " + to + "? Delivery costs " + with_commas(fee) + " meso.";

			UI::get().emplace<UIYesNo>(question, [invtype, slot, amount, mesos, to](bool yes)
			{
				if (yes)
					DueySendPacket(invtype, slot, amount, mesos, to).dispatch();
			});
		}, 13);
	}

	void UIDuey::act(bool second)
	{
		int16_t sel = selection();

		if (sel < 0)
			return;

		if (!send_mode)
		{
			int32_t id = packages[sel].id;

			if (second)
			{
				UI::get().emplace<UIYesNo>("Throw this package away? It can't be brought back.", [id](bool yes)
				{
					if (yes)
						DueyRemovePacket(id).dispatch();
				});
			}
			else
			{
				DueyClaimPacket(id).dispatch();
			}

			return;
		}

		InvEntry entry = items[sel];
		int8_t invtype = static_cast<int8_t>(tab);
		std::string name = ItemData::get(entry.itemid).get_name();

		if (entry.count > 1)
		{
			UI::get().emplace<UIEnterNumber>("How many would you like to send?", [this, entry, invtype, name](int32_t qty)
			{
				int16_t amount = static_cast<int16_t>(std::clamp<int32_t>(qty, 1, entry.count));
				ask_recipient(invtype, entry.slot, amount, 0, name + " x" + std::to_string(amount));
			}, entry.count, entry.count);
		}
		else
		{
			ask_recipient(invtype, entry.slot, 1, 0, name);
		}
	}

	Cursor::State UIDuey::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		// Stay on top and keep the touches after a dialog over this closes
		UI::get().bring_to_front(TYPE, true);

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

		if (l.recv_mode.contains(cursorpos) || l.send_mode.contains(cursorpos))
		{
			bool to_send = l.send_mode.contains(cursorpos);

			if (to_send != send_mode)
			{
				send_mode = to_send;
				first_row = 0;
				selected = -1;
				subtitles.clear();

				if (send_mode)
					refresh_inventory();
			}

			return Cursor::State::CLICKING;
		}

		if (send_mode)
		{
			for (size_t i = 0; i < NUM_TABS; i++)
			{
				if (!l.tabs[i].contains(cursorpos))
					continue;

				if (TABS[i] != tab)
				{
					tab = TABS[i];
					first_row = 0;
					selected = -1;
					refresh_inventory();
				}

				return Cursor::State::CLICKING;
			}

			if (l.send_mesos.contains(cursorpos))
			{
				int64_t carry = inventory.get_meso();
				int32_t max = static_cast<int32_t>(std::max<int64_t>(0, std::min<int64_t>(carry - 5000, 2147483647)));

				if (max > 0)
				{
					UI::get().emplace<UIEnterNumber>("How many mesos would you like to send?", [this](int32_t mesos)
					{
						if (mesos > 0)
							ask_recipient(0, 0, 0, mesos, with_commas(mesos) + " meso");
					}, max, std::min<int32_t>(max, 100000));
				}

				return Cursor::State::CLICKING;
			}
		}

		if (l.list.contains(cursorpos))
		{
			int16_t last = std::min<int16_t>(count(), first_row + l.rows);

			for (int16_t i = first_row; i < last; i++)
			{
				if (!row_rect(l, i).contains(cursorpos))
					continue;

				// A second tap on the selected row claims or sends it
				if (selected == i)
					act(false);
				else
					selected = i;

				return Cursor::State::CLICKING;
			}

			return Cursor::State::IDLE;
		}

		if (selection() >= 0)
		{
			if (l.action.contains(cursorpos))
			{
				act(false);
				return Cursor::State::CLICKING;
			}

			if (!send_mode && l.action2.contains(cursorpos))
			{
				act(true);
				return Cursor::State::CLICKING;
			}
		}

		return Cursor::State::IDLE;
	}

	void UIDuey::send_scroll(double yoffset)
	{
		gesture.cancel();

		first_row += yoffset > 0 ? -1 : 1;
		clamp_scroll();
	}

	void UIDuey::send_key(int32_t, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIDuey::is_in_range(Point<int16_t>) const
	{
		return true;
	}

	void UIDuey::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIDuey::get_type() const
	{
		return TYPE;
	}

	void UIDuey::remove_package(int32_t packageid)
	{
		packages.erase(std::remove_if(packages.begin(), packages.end(),
			[packageid](const Package& p) { return p.id == packageid; }), packages.end());

		subtitles.clear();

		if (selected >= static_cast<int16_t>(packages.size()))
			selected = -1;

		clamp_scroll();
	}
}
