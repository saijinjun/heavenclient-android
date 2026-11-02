//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton						//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//																				//
//	This program is distributed in the hope that it will be useful,				//
//	but WITHOUT ANY WARRANTY; without even the implied warranty of				//
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the				//
//	GNU Affero General Public License for more details.							//
//																				//
//	You should have received a copy of the GNU Affero General Public License	//
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.		//
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIDragElement.h"

#include "../Components/Slider.h"
#include "../Components/TapGesture.h"

#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Animation.h"

#include <array>
#include <map>

namespace ms
{
	class UIShop : public UIDragElement<PosSHOP>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::SHOP;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = true;

		UIShop(const CharLook& charlook, const Inventory& inventory);

		void draw(float alpha) const override;
		void update() override;

		void remove_cursor() override;
		Cursor::State send_cursor(bool clicked, Point<int16_t> position) override;
		void send_scroll(double yoffset) override;
		void rightclick(Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

#ifdef __ANDROID__
		// On phones the shop fills the screen
		bool is_in_range(Point<int16_t> cursorpos) const override;
#endif

		void reset(int32_t npcid);
		// Build the buy-side category tabs — call after the server's
		// full item list has been added so only categories present in
		// this shop get a tab.
		void finalize_buy_tabs();
		// 0 = mesos; else the ETC item prices are charged in. Set before add_item.
		void set_currency_item(int32_t itemid);
		void add_item(int32_t id, int32_t price, int32_t pitch, int32_t time, int16_t buyable);
		void add_rechargable(int32_t id, int32_t price, int32_t pitch, int32_t time, int16_t chargeprice, int16_t buyable);

		void modify(InventoryType::Id type);

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void clear_tooltip();
		void show_item(int16_t slot, bool sale);
		void changeselltab(InventoryType::Id tab);
		int16_t slot_by_position(int16_t y);
		uint16_t tabbyinventory(InventoryType::Id type);
		uint16_t buy_tab_by_inventory(InventoryType::Id type);
		void changebuytab(InventoryType::Id type);
		void exit_shop();

		enum Buttons : int16_t
		{
			BUY_ITEM,
			SELL_ITEM,
			EXIT,
			CHECKBOX,
			OVERALL,
			EQUIP,
			USE,
			ETC,
			SETUP,
			CASH,
			BUY_TAB_EQUIP,
			BUY_TAB_USE,
			BUY_TAB_ETC,
			BUY_TAB_SETUP,
			BUY_TAB_CASH,
			BUY0,
			BUY1,
			BUY2,
			BUY3,
			BUY4,
			BUY5,
			BUY6,
			BUY7,
			BUY8,
			SELL0,
			SELL1,
			SELL2,
			SELL3,
			SELL4,
			SELL5,
			SELL6,
			SELL7,
			SELL8,
			NUM_BUTTONS
		};

		const CharLook& charlook;
		const Inventory& inventory;

		Animation npc;
		Texture buy_selection;
		Texture sell_selection;
		Texture meso;
		// Single shared meso-coin animation overlaid on every item's
		// price row, so all items blink in sync.
		Animation meso_anim;
		int32_t currency_item = 0;
		Texture currency_icon;
		std::string currency_name;
		// Local frame counter for the char portrait so it animates
		// independently of whatever the player's world char is doing.
		mutable uint8_t charframe = 0;
		mutable uint16_t charframe_delay = 0;
		Text mesolabel;

		Slider buyslider;
		Slider sellslider;

		int16_t buy_x;
		int16_t buy_width;
		int16_t sell_x;
		int16_t sell_width;

		BoolPair<Texture> checkBox;

		bool rightclicksell;

		Point<int16_t> lastcursorpos;

		class BuyItem
		{
		public:
			BuyItem(Texture cur, bool item_currency, const std::string& suffix, int32_t i, int32_t p, int32_t pt, int32_t t, int16_t cp, int16_t b);

			void draw(Point<int16_t> position) const;

			int32_t get_id() const;
			int16_t get_buyable() const;
			int32_t get_price() const;
			int32_t get_pitch() const;
			int16_t get_chargeprice() const { return chargeprice; }

		private:
			Texture icon;
			Texture currency;
			bool item_currency;
			int32_t id;
			int32_t price;
			int32_t pitch;
			int32_t time;
			int16_t chargeprice;
			int16_t buyable;
			Text namelabel;
			Text pricelabel;
		};

		class SellItem
		{
		public:
			SellItem(int32_t item_id, int16_t count, int16_t slot, bool showcount, Texture cur);

			void draw(Point<int16_t> position) const;

			int32_t get_id() const;
			int16_t get_slot() const;
			int16_t get_sellable() const;

		private:
			Texture icon;
			int32_t id;
			int16_t slot;
			int16_t sellable;
			bool showcount;
			Text namelabel;
			Text pricelabel;
		};

		struct BuyState
		{
			// Full list from the server, plus the filtered view that
			// the UI actually displays. `item_slots` maps the filtered
			// index back to the server's buy-list slot, which is what
			// NpcShopActionPacket expects.
			std::vector<BuyItem> all_items;
			std::vector<BuyItem> items;
			std::vector<int16_t> item_slots;
			// NONE = no filter (show all).
			InventoryType::Id filter = InventoryType::Id::NONE;
			int16_t offset;
			int16_t lastslot;
			int16_t selection;

			void reset();
			void draw(Point<int16_t> position, const Texture& selected) const;
			void show_item(int16_t slot);
			void add(BuyItem item);
			void set_filter(InventoryType::Id f);
			void buy() const;
			void select(int16_t selected);
		};

		BuyState buystate;

		struct SellState
		{
			std::vector<SellItem> items;
			int16_t offset;
			InventoryType::Id tab;
			int16_t lastslot;
			int16_t selection;

			void reset();
			void change_tab(const Inventory& inventory, InventoryType::Id type, Texture meso);
			void draw(Point<int16_t> position, const Texture& selected) const;
			void show_item(int16_t slot);
			void sell(bool skip_confirmation) const;
			void select(int16_t selected);
		};

		SellState sellstate;

#ifdef __ANDROID__
		// The shop as a full-screen panel for touch (UIShopMobile.cpp): Buy
		// and Sell modes, big category tabs and rows, and the selected item
		// with a big Buy or Sell button. The buy and sell states above stay
		// the model, so prices, quantities and packets work as on desktop.
		static constexpr size_t MOBILE_TABS = 6;

		struct MobileLayout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> leave;
			Rectangle<int16_t> buy_mode;
			Rectangle<int16_t> sell_mode;
			std::array<Rectangle<int16_t>, MOBILE_TABS> tabs;
			Rectangle<int16_t> list;
			Rectangle<int16_t> detail;
			Rectangle<int16_t> action;
			int16_t rows;
		};

