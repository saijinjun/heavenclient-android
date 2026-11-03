//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "UIMobileInventory.h"

#include "UIItemInventory.h"
#include "UINotice.h"

#include "../UI.h"

#include "../../Constants.h"
#include "../../Character/Look/EquipSlot.h"
#include "../../Data/ItemData.h"
#include <tuple>
#include "../../Character/Inventory/Equip.h"
#include "../../Net/Packets/PlayerPackets.h"
#include "../../Data/EquipData.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Gameplay/Stage.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Util/Misc.h"

#include <algorithm>

namespace ms
{
	namespace
	{
		const InventoryType::Id TAB_TYPES[] = {
			InventoryType::Id::EQUIPPED, InventoryType::Id::EQUIP, InventoryType::Id::USE, InventoryType::Id::SETUP,
			InventoryType::Id::ETC, InventoryType::Id::CASH
		};

		const char* TAB_NAMES[] = { "Worn", "Equip", "Use", "Set-up", "Etc", "Cash" };
		const char* TAB_ICONS[] = { nullptr, "tab_equip", "tab_use", "tab_setup", "tab_etc", "tab_cash" };

		// The worn tab lists these equipment slots in order
		const std::pair<EquipSlot::Id, const char*> WORN[] = {
			{ EquipSlot::Id::HAT, "Hat" }, { EquipSlot::Id::FACE, "Face" }, { EquipSlot::Id::EYEACC, "Eye" },
			{ EquipSlot::Id::EARACC, "Earring" }, { EquipSlot::Id::TOP, "Top" }, { EquipSlot::Id::BOTTOM, "Bottom" },
			{ EquipSlot::Id::SHOES, "Shoes" }, { EquipSlot::Id::GLOVES, "Gloves" }, { EquipSlot::Id::CAPE, "Cape" },
			{ EquipSlot::Id::WEAPON, "Weapon" }, { EquipSlot::Id::SHIELD, "Shield" }, { EquipSlot::Id::PENDANT1, "Pendant" },
			{ EquipSlot::Id::BELT, "Belt" }, { EquipSlot::Id::MEDAL, "Medal" }, { EquipSlot::Id::RING1, "Ring" },
			{ EquipSlot::Id::RING2, "Ring" }, { EquipSlot::Id::RING3, "Ring" }, { EquipSlot::Id::RING4, "Ring" },
			{ EquipSlot::Id::SHOULDER, "Shoulder" }, { EquipSlot::Id::POCKET, "Pocket" }
		};

		constexpr int16_t NUM_WORN = static_cast<int16_t>(sizeof(WORN) / sizeof(WORN[0]));

		constexpr int16_t MARGIN = 12;
		constexpr int16_t TITLE_H = 26;
		constexpr int16_t TAB_H = 32;
		constexpr int16_t DETAIL_W = 230;
		constexpr int16_t SLOT_BTN_H = 30;
		constexpr size_t NUM_QUICK = 6;
		constexpr int16_t FOOT_H = 30;
		constexpr int16_t SLOT = 50;
		constexpr int16_t GAP = 6;

