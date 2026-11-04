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
#include "UICashShop.h"

#include "../KeyAction.h"
#include "../UI.h"
#include "../Window.h"

#include "../Components/MapleButton.h"

#include "../../Constants.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Graphics/Geometry.h"
#include "../../Gameplay/Stage.h"

#include "../../Net/Handlers/CashShopHandlers.h"
#include "../../Net/Packets/CashShopPackets.h"
#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/LoginPackets.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UICashShop::UICashShop() : preview_index(0), menu_index(1), promotion_index(0), mvp_grade(1), mvp_exp(0.07f), list_offset(0)
	{
		// Primary English cash-shop sprites live in CashShopGL.img. A few
		// elements (item search/effect/char/list) don't have CashShopGL
		// equivalents, so they fall back to CashShop.img's Korean nodes.
		nl::node CashShop = nl::nx::ui["CashShop.img"];
		nl::node CashShopGL = nl::nx::ui["CashShopGL.img"];

		nl::node Base = CashShop["Base"];
		nl::node BaseFrame = CashShopGL["BaseFrame"];
		nl::node backgrnd = BaseFrame["background"];
		nl::node Preview = Base["Preview"];
		nl::node CSItemSearch = BaseFrame["CSItemSearch"];
		nl::node CSList = CashShop["CSList"];
		nl::node MainItem = CashShopGL["MainMenu"]["MainItem"];
		nl::node Popup = CashShopGL["Popup"];

		sprites.emplace_back(backgrnd);

		// BestNew banner has Korean text baked in — omit. Leave dimensions
		// zeroed out (item_none fallback still works).
		BestNew_dim = Point<int16_t>();

		for (size_t i = 0; i < 3; i++)
		{
			preview_sprites[i] = Preview[i];
			preview_scene[i] = Texture(Preview[i]);
		}

		for (size_t i = 0; i < 3; i++)
			buttons[Buttons::BtPreview1 + i] = std::make_unique<TwoSpriteButton>(Base["Tab"]["Disable"][i], Base["Tab"]["Enable"][i], Point<int16_t>(957 + (i * 17), 46));

		buttons[Buttons::BtPreview1]->set_state(Button::State::PRESSED);

		// English header/exit buttons from CashShopGL/BaseFrame.
		// The bitmaps carry absolute 1024x768 positions via negative origin
		// vectors (e.g. BtExit origin = (-962,-10) → draws at (962,10)), so
		// pass Point(0,0) and let the origin place them on the canvas.
		buttons[Buttons::BtExit] = std::make_unique<MapleButton>(BaseFrame["BtExit"]);
		buttons[Buttons::BtHelp] = std::make_unique<MapleButton>(BaseFrame["BtHelp"]);
		buttons[Buttons::BtCoupon] = std::make_unique<MapleButton>(BaseFrame["BtCoupon"]);

		// Korean-only elements (CSTab/Tab menu banners, CSStatus/BtMileage,
		// CSPromotionBanner, CSChar avatar buttons, CSMVPBanner) are
		// deliberately not instantiated — there are no equivalents in
		// CashShopGL.img that match this layout 1:1. (CSStatus/BtWish and
		// CSChar/BtInventory are the exception: they toggle the wishlist /
		// cash inventory sub-panels and have no English counterpart, so the
		// Korean buttons are instantiated further below.)

		Player& player = Stage::get().get_player();
		std::string pname = player.get_stats().get_name();
		std::string pjob = player.get_stats().get_jobname();

		job_label = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::SUPERNOVA, pjob);
		name_label = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::WHITE, pname);

		promotion_pos = Point<int16_t>();
		mvp_pos = Point<int16_t>();

		// English search bar under BaseFrame/CSItemSearch.
		sprites.emplace_back(CSItemSearch["background"]);

		// Charge NX numeric charset — fall back to CashShop/Base/Number.
		charge_charset = Charset(Base["Number"], Charset::Alignment::RIGHT);

		item_base = CSList["Base"];
		item_line = Base["line"];
		item_none = Base["noItem"];
		// Korean CSEffect labels omitted; item_labels stays empty so no
		// HOT/NEW/SALE sticker draws with Korean text.

		// The real catalog from Etc/Commodity.img — the same data Cosmic's
		// CashItemFactory sells from, so the SNs and NX prices match the
		// server exactly. Only OnSale entries are listed.
		for (nl::node entry : nl::nx::etc["Commodity.img"])
		{
			int32_t onsale = entry["OnSale"];

			if (onsale != 1)
				continue;

			int32_t itemid = entry["ItemId"];

			// Only check that the item exists: loading every listed item's
			// icons here pulled most of Character.nx and Item.nx on a
			// phone's first visit, freezing the game for minutes until the
			// server dropped the connection. Icons load as cards are shown.
			if (!ItemData::exists(itemid))
				continue;

			int32_t sn = entry["SN"];
			int32_t price = entry["Price"];
			int32_t count = entry["Count"];

			items.push_back(Item(itemid, sn, Item::Label::NONE, price, static_cast<uint16_t>(count)));
			item_gender.push_back(entry["Gender"] ? static_cast<int8_t>(entry["Gender"].get_integer()) : static_cast<int8_t>(2));
		}

		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			div_t div = std::div(i, GRID_COLS);

			// The GL BUY bitmap carries origin (-81, -54); this lands it
			// at the classic card's bottom strip (in-card 9,151)
			buttons[Buttons::BtBuy + i] = std::make_unique<MapleButton>(MainItem["BtBuy"], Point<int16_t>(GRID_X + STRIDE_X * div.rem - 72, GRID_Y + STRIDE_Y * div.quot + 97));

			item_name[i] = Text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::MINESHAFT);
			item_price[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::GRAY);
			item_discount[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE);
			item_percent[i] = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::TORCHRED);
		}

		Point<int16_t> slider_pos = Point<int16_t>(770, GRID_Y);

		list_slider = Slider(
			Slider::Type::THIN_MINESHAFT,
			Range<int16_t>(slider_pos.y(), slider_pos.y() + GRID_ROWS * STRIDE_Y - 30),
			slider_pos.x(),
			GRID_ROWS,
			static_cast<int16_t>(items.size() / GRID_COLS + 1),
			[&](bool upwards)
			{
				int16_t shift = upwards ? -GRID_COLS : GRID_COLS;
				bool above = list_offset >= 0;
				bool below = list_offset + shift < items.size();

				if (above && below)
				{
					list_offset += shift;

					update_items();
				}
			}
		);

		update_items();

		// === Sub-panel backgrounds (English — CashShopGL/Popup) ===
		// Popup/Gift/background and Popup/Cupon/background are the English
		// popup chrome (the nodes are named "background", not "backgrnd").
		// Popup/Buy has no single background bitmap (it is composed of
		// Top/Middle/Bottom strips), so purchase_bg stays empty and its
		// draw call remains guarded by is_valid().
		gift_bg = Texture(Popup["Gift"]["background"]);
		coupon_bg = Texture(Popup["Cupon"]["background"]);

		// Wishlist ("CART INVENTORY") and cash inventory ("CASH INVENTORY")
		// use the English right-side menu panels — 242px wide with a 6x2
		// grid of 32x32 item slots at (8 + 36 * col, 28 + 35 * row).
		nl::node RightMenu = CashShopGL["RightMenu"];
		wishlist_bg = Texture(RightMenu["CartInven"]["background"]);
		cs_inventory_bg = Texture(RightMenu["CashInven"]["background"]);

		// No wishlist/inventory toggle buttons: the cash inventory renders
		// permanently into its baked right-column panel, and the wishlist
		// has no English art (its data is never parsed client-side).

		// Charge NX under the baked NX-balance panel at (9,646); the
		// button bitmap's origin drops it just below the panel
		buttons[Buttons::BtChargeNX] = std::make_unique<MapleButton>(CashShopGL["LeftMenu"]["MyMenu"]["BtChageNx"], Point<int16_t>(10, 646));

		for (int i = 0; i < 3; i++)
			cash_balance_text[i] = Text(Text::Font::A11B, Text::Alignment::RIGHT, Color::Name::EMPEROR);

		selected_item = -1;
		preview_name = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", 220);
		preview_desc = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE, "", 220);
		preview_price = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::SUPERNOVA);

		// Your character lives on the preview stage; selecting an equip
		// dresses it, arrow keys walk it, jump key jumps
		preview_look = player.get_look();
		preview_nametag = std::make_unique<NameTagStyle>(player.get_stats().get_name());
		preview_look.set_stance(Stance::Id::STAND1);
		cur_stance = Stance::Id::STAND1;

		active_subpanel = SUBPANEL_NONE;

		dimension = Texture(backgrnd).get_dimensions();

		exit_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Exit");
		buy_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
		status_text = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);

		// Ask the server for the NX balances shown on the left
		OutPacket(0xE4).dispatch();

