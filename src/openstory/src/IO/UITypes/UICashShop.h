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

#include "../UIElement.h"

#include "../Components/Charset.h"
#include "../Components/Gauge.h"
#include "../Components/Slider.h"
#include "../Components/TapGesture.h"

#include "../../Character/Look/CharLook.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/Animation.h"
#include "../../Graphics/Text.h"

#include <array>
#include <map>
#include <vector>

namespace ms
{
	class UICashShop : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CASHSHOP;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UICashShop();

			void draw(float inter) const;
		void update() override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		Button::State button_pressed(uint16_t buttonid);

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;

		// A message over the shop (purchase results, errors), since the
		// chat log isn't shown here
		static void show_status(const std::string& text, bool error);

		UIElement::Type get_type() const override;

		// Asks the server to leave; exit_cashshop() runs on its answer
		void request_exit();
		void exit_cashshop();

	private:
		void update_items();
		void scroll_rows(int16_t rows);

		// Touch-sized Exit and Buy buttons, drawn over the shop art (which
		// a newer UI.nx may not ship). Buy asks for a second tap.
		Rectangle<int16_t> exit_rect() const;
		Rectangle<int16_t> buy_rect() const;
		void draw_touch_buttons() const;
		bool buy_armed = false;
		int32_t exit_ticks = 0;
		mutable Text exit_label;
		mutable Text buy_label;
		mutable Text status_text;

		// 5 columns x 3 rows of the classic card cells (119x184) in the
		// center panel, clear of the baked right-column panels
		static constexpr uint8_t GRID_COLS = 5u;
		static constexpr uint8_t GRID_ROWS = 3u;
		static constexpr uint8_t MAX_ITEMS = GRID_COLS * GRID_ROWS;
		static constexpr int16_t GRID_X = 140;
		static constexpr int16_t GRID_Y = 50;
		static constexpr int16_t STRIDE_X = 128;
		static constexpr int16_t STRIDE_Y = 210;
		static constexpr int16_t CARD_W = 119;
		static constexpr int16_t CARD_H = 184;

		class Item
		{
		public:
			enum Label : uint8_t
			{
				ACTION,
				BOMB_SALE,
				BONUS,
				EVENT = 4,
				HOT,
				LIMITED,
				LIMITED_BRONZE,
				LIMITED_GOLD,
				LIMITED_SILVER,
				LUNA_CRYSTAL,
				MASTER = 12,
				MUST,
				NEW,
				SALE = 17,
				SPEICAL,
				SPECIAL_PRICE,
				TIME,
				TODAY,
				WEEKLY,
				WONDER_BERRY,
				WORLD_SALE,
				NONE
			};

			// price is the NX cash price from Commodity.img, not the
			// item's meso shop price
			Item(int32_t itemid, int32_t sn, Label label, int32_t price, uint16_t count) : sn(sn), label(label), price(price), discount_price(0), count(count), itemid(itemid) {}

			int32_t sn;
			Label label;
			int32_t price;
			int32_t discount_price;
			uint16_t count;

			void draw(const DrawArgument& args) const
			{
				ItemData::get(itemid).get_icon(false).draw(args);
			}

			const std::string get_name() const
			{
				return ItemData::get(itemid).get_name();
			}

			const std::string& get_desc() const
			{
				return ItemData::get(itemid).get_desc();
			}

			int32_t get_itemid() const
			{
				return itemid;
			}

			const int32_t get_price() const
			{
				return price;
			}

		private:
			// Only the id: the shop lists thousands of items, and loading
			// each one's data up front downloads its icons on a phone
			int32_t itemid;
		};

		enum Buttons : uint16_t
		{
			BtPreview1,
			BtPreview2,
			BtPreview3,
			BtExit,
			BtChargeNX,
			BtChargeRefresh,
			BtMileage,
			BtHelp,
			BtCoupon,
			BtNext,
			BtPrev,
			BtDetailPackage,
			BtNonGrade,
			BtBuyAvatar,
			BtDefaultAvatar,
			BtSaveAvatar,
			BtTakeoffAvatar,
			BtBuy
		};

		Point<int16_t> BestNew_dim;

		Sprite preview_sprites[3];
		uint8_t preview_index;

		Sprite menu_tabs[9];
		uint8_t menu_index;

		Text job_label;
		Text name_label;
		mutable Text cash_balance_text[3];

