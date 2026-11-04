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
#include "../../Character/Inventory/InventoryType.h"
#include "../../Graphics/Text.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace ms
{
	// Duey parcels as a full-screen panel (touch first, works with a mouse
	// too). Receive: the waiting packages, each with who sent it, the item
	// or mesos and the note, with Claim and Delete. Send: pick an item from
	// the inventory, or mesos, and a character's name. SoloMapling's server
	// lets you send to your own characters, so this is how items move
	// between them.
	class UIDuey : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::DUEY;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		struct Package
		{
			int32_t id = 0;
			std::string sender;
			int32_t mesos = 0;
			std::string message;
			int32_t itemid = 0;
			int16_t count = 0;
		};

		// Normal delivery fee: 5,000 plus the trade tax on the mesos sent
		// (Cosmic's Trade.getFee).
		static int32_t fee_for(int32_t mesos);

		UIDuey(const Inventory& inventory, std::vector<Package> packages);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

		// The server took a package away (claimed or deleted)
		void remove_package(int32_t packageid);

	private:
		static constexpr size_t NUM_TABS = 5;

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			Rectangle<int16_t> recv_mode;
			Rectangle<int16_t> send_mode;
			std::array<Rectangle<int16_t>, NUM_TABS> tabs;
			Rectangle<int16_t> list;
			Rectangle<int16_t> detail;
			Rectangle<int16_t> action;
			Rectangle<int16_t> action2;
			Rectangle<int16_t> send_mesos;
			int16_t rows;
		};

		struct InvEntry
		{
			int16_t slot;
			int32_t itemid;
			int16_t count;
		};

		Layout layout() const;
		void refresh_inventory();
		int16_t count() const;
		int16_t selection() const;
		Rectangle<int16_t> row_rect(const Layout& l, int16_t index) const;
		void clamp_scroll();
		void draw_icon(int32_t itemid, Rectangle<int16_t> box) const;
		const Text& item_name(int32_t itemid) const;
		void act(bool second);
		void ask_recipient(int8_t invtype, int16_t slot, int16_t amount, int32_t mesos, const std::string& what);

		const Inventory& inventory;
		std::vector<Package> packages;
		std::vector<InvEntry> items;

		bool send_mode = false;
		InventoryType::Id tab = InventoryType::Id::USE;
		int16_t first_row = 0;
		int16_t selected = -1;
		int16_t refresh_ticks = 0;
		TapGesture gesture;

		Text title;
		Text close_label;
		mutable Text recv_label;
		mutable Text send_label;
		mutable std::array<Text, NUM_TABS> tab_labels;
		Text mesos_label;
		Text hint;
		Text empty;
		mutable Text action_label;
		mutable Text action2_label;
		mutable Text detail_name;
		mutable Text detail_line;
		mutable Text detail_note;
		mutable Text footer;
		mutable std::map<int32_t, Text> names;
		mutable std::map<int16_t, std::pair<int32_t, Text>> subtitles;
	};
}