		Rectangle<int16_t> rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}
	}

	UIMobileInventory::UIMobileInventory(const Inventory& invent, bool worn) : UIElement(Point<int16_t>(0, 0), Point<int16_t>(0, 0)), inventory(invent), tab(worn ? 0 : 1)
	{
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Inventory");

		for (size_t i = 0; i < NUM_TABS; i++)
			tab_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, TAB_NAMES[i]);

		name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", DETAIL_W - 20);
		amount = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE);
		meso = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP);
		action_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE);
		drop_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Drop");
		classic_label = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, "Classic view");
		close_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "X");
		for (const auto& worn : WORN)
			worn_labels.emplace_back(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, worn.second);

		quick_hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE, "Put on quick slot");

		for (size_t i = 0; i < NUM_QUICK; i++)
			quick_labels[i] = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, std::to_string(i + 1));

		scroll_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::HUD_EXP, "", DETAIL_W - 20);
		empty_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap an item to see it here", DETAIL_W - 20);
	}

	UIMobileInventory::Layout UIMobileInventory::layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		Layout l;
		int16_t pw = std::min<int16_t>(vw - 24, 680);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = rect(px, py, pw, ph);
		l.close = rect(px + pw - 40, py + 4, 34, 34);
		l.classic = rect(px + MARGIN, py + 6, 96, 24);

		int16_t tab_y = py + TITLE_H + 14;
		int16_t tab_w = (pw - MARGIN * 2 - GAP * (NUM_TABS - 1)) / NUM_TABS;

		for (size_t i = 0; i < NUM_TABS; i++)
			l.tabs[i] = rect(px + MARGIN + static_cast<int16_t>(i) * (tab_w + GAP), tab_y, tab_w, TAB_H);

		int16_t body_y = tab_y + TAB_H + 10;
		int16_t body_h = py + ph - FOOT_H - 8 - body_y;
		int16_t grid_w = pw - MARGIN * 3 - DETAIL_W;

		l.cell = SLOT + GAP;
		l.cols = std::max<int16_t>(1, (grid_w + GAP) / l.cell);
		l.rows = std::max<int16_t>(1, (body_h + GAP) / l.cell);
		l.grid = rect(px + MARGIN, body_y, grid_w, body_h);
		l.detail = rect(px + pw - MARGIN - DETAIL_W, body_y, DETAIL_W, body_h);
		l.action = rect(l.detail.left() + 10, l.detail.bottom() - 86, DETAIL_W - 20, 38);
		l.drop = rect(l.detail.left() + 10, l.detail.bottom() - 42, DETAIL_W - 20, 34);

		// Quick slot 1-6 buttons, two rows of three above Use and Drop
		int16_t quick_w = (DETAIL_W - 20 - 2 * 6) / 3;

		for (size_t i = 0; i < NUM_QUICK; i++)
		{
			int16_t col = static_cast<int16_t>(i % 3);
			int16_t row = static_cast<int16_t>(i / 3);
			int16_t y = l.action.top() - 8 - (2 - row) * SLOT_BTN_H - (1 - row) * 6;
			l.quick[i] = rect(l.detail.left() + 10 + col * (quick_w + 6), y, quick_w, SLOT_BTN_H);
		}

		return l;
	}

	InventoryType::Id UIMobileInventory::tab_type() const
	{
		return TAB_TYPES[tab];
	}

	bool UIMobileInventory::worn_tab() const
	{
		return tab_type() == InventoryType::Id::EQUIPPED;
	}

	int16_t UIMobileInventory::slot_count() const
	{
		return worn_tab() ? NUM_WORN : inventory.get_slotmax(tab_type());
	}

	int16_t UIMobileInventory::inventory_slot(int16_t slot) const
	{
		return worn_tab() ? WORN[slot - 1].first : slot;
	}

	int32_t UIMobileInventory::item_at(int16_t slot) const
	{
		if (slot <= 0 || slot > slot_count())
			return 0;

		return inventory.get_item_id(tab_type(), inventory_slot(slot));
	}

	int16_t UIMobileInventory::total_rows(const Layout& l) const
	{
		return (slot_count() + l.cols - 1) / l.cols;
	}

	Rectangle<int16_t> UIMobileInventory::slot_rect(const Layout& l, int16_t slot) const
	{
		int16_t index = slot - 1 - first_row * l.cols;
		int16_t col = index % l.cols;
		int16_t row = index / l.cols;

		// Center the grid in its area
		int16_t used_w = l.cols * l.cell - GAP;
		int16_t x = l.grid.left() + (l.grid.width() - used_w) / 2 + col * l.cell;
		int16_t y = l.grid.top() + row * l.cell;

		return rect(x, y, SLOT, SLOT);
	}

	int16_t UIMobileInventory::slot_at(const Layout& l, Point<int16_t> cursorpos) const
	{
		int16_t first = first_row * l.cols + 1;
		int16_t last = std::min<int16_t>(slot_count(), first + l.rows * l.cols - 1);

		for (int16_t slot = first; slot <= last; slot++)
			if (slot_rect(l, slot).contains(cursorpos))
				return slot;

		return 0;
	}

	void UIMobileInventory::clamp_scroll()
	{
		Layout l = layout();
		int16_t max_first = std::max<int16_t>(0, total_rows(l) - l.rows);
		first_row = std::clamp<int16_t>(first_row, 0, max_first);
	}

	void UIMobileInventory::draw_icon(int32_t itemid, Rectangle<int16_t> box) const
	{
		const Texture& icon = ItemData::get(itemid).get_icon(false);

		if (!icon.is_valid())
			return;

		// Icons are 32 px; scale them to fill most of the box, centered
		Point<int16_t> dim = icon.get_dimensions();
		float scale = std::min(box.width() * 0.8f / std::max<int16_t>(dim.x(), 1), box.height() * 0.8f / std::max<int16_t>(dim.y(), 1));
		scale = std::min(scale, 2.5f);

		Point<int16_t> origin = icon.get_origin();
		int16_t x = box.left() + (box.width() - static_cast<int16_t>(dim.x() * scale)) / 2 + static_cast<int16_t>(origin.x() * scale);
		int16_t y = box.top() + (box.height() - static_cast<int16_t>(dim.y() * scale)) / 2 + static_cast<int16_t>(origin.y() * scale);

		icon.draw(DrawArgument(Point<int16_t>(x, y), scale, scale));
	}

	void UIMobileInventory::draw_label(const Text& text, Rectangle<int16_t> box) const
	{
		text.draw(Point<int16_t>(box.left() + box.width() / 2, box.top() + (box.height() - text.height()) / 2));
	}

	void UIMobileInventory::draw(float inter) const
	{
		Layout l = layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		// Dim the game behind the panel
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
			classic::tab(l.tabs[i].get_left_top(), l.tabs[i].width(), l.tabs[i].height(), lit);
			tab_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);

			// The tab's picture sits left of its name when the APK has it
			int16_t icon = TAB_H - 12;
			int16_t text_w = tab_labels[i].width();
			int16_t total = icon + 4 + text_w;
			int16_t ix = l.tabs[i].left() + (l.tabs[i].width() - total) / 2;

			if (TAB_ICONS[i] && total <= l.tabs[i].width() - 6
				&& classic::icon(TAB_ICONS[i], Point<int16_t>(ix, l.tabs[i].top() + 6), icon))
				tab_labels[i].draw(Point<int16_t>(ix + icon + 4 + text_w / 2, l.tabs[i].top() + (l.tabs[i].height() - tab_labels[i].height()) / 2));
			else
				draw_label(tab_labels[i], l.tabs[i]);
		}

		// The grid
		InventoryType::Id type = tab_type();
		int16_t first = first_row * l.cols + 1;
		int16_t last = std::min<int16_t>(slot_count(), first + l.rows * l.cols - 1);

		for (int16_t slot = first; slot <= last; slot++)
		{
			Rectangle<int16_t> r = slot_rect(l, slot);
			classic::slot(r.get_left_top(), r.width(), r.height(), slot == selected);

			int32_t itemid = item_at(slot);

			if (itemid == 0)
			{
				// Empty worn slots say what goes there
				if (worn_tab())
					draw_label(worn_labels[slot - 1], r);

				continue;
			}

			draw_icon(itemid, r);

			int16_t count = inventory.get_item_count(type, slot);

			if (type != InventoryType::Id::EQUIP && !worn_tab() && count > 1)
			{
				auto& entry = counts[slot];

				if (entry.first != count)
				{
					entry.first = count;
					entry.second = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::WHITE, std::to_string(count));
				}

				entry.second.draw(Point<int16_t>(r.right() - 4, r.bottom() - 15));
			}
		}

		// A thin bar on the right of the grid shows where the list is
		int16_t rows = total_rows(l);

		if (rows > l.rows)
		{
			int16_t track_h = l.grid.height() - GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / rows);
			int16_t thumb_y = l.grid.top() + (track_h - thumb_h) * first_row / std::max<int16_t>(1, rows - l.rows);
			int16_t bar_x = l.grid.right() + 2;

			classic::scrollbar(bar_x, l.grid.top(), 8, track_h, thumb_y, thumb_h);
		}

		// The selected item
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		int32_t itemid = item_at(selected);

		if (scroll_id && worn_tab())
		{
			scroll_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 40));
		}
		else if (itemid == 0)
		{
			empty_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.detail.top() + l.detail.height() / 2 - 20));
		}
		else
		{
			if (shown_item != itemid || shown_slot != selected || shown_tab != tab)
			{
				const ItemData& data = ItemData::get(itemid);
				shown_item = itemid;
				shown_slot = selected;
				shown_tab = tab;
				name.change_text(data.get_name());
				desc.change_text(data.get_desc());
				build_equip_lines();
			}

			int16_t count = inventory.get_item_count(type, selected);
			std::string amount_text = (type == InventoryType::Id::EQUIP || worn_tab()) ? "" : "Quantity: " + std::to_string(count);

			if (amount.get_text() != amount_text)
				amount.change_text(amount_text);

			Rectangle<int16_t> big = rect(l.detail.left() + (l.detail.width() - 72) / 2, l.detail.top() + 10, 72, 72);
			classic::slot(big.get_left_top(), big.width(), big.height(), true);
			draw_icon(itemid, big);

			int16_t y = big.bottom() + 8;
			name.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += name.height() + 4;

			if (!amount_text.empty())
			{
				amount.draw(Point<int16_t>(l.detail.left() + 10, y));
				y += amount.height() + 4;
			}

			bool quick = can_quick_slot();
			int16_t bottom = quick ? l.quick[0].top() - 22 : l.action.top() - 6;

			// An equip's requirements and stats, then the description in
			// what's left above the buttons
			for (const Text& line : equip_lines)
			{
				if (y + line.height() > bottom)
					break;

				line.draw(Point<int16_t>(l.detail.left() + 10, y));
				y += line.height() + 1;
			}

			if (!equip_lines.empty())
				y += 6;

			if (y + desc.height() < bottom)
				desc.draw(Point<int16_t>(l.detail.left() + 10, y));

			if (quick)
			{
				int16_t bound = bound_quick_slot(itemid);
				quick_hint.draw(Point<int16_t>(l.detail.left() + 10, l.quick[0].top() - 18));

				for (size_t i = 0; i < NUM_QUICK; i++)
				{
					const Rectangle<int16_t>& q = l.quick[i];
					classic::button(q.get_left_top(), q.width(), q.height(), static_cast<int16_t>(i) == bound);
					draw_label(quick_labels[i], q);
				}
			}

			std::string action = action_name();

			if (!action.empty())
			{
				if (action_label.get_text() != action)
					action_label.change_text(action);

				classic::button(l.action.get_left_top(), l.action.width(), l.action.height(), true);
				draw_label(action_label, l.action);
			}

			if (!worn_tab())
			{
				classic::button(l.drop.get_left_top(), l.drop.width(), l.drop.height());
				draw_label(drop_label, l.drop);
			}
		}

		std::string meso_text = std::to_string(inventory.get_meso());
		string_format::split_number(meso_text);
		int16_t meso_x = l.panel.left() + MARGIN + 4;
		int16_t meso_y = l.panel.bottom() - FOOT_H - 2;

		if (classic::meso_icon(Point<int16_t>(meso_x, meso_y), 18))
			meso_x += 22;
		else
			meso_text = "Meso  " + meso_text;

		if (meso.get_text() != meso_text)
			meso.change_text(meso_text);

		meso.draw(Point<int16_t>(meso_x, meso_y));

		UIElement::draw(inter);
	}

	void UIMobileInventory::update()
	{
		UIElement::update();

		// The selection goes away with the item (used up, dropped, moved)
		if (selected && item_at(selected) == 0)
			selected = 0;

		clamp_scroll();
	}

	std::string UIMobileInventory::action_name() const
	{
		switch (tab_type())
		{
		case InventoryType::Id::EQUIPPED:
			return "Take off";
		case InventoryType::Id::EQUIP:
			return "Equip";
		case InventoryType::Id::USE:
			return is_equip_scroll(item_at(selected)) ? "Use on equipment" : "Use";
		case InventoryType::Id::SETUP:
			return "Sit";
		case InventoryType::Id::CASH:
			return "Use";
		default:
			return "";
		}
	}

	void UIMobileInventory::use_selected()
	{
		if (!item_at(selected))
			return;

		if (worn_tab())
		{
			if (int16_t freeslot = inventory.find_free_slot(InventoryType::Id::EQUIP))
				UnequipItemPacket(inventory_slot(selected), freeslot).dispatch();
			else
				UI::get().emplace<UIOk>("Your Equip inventory is full.", [](bool) {});

			return;
		}

		if (tab_type() == InventoryType::Id::USE && is_equip_scroll(item_at(selected)))
		{
			start_scrolling();
			return;
		}

		UIItemInventory::use_item(inventory, tab_type(), selected);
	}

	bool UIMobileInventory::is_equip_scroll(int32_t itemid)
	{
		// 204xxxx: scrolls for weapons and armor, plus chaos scrolls and
		// clean slates, which also go on an equip
		return itemid / 10000 == 204;
	}

	void UIMobileInventory::start_scrolling()
	{
		scroll_slot = selected;
		scroll_id = item_at(selected);

		scroll_hint.change_text("Tap the worn item to use\\n" + ItemData::get(scroll_id).get_name()
			+ " on.\\n\\nTap another tab to cancel.");

		// Scrolls only work on what the character is wearing
		tab = 0;
		selected = 0;
		first_row = 0;
		counts.clear();
	}

	void UIMobileInventory::stop_scrolling()
	{
		scroll_slot = 0;
		scroll_id = 0;
	}

	void UIMobileInventory::apply_scroll(int16_t worn_slot)
	{
		int16_t source = scroll_slot;
		int32_t scroll = scroll_id;
		int32_t target_id = item_at(worn_slot);
		auto target = static_cast<EquipSlot::Id>(inventory_slot(worn_slot));

		stop_scrolling();

		// The scroll may have been used up while the player looked
		if (inventory.get_item_id(InventoryType::Id::USE, source) != scroll || target_id == 0)
			return;

		if (auto equip = inventory.get_equip(InventoryType::Id::EQUIPPED, inventory_slot(worn_slot)))
		{
			if (equip->get_slots() <= 0 && ItemData::get(scroll).get_name().find("Clean Slate") == std::string::npos)
			{
				UI::get().emplace<UIOk>(ItemData::get(target_id).get_name() + " has no upgrade slots left.", [](bool) {});
				return;
			}
		}

		std::string question = "Use " + ItemData::get(scroll).get_name() + "\\non " + ItemData::get(target_id).get_name() + "?";

		UI::get().emplace<UIYesNo>(question, [source, target](bool yes)
		{
			if (yes)
				ScrollEquipPacket(source, target).dispatch();
		});
	}

	void UIMobileInventory::drop_selected()
	{
		InventoryType::Id type = tab_type();
		int16_t slot = selected;
		int32_t itemid = item_at(slot);

		if (itemid == 0 || worn_tab())
			return;

		const ItemData& data = ItemData::get(itemid);
		int16_t count = inventory.get_item_count(type, slot);

		if (data.is_cashitem())
		{
			UI::get().emplace<UIOk>("You can't drop this item.", [](bool) {});
			return;
		}

		// Captures by value: the dialogs outlive this call
		auto drop = [type, slot, count]()
		{
			if (count <= 1)
			{
				MoveItemPacket(type, slot, 0, 1).dispatch();
			}
			else
			{
				UI::get().emplace<UIEnterNumber>("How many will you drop?",
					[type, slot](int32_t qty) { MoveItemPacket(type, slot, 0, static_cast<int16_t>(qty)).dispatch(); },
					count, count);
			}
		};

		if (data.is_untradable())
		{
			UI::get().emplace<UIYesNo>("This item can't be taken back once thrown away.\\nWill you still drop it?",
				[drop](bool yes) { if (yes) drop(); });
		}
		else
		{
			UI::get().emplace<UIYesNo>("Drop " + data.get_name() + "?", [drop](bool yes) { if (yes) drop(); });
		}
	}

	bool UIMobileInventory::can_quick_slot() const
	{
		// Potions, scrolls, chairs and cash items go on quick slots; equips
		// and etc items can't be used from one
		InventoryType::Id type = tab_type();
		return type == InventoryType::Id::USE || type == InventoryType::Id::SETUP || type == InventoryType::Id::CASH;
	}

	int16_t UIMobileInventory::bound_quick_slot(int32_t itemid) const
	{
		const Keyboard& keyboard = UI::get().get_keyboard();
		const auto& keys = keyboard.get_quickslot_keys();

		for (size_t i = 0; i < NUM_QUICK && i < keys.size(); i++)
		{
			Keyboard::Mapping mapping = keyboard.get_maple_mapping(keys[i]);

			if (mapping.type == KeyType::Id::ITEM && mapping.action == itemid)
				return static_cast<int16_t>(i);
		}

		return -1;
	}

	void UIMobileInventory::bind_quick_slot(size_t slot, int32_t itemid) const
	{
		// The touch quick slots show the quickslot bar's keys, so this is the
		// same binding a drag onto a slot makes
		Keyboard& keyboard = UI::get().get_keyboard();
		const auto& keys = keyboard.get_quickslot_keys();

		if (slot >= keys.size())
			return;

		uint8_t key = keys[slot];
		keyboard.assign(key, static_cast<uint8_t>(KeyType::Id::ITEM), itemid);

		std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updates;
		updates.emplace_back(static_cast<KeyConfig::Key>(key), KeyType::Id::ITEM, itemid);
		ChangeKeyMapPacket(updates).dispatch();
	}

	void UIMobileInventory::build_equip_lines() const
	{
		equip_lines.clear();

		InventoryType::Id type = tab_type();

		if (type != InventoryType::Id::EQUIP && type != InventoryType::Id::EQUIPPED)
			return;

		int32_t itemid = item_at(selected);
		const EquipData& data = EquipData::get(itemid);

		if (!data.is_valid())
			return;

		const CharStats& stats = Stage::get().get_player().get_stats();
		uint16_t width = DETAIL_W - 20;

		auto add = [&](const std::string& text, Color::Name color)
		{
			equip_lines.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, color, text, width, false);
		};

		if (!data.get_type().empty())
			add(data.get_type(), Color::Name::SILVERCHALICE);

		// Requirements, red when the character doesn't meet them
		const std::pair<MapleStat::Id, const char*> reqs[] = {
			{ MapleStat::Id::LEVEL, "Req Lv." }, { MapleStat::Id::STR, "Req STR" }, { MapleStat::Id::DEX, "Req DEX" },
			{ MapleStat::Id::INT, "Req INT" }, { MapleStat::Id::LUK, "Req LUK" }
		};

		for (const auto& req : reqs)
		{
			int16_t need = data.get_reqstat(req.first);

			if (need <= 0)
				continue;

			bool met = stats.get_stat(req.first) >= need;
			add(std::string(req.second) + " " + std::to_string(need), met ? Color::Name::WHITE : Color::Name::TORCHRED);
		}

		// This copy's own stats (scrolls included), or the item's defaults
		Optional<const Equip> equip = inventory.get_equip(type, inventory_slot(selected));

		for (auto id : EquipStat::values)
		{
			int32_t value = equip ? equip->get_stat(id) : data.get_defstat(id);

			if (value > 0)
				add(std::string(EquipStat::names[id]) + " +" + std::to_string(value), Color::Name::HUD_EXP);
		}

		if (equip && equip->get_slots() > 0)
			add("Upgrades available: " + std::to_string(equip->get_slots()), Color::Name::SILVERCHALICE);
	}

	void UIMobileInventory::open_classic()
	{
		deactivate();
		UI::get().emplace<UIItemInventory>(inventory);
	}

	Cursor::State UIMobileInventory::send_cursor(bool clicked, Point<int16_t> cursorpos)
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
					stop_scrolling();
					tab = i;
					selected = 0;
					first_row = 0;
					counts.clear();
				}

				return Cursor::State::CLICKING;
			}
		}

		if (item_at(selected) && can_quick_slot())
		{
			for (size_t i = 0; i < NUM_QUICK; i++)
			{
				if (l.quick[i].contains(cursorpos))
				{
					bind_quick_slot(i, item_at(selected));
					return Cursor::State::CLICKING;
				}
			}
		}

		if (item_at(selected))
		{
			if (!action_name().empty() && l.action.contains(cursorpos))
			{
				use_selected();
				return Cursor::State::CLICKING;
			}

			if (!worn_tab() && l.drop.contains(cursorpos))
			{
				drop_selected();
				return Cursor::State::CLICKING;
			}
		}

		if (int16_t slot = slot_at(l, cursorpos))
		{
			if (scroll_id && worn_tab())
			{
				if (item_at(slot))
					apply_scroll(slot);

				return Cursor::State::CLICKING;
			}

			// A second tap on the selected item uses it, like a double-click
			if (slot == selected && item_at(slot) && !action_name().empty())
				use_selected();
			else
				selected = item_at(slot) ? slot : 0;

			return Cursor::State::CLICKING;
		}

		return Cursor::State::IDLE;
	}

	void UIMobileInventory::send_scroll(double yoffset)
	{
		gesture.cancel();

		// Wheel up or a finger moving down shows earlier rows
		first_row += yoffset > 0 ? -1 : 1;
		clamp_scroll();
	}

	void UIMobileInventory::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	bool UIMobileInventory::is_in_range(Point<int16_t>) const
	{
		// A full-screen modal panel: every touch is for it
		return true;
	}

	void UIMobileInventory::remove_cursor()
	{
		gesture.reset();
	}

	UIElement::Type UIMobileInventory::get_type() const
	{
		return TYPE;
	}
}