		int16_t selected_item;
		CharLook preview_look;
		// What the stage shows for an item you can't wear: a pet's
		// standing animation, or the item's own icon drawn large
		Animation preview_pet;
		bool preview_has_pet = false;
		bool m_was_down = false;
		int32_t preview_icon_id = 0;

		// Controllable stage character
		float char_x = 820.0f;
		float char_yoff = 0.0f;
		float char_vy = 0.0f;
		bool char_jumping = false;
		bool key_left = false;
		bool key_right = false;
		bool facing_right = true;
		uint8_t cur_stance = 0;

		Texture preview_scene[3];
		mutable Text preview_name;
		mutable Text preview_desc;
		mutable Text preview_price;

		std::vector<Sprite> promotion_sprites;
		Point<int16_t> promotion_pos;
		int8_t promotion_index;

		Sprite mvp_sprites[7];
		Point<int16_t> mvp_pos;
		uint8_t mvp_grade;
		Gauge mvp_gauge;
		float_t mvp_exp;

		Charset charge_charset;

		Sprite item_base;
		Sprite item_line;
		Sprite item_none;
		std::vector<Sprite> item_labels;
		std::vector<Item> items;
		Text item_name[MAX_ITEMS];
		Text item_price[MAX_ITEMS];
		Text item_discount[MAX_ITEMS];
		Text item_percent[MAX_ITEMS];

		Slider list_slider;
		int16_t list_offset;

		// Sub-panel backgrounds
		Texture wishlist_bg;
		Texture gift_bg;
		Texture coupon_bg;
		Texture search_bg;
		Texture cs_inventory_bg;
		Texture purchase_bg;

		// Active sub-panel tracking
		enum SubPanel : uint8_t
		{
			SUBPANEL_NONE,
			SUBPANEL_WISHLIST,
			SUBPANEL_GIFT,
			SUBPANEL_COUPON,
			SUBPANEL_SEARCH,
			SUBPANEL_INVENTORY,
			SUBPANEL_PURCHASE
		};
		SubPanel active_subpanel;

		// Commodity "Gender": 0 male, 1 female, 2 or missing for both
		std::vector<int8_t> item_gender;

#ifdef __ANDROID__
		// The shop as a full-screen touch layout (end of UICashShop.cpp):
		// categories down the left, big item cards, and the selected item
		// on the right with your character trying it on and a Buy button.
		// "My items" lists what was bought, with Move to inventory.
		static constexpr size_t MOBILE_CATEGORIES = 12;

		struct MobileLayout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> exit;
			Rectangle<int16_t> shop_mode;
			Rectangle<int16_t> mine_mode;
			std::array<Rectangle<int16_t>, MOBILE_CATEGORIES> categories;
			Rectangle<int16_t> grid;
			Rectangle<int16_t> detail;
			Rectangle<int16_t> stage;
			Rectangle<int16_t> action;
			int16_t card;
			int16_t cols;
			int16_t rows;
		};

		void init_mobile();
		MobileLayout mobile_layout() const;
		void mobile_filter();
		int16_t mobile_count() const;
		int32_t mobile_itemid(int16_t index) const;
		Rectangle<int16_t> mobile_card(const MobileLayout& l, int16_t index) const;
		void mobile_clamp();
		void mobile_select(int16_t index);
		void mobile_draw_icon(int32_t itemid, Rectangle<int16_t> box) const;
		Text& mobile_name(int32_t itemid, int16_t width) const;
		void draw_mobile(float inter) const;
		Cursor::State send_cursor_mobile(bool clicked, Point<int16_t> cursorpos);

		bool m_mine = false;
		size_t m_category = 0;
		std::vector<int16_t> m_view;
		int16_t m_first = 0;
		int16_t m_selected = -1;
		TapGesture m_gesture;
		mutable size_t m_storage_seen = 0;
		Text m_title;
		Text m_exit;
		mutable Text m_shop_label;
		mutable Text m_mine_label;
		mutable std::array<Text, MOBILE_CATEGORIES> m_category_labels;
		mutable Text m_nx;
		mutable Text m_detail_name;
		mutable Text m_detail_price;
		mutable Text m_detail_desc;
		mutable Text m_action;
		mutable Text m_status;
		Text m_hint;
		Text m_empty;
		mutable int32_t m_shown = 0;
		mutable std::map<std::pair<int32_t, int16_t>, Text> m_names;
		mutable std::map<int32_t, Text> m_prices;
#endif
	};
}