#ifdef __ANDROID__
		init_mobile();
#endif
	}

	namespace
	{
		std::string status_message;
		bool status_error = false;
		int32_t status_ticks = 0;
	}

	void UICashShop::show_status(const std::string& text, bool error)
	{
		status_message = text;
		status_error = error;
		status_ticks = 4000 / Constants::TIMESTEP;
	}

	Rectangle<int16_t> UICashShop::exit_rect() const
	{
		Point<int16_t> p = get_draw_position() + Point<int16_t>(906, 6);

		return Rectangle<int16_t>(p, p + Point<int16_t>(110, 36));
	}

	Rectangle<int16_t> UICashShop::buy_rect() const
	{
		Point<int16_t> p = get_draw_position() + Point<int16_t>(682, 304);

		return Rectangle<int16_t>(p, p + Point<int16_t>(300, 38));
	}

	void UICashShop::draw_touch_buttons() const
	{
		Rectangle<int16_t> e = exit_rect();
		int16_t eh = e.height();

		// A navy plate with the label and the round close button on its right
		classic::panel(e.get_left_top(), e.width(), eh);

		if (classic::close_button(Point<int16_t>(e.right() - eh, e.top()), eh))
			exit_label.draw(Point<int16_t>(e.left() + (e.width() - eh) / 2, classic::text_y(exit_label, e.top() + eh / 2)));
		else
			exit_label.draw(Point<int16_t>(e.left() + e.width() / 2, classic::text_y(exit_label, e.top() + eh / 2)));

		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()))
		{
			const Item& sel = items[selected_item];
			std::string label = buy_armed
				? "Tap again to buy for " + std::to_string(sel.get_price()) + " NX"
				: "Buy - " + std::to_string(sel.get_price()) + " NX";

			if (buy_label.get_text() != label)
				buy_label.change_text(label);

			Rectangle<int16_t> b = buy_rect();
			classic::button(b.get_left_top(), b.width(), b.height(), buy_armed);
			buy_label.draw(Point<int16_t>(b.left() + b.width() / 2, classic::text_y(buy_label, b.top() + b.height() / 2)));
		}

		if (status_ticks > 0 && !status_message.empty())
		{
			if (status_text.get_text() != status_message)
				status_text.change_text(status_message);

			int16_t w = std::max<int16_t>(status_text.width() + 40, 300);
			Point<int16_t> p = get_draw_position() + Point<int16_t>((1024 - w) / 2, 330);
			Rectangle<int16_t> r(p, p + Point<int16_t>(w, 40));

			status_text.change_color(status_error ? Color::Name::TORCHRED : Color::Name::WHITE);
			classic::panel(p, w, 40);
			status_text.draw(Point<int16_t>(r.left() + w / 2, classic::text_y(status_text, r.top() + 40 / 2)));
		}
	}

	void UICashShop::scroll_rows(int16_t rows)
	{
		int16_t shift = rows * GRID_COLS;
		int16_t next = list_offset + shift;

		if (next < 0 || next >= static_cast<int16_t>(items.size()))
			return;

		list_offset = next;
		update_items();
	}

	void UICashShop::send_scroll(double yoffset)
	{
#ifdef __ANDROID__
		// A swipe moves the cards one row at a time
		m_gesture.cancel();
		m_first += yoffset > 0 ? -1 : 1;
		mobile_clamp();
		return;
#endif

		if (yoffset > 0)
			scroll_rows(-1);
		else if (yoffset < 0)
			scroll_rows(1);
	}

	void UICashShop::draw(float inter) const
	{
#ifdef __ANDROID__
		draw_mobile(inter);
		return;
#endif

		// The preview stage stretches to the window top
		preview_scene[preview_index].draw(DrawArgument(position + Point<int16_t>(644, 40), Point<int16_t>(366, 310)));

		UIElement::draw_sprites(inter);

		// Korean menu_tabs / promotion_sprites / mvp_sprites / charge_charset
		// draws removed — those elements have no English equivalent.

		// NX balances beside the baked labels: NX Prepaid / NX Credit /
		// Maple Pts (query result order: credit, points, prepaid)
		cash_balance_text[0].change_text(std::to_string(get_cash_balance(2)));
		cash_balance_text[1].change_text(std::to_string(get_cash_balance(0)));
		cash_balance_text[2].change_text(std::to_string(get_cash_balance(1)));

		for (int b = 0; b < 3; b++)
			cash_balance_text[b].draw(position + Point<int16_t>(152, 653 + 26 * b));

		Point<int16_t> label_pos = position + Point<int16_t>(4, 3);
		job_label.draw(label_pos);

		size_t length = job_label.width();
		name_label.draw(label_pos + Point<int16_t>(length + 10, 0));

		if (items.empty())
			item_none.draw(position + Point<int16_t>(430, 360) - item_none.get_dimensions() / 2, inter);

		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			int16_t index = i + list_offset;

			if (index < items.size())
			{
				div_t div = std::div(i, GRID_COLS);
				Point<int16_t> cell = position + Point<int16_t>(GRID_X + STRIDE_X * div.rem, GRID_Y + STRIDE_Y * div.quot);
				Item item = items[index];

				item_base.draw(cell, inter);
				// Icon centered in the card's upper frame; v83 icons carry
				// a (0,32) origin which doubles at 2x scale
				item.draw(DrawArgument(cell + Point<int16_t>(27, 101), 2.0f, 2.0f));

				item_name[i].draw(cell + Point<int16_t>(55, 108));
				item_price[i].draw(cell + Point<int16_t>(58, 127));
			}
		}

		// Your character on the stage, dressed with whatever is selected
		preview_look.draw(
			DrawArgument(position + Point<int16_t>(static_cast<int16_t>(char_x), static_cast<int16_t>(275.0f + char_yoff)), !facing_right),
			inter);

		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()))
		{
			const Item& sel = items[selected_item];
			Point<int16_t> pv = position + Point<int16_t>(652, 46);

			preview_name.change_text(sel.get_name());
			preview_name.draw(pv);

			std::string pricestr = std::to_string(sel.get_price()) + " NX";

			if (sel.count > 1)
				pricestr += " (" + std::to_string(sel.count) + ")";

			preview_price.change_text(pricestr);
			preview_price.draw(pv + Point<int16_t>(0, 16));
		}

		list_slider.draw(position);

		UIElement::draw_buttons(inter);
		draw_touch_buttons();

		// Draw active sub-panel (English popups only).
		switch (active_subpanel)
		{
		case SUBPANEL_GIFT:
			if (gift_bg.is_valid())
				gift_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		case SUBPANEL_COUPON:
			if (coupon_bg.is_valid())
				coupon_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		case SUBPANEL_PURCHASE:
			if (purchase_bg.is_valid())
				purchase_bg.draw(DrawArgument(position + Point<int16_t>(200, 50)));
			break;
		default:
			break;
		}

		// Player's cash items rendered into the baked CASH INVENTORY
		// panel in the right column (7x2 slot grid)
		const Inventory& inventory = Stage::get().get_player().get_inventory();
		uint8_t slotmax = inventory.get_slotmax(InventoryType::Id::CASH);
		int16_t shown = 0;

		for (int16_t slot = 1; slot <= slotmax && shown < 14; slot++)
		{
			int32_t item_id = inventory.get_item_id(InventoryType::Id::CASH, slot);

			if (item_id == 0)
				continue;

			div_t sdiv = std::div(shown, 7);
			Point<int16_t> slot_pos = position + Point<int16_t>(771 + 34 * sdiv.rem, 513 + 34 * sdiv.quot);

			// v83 item icons carry a (0, 32) origin, so draw at the
			// slot's bottom edge to land the 32x32 icon inside it.
			ItemData::get(item_id).get_icon(false).draw(DrawArgument(slot_pos + Point<int16_t>(0, 32)));

			shown++;
		}
	}

	void UICashShop::send_key(int32_t keycode, bool pressed, bool escape)
	{
		switch (keycode)
		{
		case KeyAction::Id::LEFT:
			key_left = pressed;
			break;
		case KeyAction::Id::RIGHT:
			key_right = pressed;
			break;
		case KeyAction::Id::JUMP:
			if (pressed && !char_jumping)
			{
				char_jumping = true;
				char_vy = -6.2f;
			}
			break;
		default:
			break;
		}
	}

	void UICashShop::update()
	{
		UIElement::update();

		if (status_ticks > 0)
			status_ticks--;

		if (exit_ticks > 0 && --exit_ticks == 0)
			show_status("The server didn't answer. Tap Exit to try again.", true);

		preview_look.update(Constants::TIMESTEP);

		if (preview_has_pet)
			preview_pet.update();

		// Walk
		float dx = 0.0f;

		if (key_left)
			dx -= 1.3f;

		if (key_right)
			dx += 1.3f;

		if (dx != 0.0f)
		{
			char_x += dx;
			facing_right = dx > 0.0f;

			if (char_x < 662.0f)
				char_x = 662.0f;

			if (char_x > 992.0f)
				char_x = 992.0f;
		}

		// Jump arc
		if (char_jumping)
		{
			char_yoff += char_vy;
			char_vy += 0.28f;

			if (char_yoff >= 0.0f)
			{
				char_yoff = 0.0f;
				char_vy = 0.0f;
				char_jumping = false;
			}
		}

		// Stance follows the movement state
		uint8_t want = char_jumping ? Stance::Id::JUMP
			: (dx != 0.0f) ? Stance::Id::WALK1
			: Stance::Id::STAND1;

		if (want != cur_stance)
		{
			cur_stance = want;
			preview_look.set_stance(static_cast<Stance::Id>(cur_stance));
		}
	}

	Button::State UICashShop::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::BtPreview1:
		case Buttons::BtPreview2:
		case Buttons::BtPreview3:
			buttons[preview_index]->set_state(Button::State::NORMAL);

			preview_index = buttonid;
			return Button::State::PRESSED;
		case Buttons::BtExit:
			request_exit();

			return Button::State::NORMAL;
		case Buttons::BtNext:
		{
			size_t size = promotion_sprites.size() - 1;

			promotion_index++;

			if (promotion_index > size)
				promotion_index = 0;

			return Button::State::NORMAL;
		}
		case Buttons::BtPrev:
		{
			size_t size = promotion_sprites.size() - 1;

			promotion_index--;

			if (promotion_index < 0)
				promotion_index = size;

			return Button::State::NORMAL;
		}
		case Buttons::BtChargeNX:
		{
			std::string url = Configuration::get().get_chargenx();

#ifdef _WIN32
			ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

			return Button::State::NORMAL;
		}
		case Buttons::BtCoupon:
			active_subpanel = (active_subpanel == SUBPANEL_COUPON) ? SUBPANEL_NONE : SUBPANEL_COUPON;
			return Button::State::NORMAL;
		default:
			break;
		}

		if (buttonid >= Buttons::BtBuy)
		{
			int16_t index = buttonid - Buttons::BtBuy + list_offset;

			if (index >= 0 && index < items.size())
			{
				Item item = items[index];

				// Currency type 1 = NX Credit (standard purchase)
				int8_t currency = 1;
				BuyCashItemPacket(currency, item.sn).dispatch();
			}

			return Button::State::NORMAL;
		}

		return Button::State::DISABLED;
	}

	Cursor::State UICashShop::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
