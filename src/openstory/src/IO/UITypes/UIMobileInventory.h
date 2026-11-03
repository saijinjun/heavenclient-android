//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"
#include "../Components/TapGesture.h"

#include "../../Character/Inventory/Inventory.h"
#include "../../Graphics/Text.h"

#include <array>
#include <map>
#include <vector>

namespace ms
{
	// The inventory as a full-screen panel for touch: big slots, big tabs,
	// and the selected item's details with Use/Equip and Drop buttons beside
	// the grid, instead of double-clicks and drag-and-drop. Swipe to scroll.
	class UIMobileInventory : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEINVENTORY;
		static constexpr bool FOCUSED = true;
		// Made fresh each time, so it always opens focused and on the tab asked for
		static constexpr bool TOGGLED = false;

		// Opens on the worn equipment when `worn`, else on the equip bag
		UIMobileInventory(const Inventory& inventory, bool worn = false);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		static constexpr size_t NUM_TABS = 6;

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			Rectangle<int16_t> classic;
			std::array<Rectangle<int16_t>, NUM_TABS> tabs;
			Rectangle<int16_t> grid;
			Rectangle<int16_t> detail;
			Rectangle<int16_t> action;
			Rectangle<int16_t> drop;
			std::array<Rectangle<int16_t>, 6> quick;
			Rectangle<int16_t> sort;
			Rectangle<int16_t> drop_meso;
			int16_t cell;
			int16_t cols;
			int16_t rows;
		};

		Layout layout() const;
		InventoryType::Id tab_type() const;
		bool worn_tab() const;
		int16_t slot_count() const;
		// The inventory's own slot number for a grid position (1-based)
		int16_t inventory_slot(int16_t slot) const;
		int32_t item_at(int16_t slot) const;
		int16_t total_rows(const Layout& l) const;
		int16_t slot_at(const Layout& l, Point<int16_t> cursorpos) const;
		Rectangle<int16_t> slot_rect(const Layout& l, int16_t slot) const;
		void clamp_scroll();

		// What the action button does for the selected item, or "" for none
		std::string action_name() const;
		void use_selected();
		void drop_selected();
		void drop_meso();
		void sort_tab();
		bool can_sort() const;
		void open_classic();
		bool can_quick_slot() const;
		// The touch quick slot that holds this item, or -1
		int16_t bound_quick_slot(int32_t itemid) const;
		void bind_quick_slot(size_t slot, int32_t itemid) const;
		// An equip's type, requirements and stats for the details
		void build_equip_lines() const;
		// Scrolls for equipment go on a worn item: the action button picks
		// the scroll, then a tap on a worn item applies it
		static bool is_equip_scroll(int32_t itemid);
		void start_scrolling();
		void apply_scroll(int16_t worn_slot);
		void stop_scrolling();

		void draw_icon(int32_t itemid, Rectangle<int16_t> box) const;
		void draw_label(const Text& text, Rectangle<int16_t> box) const;

		const Inventory& inventory;
		size_t tab;
		int16_t selected = 0;
		int16_t first_row = 0;
		TapGesture gesture;

		Text title;
		mutable std::array<Text, NUM_TABS> tab_labels;
		mutable Text name;
		mutable Text desc;
		mutable Text amount;
		mutable Text meso;
		mutable Text action_label;
		Text drop_label;
		Text sort_label;
		Text drop_meso_label;
		Text classic_label;
		Text close_label;
		Text empty_hint;
		std::vector<Text> worn_labels;
		mutable std::map<int16_t, std::pair<int16_t, Text>> counts;
		mutable int32_t shown_item = 0;
		mutable int16_t shown_slot = 0;
		mutable size_t shown_tab = 0;
		mutable std::vector<Text> equip_lines;
		Text quick_hint;
		Text scroll_hint;
		int16_t scroll_slot = 0;
		int32_t scroll_id = 0;
		std::array<Text, 6> quick_labels;
	};
}
