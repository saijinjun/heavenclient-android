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

#include "../Components/Textfield.h"

#include "../../Graphics/Geometry.h"
#include "../../Template/BoolPair.h"

namespace ms
{
	class UILogin : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::LOGIN;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UILogin();

		void draw(float alpha) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursor_pos) override;

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t id) override;

	public:
		void login();

	private:
		void open_url(uint16_t id);

		// The phone layout: one navy and gold sign-in card with large fields
		// in place of the small classic signboard, see draw_card()
		void draw_card(Point<int16_t> drawpos) const;
		Cursor::State send_card_cursor(bool clicked, Point<int16_t> cursorpos);
		void toggle_saveid();
		void guest_login();
		// Where the card's parts are, in screen coordinates for drawpos
		Point<int16_t> card_origin(Point<int16_t> drawpos) const;
		Rectangle<int16_t> card_part(Point<int16_t> drawpos, int16_t y, int16_t h) const;

		enum Buttons
		{
			BT_LOGIN,
			BT_REGISTER,
			BT_HOMEPAGE,
			BT_PASSLOST,
			BT_IDLOST,
			BT_SAVEID,
			BT_QUIT,
			BT_GUEST_LOGIN,
			NUM_BUTTONS
		};

		Text version;
		Textfield account;
		Textfield password;
		Texture accountbg;
		Texture passwordbg;
		BoolPair<Texture> checkbox;
		ColorBox background;
		Point<int16_t> signboard_pos;

		bool saveid;

		bool phone_layout;
		// How far the card has slid up to stay above the phone keyboard
		float card_shift;
		bool cursor_was_down;
		Text account_hint;
		Text password_hint;
		Text login_label;
		Text saveid_label;
		Text new_account_tip;
		Text guest_label;
		Text guest_note;
	};
}