#ifdef __ANDROID__
		return send_cursor_mobile(clicked, cursorpos);
#endif

		Point<int16_t> cursor_relative = cursorpos - position;

		if (exit_rect().contains(cursorpos))
		{
			if (clicked)
				button_pressed(Buttons::BtExit);

			return Cursor::State::CANCLICK;
		}

		if (selected_item >= 0 && selected_item < static_cast<int16_t>(items.size()) && buy_rect().contains(cursorpos))
		{
			if (clicked)
			{
				if (buy_armed)
				{
					BuyCashItemPacket(1, items[selected_item].sn).dispatch();
					show_status("Buying " + items[selected_item].get_name() + "...", false);
					buy_armed = false;
				}
				else
				{
					buy_armed = true;
				}
			}

			return Cursor::State::CANCLICK;
		}

		if (clicked)
			buy_armed = false;

		if (list_slider.isenabled())
		{
			Cursor::State state = list_slider.send_cursor(cursor_relative, clicked);

			if (state != Cursor::State::IDLE)
				return state;
		}

		if (clicked)
		{
			Point<int16_t> off = cursorpos - position;

			for (int16_t i = 0; i < MAX_ITEMS; i++)
			{
				int16_t index = i + list_offset;

				if (index >= static_cast<int16_t>(items.size()))
					break;

				div_t cdiv = std::div(i, GRID_COLS);
				Rectangle<int16_t> card(
					Point<int16_t>(GRID_X + STRIDE_X * cdiv.rem, GRID_Y + STRIDE_Y * cdiv.quot),
					Point<int16_t>(GRID_X + STRIDE_X * cdiv.rem + CARD_W, GRID_Y + STRIDE_Y * cdiv.quot + CARD_H)
				);

				if (card.contains(off))
				{
					selected_item = index;

					int32_t itemid = items[index].get_itemid();

					if (itemid >= 1000000 && itemid < 2000000)
					{
						// Try-on: current look plus the selected equip
						preview_look = Stage::get().get_player().get_look();
						preview_look.add_equip(itemid);
						preview_look.set_stance(static_cast<Stance::Id>(cur_stance));
					}

					break;
				}
			}
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UICashShop::get_type() const
	{
		return TYPE;
	}

	void UICashShop::request_exit()
	{
		// The shop stays up until the server sends the channel back, so the
		// tap shows at once and a second tap doesn't send a second request.
		if (exit_ticks > 0)
			return;

		exit_ticks = 8000 / Constants::TIMESTEP;
		buy_armed = false;
		show_status("Leaving the Cash Shop...", false);
		ChangeMapPacket().dispatch();
	}

	void UICashShop::exit_cashshop()
	{
		exit_ticks = 0;

		UI& ui = UI::get();

		// Restore UI scale + physical window size that were overridden in
		// SetCashShopHandler::transition(). Without this the game world would
		// remain confined to the 1024x768 cash-shop window at scale 1.0.
		//
		// This has to happen *before* change_state: UIStateGame and every
		// window it builds lay themselves out against the logical viewport
		// that is live at construction time (UIChatBar's baseline, UIStatusBar's
		// right-edge buttons, and the reference viewport UIDragElement records
		// for the positions it loads from the config). Constructing them while
		// the cash shop's 1024x768 was still in force left the whole HUD
		// anchored to a viewport that vanished a frame later.
		uint16_t width = Setting<Width>::get().load();
		uint16_t height = Setting<Height>::get().load();
		Constants::Constants::get().set_ui_scale(get_pre_cashshop_ui_scale());
		Constants::Constants::get().set_viewwidth(width);
		Constants::Constants::get().set_viewheight(height);

		ui.change_state(UI::State::GAME);

		// The shop's preview stage is still loaded; Stage::load only
		// loads into a cleared stage.
		GraphicsGL::get().clear();
		Stage& stage = Stage::get();
		stage.clear();
		Player& player = stage.get_player();

		PlayerLoginPacket(player.get_oid()).dispatch();

		int32_t mapid = player.get_stats().get_mapid();
		uint8_t portalid = player.get_stats().get_portal();

		stage.load(mapid, portalid);
		stage.transfer_player();

		ui.enable();
		Timer::get().start();
		GraphicsGL::get().unlock();
	}

	void UICashShop::update_items()
	{
		for (size_t i = 0; i < MAX_ITEMS; i++)
		{
			int16_t index = i + list_offset;
			bool found_item = index < items.size();

			buttons[Buttons::BtBuy + i]->set_active(found_item);

			std::string name = "";
			std::string price_text = "";
			std::string discount_text = "";
			std::string percent_text = "";

			if (found_item)
			{
				Item item = items[index];

				name = item.get_name();

				int32_t price = item.get_price();
				price_text = std::to_string(price);

				if (item.discount_price > 0 && price > 0)
				{
					discount_text = price_text;

					uint32_t discount = item.discount_price;
					price_text = std::to_string(discount);

					float_t percent = (float)discount / price;
					std::string percent_str = std::to_string(percent);
					percent_text = "(" + percent_str.substr(2, 1) + "%)";
				}

				string_format::split_number(price_text);
				string_format::split_number(discount_text);

				price_text += " NX";

				if (discount_text != "")
					discount_text += " NX";

				if (item.count > 0)
					price_text += "(" + std::to_string(item.count) + ")";
			}

			item_name[i].change_text(name);
			item_price[i].change_text(price_text);
			item_discount[i].change_text(discount_text);
			item_percent[i].change_text(percent_text);

			string_format::format_with_ellipsis(item_name[i], 92);
		}
	}

#ifdef __ANDROID__
	namespace
	{
		const char* CATEGORY_NAMES[] = {
			"All", "Hats", "Tops", "Bottoms", "Overalls", "Shoes",
			"Gloves", "Capes", "Weapons", "Accessories", "Pets", "More"
		};

		// Which of the categories above an item belongs to (never 0)
		size_t category_of(int32_t itemid)
		{
			int32_t kind = itemid / 10000;

			switch (kind)
			{
			case 100: return 1;
			case 104: return 2;
			case 106: return 3;
			case 105: return 4;
			case 107: return 5;
			case 108: return 6;
			case 110: return 7;
			case 101: case 102: case 103: case 111: case 112: case 114: return 9;
			case 500: return 10;
			default:
				if (kind == 109 || (kind >= 130 && kind <= 170))
					return 8;

				return 11;
			}
		}

		constexpr int16_t M_MARGIN = 12;
		constexpr int16_t M_TOP = 46;
		constexpr int16_t M_GAP = 8;
		constexpr int16_t M_SIDE_W = 132;
		constexpr int16_t M_DETAIL_W = 270;

		Rectangle<int16_t> m_rect(int16_t x, int16_t y, int16_t w, int16_t h)
		{
			return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + w, y + h));
		}

		void m_centered(const Text& text, Rectangle<int16_t> box)
		{
			text.draw(Point<int16_t>(box.left() + box.width() / 2, classic::text_y(text, box.top() + box.height() / 2)));
		}

		std::string with_commas(int64_t value)
		{
			std::string text = std::to_string(value);
			string_format::split_number(text);
			return text;
		}
	}

	void UICashShop::init_mobile()
	{
		m_title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Cash Shop");
		m_exit = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Exit");
		m_shop_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Shop");
		m_mine_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "My items");

		for (size_t i = 0; i < MOBILE_CATEGORIES; i++)
			m_category_labels[i] = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, CATEGORY_NAMES[i]);

		m_nx = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP);
		m_detail_name = Text(Text::Font::A13B, Text::Alignment::LEFT, Color::Name::HUD_EXP, "", M_DETAIL_W - 20);
		m_detail_price = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "", M_DETAIL_W - 20);
		m_detail_desc = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::GALLERY, "", M_DETAIL_W - 20);
		m_action = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE);
		m_status = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE);
		m_hint = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Tap an item to try it on", M_DETAIL_W - 20);
		m_empty = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::SILVERCHALICE, "Nothing here yet");

		mobile_filter();
	}

	UICashShop::MobileLayout UICashShop::mobile_layout() const
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		MobileLayout l;
		int16_t pw = std::min<int16_t>(vw - 24, 1180);
		int16_t ph = vh - 24;
		int16_t px = (vw - pw) / 2;
		int16_t py = 12;

		l.panel = m_rect(px, py, pw, ph);
		l.exit = m_rect(px + pw - M_MARGIN - 100, py + 8, 100, 32);
		l.shop_mode = m_rect(px + M_MARGIN, py + 8, 90, 32);
		l.mine_mode = m_rect(l.shop_mode.right() + 6, py + 8, 110, 32);

		int16_t body_y = py + M_TOP + 8;
		int16_t body_h = py + ph - M_MARGIN - body_y;
		int16_t cat_h = std::min<int16_t>(36, (body_h - 4 * (MOBILE_CATEGORIES - 1)) / MOBILE_CATEGORIES);

		for (size_t i = 0; i < MOBILE_CATEGORIES; i++)
			l.categories[i] = m_rect(px + M_MARGIN, body_y + static_cast<int16_t>(i) * (cat_h + 4), M_SIDE_W, cat_h);

		// My items has no categories, so its cards take that column too
		int16_t grid_x = m_mine ? px + M_MARGIN : px + M_MARGIN * 2 + M_SIDE_W;
		int16_t detail_x = px + pw - M_MARGIN - M_DETAIL_W;

		l.grid = m_rect(grid_x, body_y, detail_x - M_MARGIN - grid_x, body_h);
		l.detail = m_rect(detail_x, body_y, M_DETAIL_W, body_h);
		l.stage = m_rect(detail_x + 10, body_y + 10, M_DETAIL_W - 20, std::min<int16_t>(150, body_h / 3));
		l.action = m_rect(detail_x + 10, l.detail.bottom() - 50, M_DETAIL_W - 20, 40);

		l.card = 118;
		l.cols = std::max<int16_t>(1, (l.grid.width() - 8 + M_GAP) / (l.card + M_GAP));
		l.rows = std::max<int16_t>(1, (l.grid.height() + M_GAP) / (l.card + M_GAP));

		return l;
	}

	void UICashShop::mobile_filter()
	{
		m_view.clear();

		if (m_mine)
		{
			const auto& storage = get_cash_storage();

			for (size_t i = 0; i < storage.size(); i++)
				m_view.push_back(static_cast<int16_t>(i));

			m_storage_seen = storage.size();
		}
		else
		{
			bool female = Stage::get().get_player().get_stats().get_female();

			for (size_t i = 0; i < items.size(); i++)
			{
				// Equips for the other gender can't be bought
				int8_t gender = i < item_gender.size() ? item_gender[i] : 2;

				if (gender == (female ? 0 : 1))
					continue;

				if (m_category == 0 || category_of(items[i].get_itemid()) == m_category)
					m_view.push_back(static_cast<int16_t>(i));
			}
		}

		m_first = 0;
		m_selected = -1;
		m_shown = 0;
		m_prices.clear();
	}

	int16_t UICashShop::mobile_count() const
	{
		return static_cast<int16_t>(m_view.size());
	}

	int32_t UICashShop::mobile_itemid(int16_t index) const
	{
		if (index < 0 || index >= mobile_count())
			return 0;

		int16_t source = m_view[index];

		if (m_mine)
		{
			const auto& storage = get_cash_storage();
			return source < static_cast<int16_t>(storage.size()) ? storage[source].itemid : 0;
		}

		return source < static_cast<int16_t>(items.size()) ? items[source].get_itemid() : 0;
	}

	Rectangle<int16_t> UICashShop::mobile_card(const MobileLayout& l, int16_t index) const
	{
		int16_t slot = index - m_first * l.cols;
		int16_t col = slot % l.cols;
		int16_t row = slot / l.cols;

		return m_rect(l.grid.left() + col * (l.card + M_GAP), l.grid.top() + row * (l.card + M_GAP), l.card, l.card);
	}

	void UICashShop::mobile_clamp()
	{
		MobileLayout l = mobile_layout();
		int16_t rows = (mobile_count() + l.cols - 1) / l.cols;
		m_first = std::clamp<int16_t>(m_first, 0, std::max<int16_t>(0, rows - l.rows));
	}

	void UICashShop::mobile_select(int16_t index)
	{
		m_selected = index;
		buy_armed = false;

		int32_t itemid = mobile_itemid(index);

		// Try-on: your look plus the selected equip
		preview_look = Stage::get().get_player().get_look();
		preview_has_pet = false;
		preview_icon_id = 0;

		// Your name, plain unless the item is a name tag ring
		preview_nametag = std::make_unique<NameTagStyle>(Stage::get().get_player().get_stats().get_name());
		preview_has_balloon = false;

		if (itemid >= 1000000 && itemid < 2000000)
		{
			preview_look.add_equip(itemid);

			// Label and quote rings name their plate and balloon in info
			nl::node info = nl::nx::character["Ring"]["0" + std::to_string(itemid) + ".img"]["info"];

			if (nl::node tag = info["nameTag"])
				preview_nametag->apply_style(tag.get_integer());

			if (nl::node balloon = info["chatBalloon"])
			{
				int64_t style = balloon.get_integer();

				if (style >= 0 && style <= INT8_MAX)
				{
					preview_balloon = ChatBalloon(static_cast<int8_t>(style));
					preview_balloon.change_text("Hello, Maple World!");
					preview_has_balloon = true;
				}
			}
		}
		else if (itemid / 10000 == 500)
		{
			// Pets stand beside you on the stage
			nl::node src = nl::nx::item["Pet"][std::to_string(itemid) + ".img"]["stand0"];

			if (src)
			{
				preview_pet = Animation(src);
				preview_has_pet = true;
			}
			else
			{
				preview_icon_id = itemid;
			}
		}
		else if (itemid != 0)
		{
			// Stars, effects, scrolls and the like: show the item itself
			preview_icon_id = itemid;
		}

		preview_look.set_stance(Stance::Id::STAND1);
	}

	void UICashShop::mobile_draw_icon(int32_t itemid, Rectangle<int16_t> box) const
	{
		const Texture& icon = ItemData::get(itemid).get_icon(false);

		if (!icon.is_valid())
			return;

		Point<int16_t> dim = icon.get_dimensions();
		float scale = std::min(box.width() * 0.8f / std::max<int16_t>(dim.x(), 1), box.height() * 0.8f / std::max<int16_t>(dim.y(), 1));
		scale = std::min(scale, 2.0f);

		Point<int16_t> origin = icon.get_origin();
		int16_t x = box.left() + (box.width() - static_cast<int16_t>(dim.x() * scale)) / 2 + static_cast<int16_t>(origin.x() * scale);
		int16_t y = box.top() + (box.height() - static_cast<int16_t>(dim.y() * scale)) / 2 + static_cast<int16_t>(origin.y() * scale);

		icon.draw(DrawArgument(Point<int16_t>(x, y), scale, scale));
	}

	Text& UICashShop::mobile_name(int32_t itemid, int16_t width) const
	{
		auto key = std::make_pair(itemid, width);
		auto iter = m_names.find(key);

		if (iter == m_names.end())
		{
			Text text(Text::Font::A11B, Text::Alignment::CENTER, Color::Name::WHITE, ItemData::get(itemid).get_name());
			std::string name = text.get_text();

			while (text.width() > width && name.size() > 4)
			{
				name.resize(name.size() - 2);
				text.change_text(name + "..");
			}

			iter = m_names.emplace(key, std::move(text)).first;
		}

		return iter->second;
	}

	void UICashShop::draw_mobile(float inter) const
	{
		MobileLayout l = mobile_layout();
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();

		ColorBox(vw, vh, Color::Name::BLACK, 1.0f).draw(DrawArgument(Point<int16_t>(0, 0)));
		classic::panel(l.panel.get_left_top(), l.panel.width(), l.panel.height());

		// Header: mode switch, title and NX, Exit
		classic::cell(l.shop_mode.get_left_top(), l.shop_mode.width(), l.shop_mode.height(), !m_mine);
		classic::cell(l.mine_mode.get_left_top(), l.mine_mode.width(), l.mine_mode.height(), m_mine);
		m_shop_label.change_color(!m_mine ? Color::Name::HUD_PANEL : Color::Name::WHITE);
		m_mine_label.change_color(m_mine ? Color::Name::HUD_PANEL : Color::Name::WHITE);

		std::string mine_text = "My items (" + std::to_string(get_cash_storage().size()) + ")";

		if (m_mine_label.get_text() != mine_text)
			m_mine_label.change_text(mine_text);

		m_centered(m_shop_label, l.shop_mode);
		m_centered(m_mine_label, l.mine_mode);

		m_title.draw(Point<int16_t>(l.panel.left() + l.panel.width() / 2, l.panel.top() + 8));

		std::string nx = "NX " + with_commas(get_cash_balance(0));

		if (m_nx.get_text() != nx)
			m_nx.change_text(nx);

		m_nx.draw(Point<int16_t>(l.exit.left() - 16 - m_nx.width(), classic::text_y(m_nx, l.exit.top() + l.exit.height() / 2)));

		classic::button(l.exit.get_left_top(), l.exit.width(), l.exit.height());
		m_centered(m_exit, l.exit);

		// Categories
		if (!m_mine)
		{
			for (size_t i = 0; i < MOBILE_CATEGORIES; i++)
			{
				bool lit = i == m_category;
				const Rectangle<int16_t>& r = l.categories[i];

				classic::cell(r.get_left_top(), r.width(), r.height(), lit);
				m_category_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
				m_centered(m_category_labels[i], r);
			}
		}

		// Item cards
		int16_t count = mobile_count();
		int16_t first = m_first * l.cols;
		int16_t last = std::min<int16_t>(count, first + l.rows * l.cols);

		if (count == 0)
			m_centered(m_empty, m_rect(l.grid.left(), l.grid.top(), l.grid.width(), 60));

		for (int16_t i = first; i < last; i++)
		{
			int32_t itemid = mobile_itemid(i);
			Rectangle<int16_t> r = mobile_card(l, i);
			bool lit = i == m_selected;

			classic::cell(r.get_left_top(), r.width(), r.height(), lit);
			mobile_draw_icon(itemid, m_rect(r.left() + 20, r.top() + 8, r.width() - 40, 56));

			// Name and price sit above the light line in the card art's
			// bottom strip; the price used to land on it and looked crossed out
			Text& name = mobile_name(itemid, r.width() - 18);
			name.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			name.draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(name, r.top() + 73)));

			std::string under;

			if (m_mine)
			{
				int16_t qty = get_cash_storage()[m_view[i]].quantity;
				under = qty > 1 ? "x" + std::to_string(qty) : "";
			}
			else
			{
				under = with_commas(items[m_view[i]].get_price()) + " NX";
			}

			if (!under.empty())
			{
				auto& price = m_prices[i];

				if (price.get_text() != under)
					price = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::HUD_EXP, under);

				price.change_color(lit ? Color::Name::HUD_PANEL : Color::Name::HUD_EXP);
				price.draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(price, r.top() + 90)));
			}
		}

		int16_t rows = (count + l.cols - 1) / l.cols;

		if (rows > l.rows)
		{
			int16_t track_h = l.grid.height() - M_GAP;
			int16_t thumb_h = std::max<int16_t>(24, track_h * l.rows / rows);
			int16_t thumb_y = l.grid.top() + (track_h - thumb_h) * m_first / std::max<int16_t>(1, rows - l.rows);
			int16_t bar_x = l.grid.right() - 4;

			classic::box(bar_x, l.grid.top(), 4, track_h, Color::Name::HUD_TRACK, 0.9f);
			classic::box(bar_x, thumb_y, 4, thumb_h, Color::Name::HUD_EXP);
		}

		// The selected item, on your character
		classic::box(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::BLACK, 0.25f);
		classic::frame(l.detail.left(), l.detail.top(), l.detail.width(), l.detail.height(), Color::Name::HUD_EXP, 0.4f);

		classic::cell(l.stage.get_left_top(), l.stage.width(), l.stage.height(), false);
		// Room under the feet for the name tag
		Point<int16_t> feet(l.stage.left() + l.stage.width() / 2, l.stage.bottom() - 26);

		if (preview_has_pet)
		{
			// The pet beside you, both on the same floor
			preview_look.draw(DrawArgument(feet - Point<int16_t>(30, 0), false), inter);
			preview_pet.draw(DrawArgument(feet + Point<int16_t>(40, 0), true), inter);
		}
		else if (preview_icon_id)
		{
			preview_look.draw(DrawArgument(feet - Point<int16_t>(50, 0), false), inter);

			Rectangle<int16_t> big(Point<int16_t>(feet.x() + 10, l.stage.top() + 20), Point<int16_t>(feet.x() + 90, l.stage.top() + 100));
			mobile_draw_icon(preview_icon_id, big);
		}
		else
		{
			preview_look.draw(DrawArgument(feet, false), inter);
		}

		// Name tag and chat balloon, placed as in game (Char::draw)
		Point<int16_t> wearer = feet - Point<int16_t>(preview_has_pet ? 30 : (preview_icon_id ? 50 : 0), 0);

		if (preview_nametag)
			preview_nametag->draw(wearer);

		if (preview_has_balloon)
			preview_balloon.draw(wearer - Point<int16_t>(0, 85));

		int32_t itemid = mobile_itemid(m_selected);

		if (itemid == 0)
		{
			m_hint.draw(Point<int16_t>(l.detail.left() + l.detail.width() / 2, l.stage.bottom() + 20));
		}
		else
		{
			if (m_shown != itemid)
			{
				const ItemData& data = ItemData::get(itemid);
				m_shown = itemid;
				m_detail_name.change_text(data.get_name());

				std::string desc = data.get_desc();

				if (desc.size() > 220)
				{
					size_t cut = desc.rfind(' ', 220);
					desc = desc.substr(0, cut == std::string::npos ? 220 : cut) + " ...";
				}

				m_detail_desc.change_text(desc);
			}

			std::string price_text;
			std::string action;

			if (m_mine)
			{
				int16_t qty = get_cash_storage()[m_view[m_selected]].quantity;
				price_text = qty > 1 ? "Quantity: " + std::to_string(qty) : "Ready to use";
				action = "Move to inventory";
			}
			else
			{
				const Item& item = items[m_view[m_selected]];
				price_text = with_commas(item.get_price()) + " NX" + (item.count > 1 ? "  (" + std::to_string(item.count) + " pcs)" : "");
				action = buy_armed ? "Tap again to buy" : "Buy";
			}

			if (m_detail_price.get_text() != price_text)
				m_detail_price.change_text(price_text);

			int16_t y = l.stage.bottom() + 10;
			m_detail_name.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_name.height() + 4;
			m_detail_price.draw(Point<int16_t>(l.detail.left() + 10, y));
			y += m_detail_price.height() + 8;

			if (y + m_detail_desc.height() < l.action.top() - 6)
				m_detail_desc.draw(Point<int16_t>(l.detail.left() + 10, y));

			if (m_action.get_text() != action)
				m_action.change_text(action);

			classic::button(l.action.get_left_top(), l.action.width(), l.action.height(), m_mine || buy_armed);
			m_centered(m_action, l.action);
		}

		// Purchase results and errors
		if (status_ticks > 0 && !status_message.empty())
		{
			if (m_status.get_text() != status_message)
				m_status.change_text(status_message);

			int16_t w = std::max<int16_t>(m_status.width() + 40, 300);
			Point<int16_t> p((vw - w) / 2, l.panel.bottom() - 60);

			m_status.change_color(status_error ? Color::Name::TORCHRED : Color::Name::WHITE);
			classic::panel(p, w, 40);
			m_status.draw(Point<int16_t>(p.x() + w / 2, classic::text_y(m_status, p.y() + 40 / 2)));
		}
	}

	Cursor::State UICashShop::send_cursor_mobile(bool clicked, Point<int16_t> cursorpos)
	{
		// The list follows the cash inventory as items arrive or leave
		if (m_mine && get_cash_storage().size() != m_storage_seen)
			mobile_filter();

		// Picking a card only changes the preview, so it happens as soon as
		// the finger lands; buying and switching still wait for the lift
		// (the press itself: the UI may be disabled, which turns clicks
		// into plain moves, so the held button is what counts)
		bool down = clicked || UI::get().is_mouse_held();
		bool pressed_now = down && !m_was_down;
		m_was_down = down;

		if (pressed_now)
		{
			MobileLayout pl = mobile_layout();

			if (pl.grid.contains(cursorpos))
			{
				int16_t first = m_first * pl.cols;
				int16_t last = std::min<int16_t>(mobile_count(), first + pl.rows * pl.cols);

				for (int16_t i = first; i < last; i++)
				{
					if (mobile_card(pl, i).contains(cursorpos))
					{
						if (i != m_selected)
							mobile_select(i);

						break;
					}
				}
			}
		}

		// Moves arrive without the button state while a finger is held;
		// act when it lifts where it landed, so swipes never buy
		Point<int16_t> at;

		if (!m_gesture.update(clicked || UI::get().is_mouse_held(), cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		cursorpos = at;
		MobileLayout l = mobile_layout();

		if (l.exit.contains(cursorpos))
		{
			request_exit();
			return Cursor::State::CLICKING;
		}

		if (l.shop_mode.contains(cursorpos) || l.mine_mode.contains(cursorpos))
		{
			bool mine = l.mine_mode.contains(cursorpos);

			if (mine != m_mine)
			{
				m_mine = mine;
				m_prices.clear();
				mobile_filter();
			}

			return Cursor::State::CLICKING;
		}

		if (!m_mine)
		{
			for (size_t i = 0; i < MOBILE_CATEGORIES; i++)
			{
				if (l.categories[i].contains(cursorpos))
				{
					if (m_category != i)
					{
						m_category = i;
						m_prices.clear();
						mobile_filter();
					}

					return Cursor::State::CLICKING;
				}
			}
		}

		if (l.grid.contains(cursorpos))
		{
			int16_t first = m_first * l.cols;
			int16_t last = std::min<int16_t>(mobile_count(), first + l.rows * l.cols);

			for (int16_t i = first; i < last; i++)
			{
				if (mobile_card(l, i).contains(cursorpos))
				{
					if (i != m_selected)
						mobile_select(i);

					return Cursor::State::CLICKING;
				}
			}

			buy_armed = false;
			return Cursor::State::IDLE;
		}

		if (mobile_itemid(m_selected) != 0 && l.action.contains(cursorpos))
		{
			if (m_mine)
			{
				const CashStorageItem& item = get_cash_storage()[m_view[m_selected]];
				show_status("Moving " + ItemData::get(item.itemid).get_name() + "...", false);
				take_from_cash_storage(item.cashid);
			}
			else if (!buy_armed)
			{
				// A second tap buys, so a stray touch never spends NX
				buy_armed = true;
			}
			else
			{
				const Item& item = items[m_view[m_selected]];
				BuyCashItemPacket(1, item.sn).dispatch();
				show_status("Buying " + item.get_name() + "...", false);
				buy_armed = false;
			}

			return Cursor::State::CLICKING;
		}

		buy_armed = false;
		return Cursor::State::IDLE;
	}
#endif
}