		void init_mobile();
		MobileLayout mobile_layout() const;
		// The categories shown for the current mode, in tab order
		std::vector<InventoryType::Id> mobile_categories() const;
		int16_t mobile_count() const;
		int32_t mobile_item(int16_t index) const;
		int16_t mobile_selection() const;
		Rectangle<int16_t> mobile_row(const MobileLayout& l, int16_t index) const;
		void mobile_clamp();
		void draw_mobile(float alpha) const;
		Cursor::State send_cursor_mobile(bool clicked, Point<int16_t> cursorpos);
		const Text& mobile_name(int32_t itemid) const;
		const Text& mobile_price(int16_t index) const;
		void mobile_draw_icon(int32_t itemid, Rectangle<int16_t> box) const;

		bool m_sell = false;
		int16_t m_first = 0;
		TapGesture m_gesture;
		Text m_title;
		Text m_leave;
		mutable Text m_buy;
		mutable Text m_sell_label;
		mutable std::array<Text, MOBILE_TABS> m_tabs;
		Text m_hint;
		Text m_empty;
		mutable Text m_action;
		mutable Text m_detail_name;
		mutable Text m_detail_desc;
		mutable Text m_detail_price;
		mutable int32_t m_shown = 0;
		mutable std::map<int32_t, Text> m_names;
		mutable std::map<std::pair<int32_t, int32_t>, Text> m_prices;
		mutable std::map<int16_t, std::pair<int16_t, Text>> m_counts;
#endif
	};
}