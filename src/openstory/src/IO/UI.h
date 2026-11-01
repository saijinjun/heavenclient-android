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

#include "UIState.h"

#include "Components/ScrollingNotice.h"
#include "Components/Textfield.h"

namespace ms
{
	class UI : public Singleton<UI>
	{
	public:
		enum State
		{
			LOGIN,
			GAME,
			CASHSHOP
		};

		UI();

		void init();
		void draw(float alpha) const;
		void update();

		void enable();
		void disable();
		void change_state(State state);
		// Whether the game screen is showing (not login or the cash shop).
		bool in_game() const;
		bool in_login() const;
		bool in_cashshop() const;
		// A full-screen touch panel (the mobile inventory) is open: the
		// touch controls step aside and a swipe scrolls it
		bool in_mobile_panel() const;
		// Sets the view for the login screens on a screen of this size.
		void fit_login_view(int16_t screen_width, int16_t screen_height);
		// Whether a soft keyboard covers the bottom of the screen, so text
		// being typed has to be shown at the top instead.
		void set_typing_on_top(bool on_top);
		bool is_typing_on_top() const;

		void quit();
		bool not_quitted() const;

		void send_cursor(Point<int16_t> pos);
		void send_cursor(bool pressed);
		void send_cursor(Point<int16_t> cursorpos, Cursor::State cursorstate);
		bool is_mouse_held() const;
		// Current mouse-cursor position (used by scroll dispatch so
		// the wheel routes to the UI under the cursor).
		Point<int16_t> get_cursor_position() const;
		void send_focus(int focused);
		// Dispatches a key-up for everything currently latched down and
		// clears the held-key table. Called on focus loss, where the OS
		// never delivers the real releases.
		void release_held_keys();
		void send_scroll(double yoffset);
		void send_close();
		void rightclick();
		void doubleclick();
		void send_key(int32_t keycode, bool pressed);
		// An action straight to the game, without a key behind it: the touch
		// controls use this for moving, attacking and opening windows.
		void send_action(KeyType::Id type, int32_t action, bool pressed);
		Icon* get_dragged_icon();
		void cancel_drag();
		// One Unicode character produced by the OS keyboard layout / IME.
		// This is the only route by which non-ASCII text (Hebrew, CJK) can be
		// typed; send_key sees physical keys and cannot represent it.
		void send_char(uint32_t codepoint);
		// Whether a text field has focus (the Android port shows the soft keyboard then).
		bool has_focused_textfield() const;

		void set_scrollnotice(const std::string& notice);
		void focus_textfield(Textfield* textfield);
		void remove_textfield();
		void drag_icon(Icon* icon);
		// Forget a dragged-icon reference before its owner destroys it
		void purge_icon(const Icon* icon);

		void add_keymapping(uint8_t no, uint8_t type, int32_t action);

		void clear_tooltip(Tooltip::Parent parent);
		void show_equip(Tooltip::Parent parent, int16_t slot);
		void show_item(Tooltip::Parent parent, int32_t item_id);
		void show_skill(Tooltip::Parent parent, int32_t skill_id, int32_t level, int32_t masterlevel, int64_t expiration);
		void show_text(Tooltip::Parent parent, std::string text);
		void show_map(Tooltip::Parent parent, std::string name, std::string description, int32_t mapid, bool bolded);

		Keyboard& get_keyboard();

		template <class T, typename...Args>
		Optional<T> emplace(Args&& ...args)
		{
			if (auto iter = state->pre_add(T::TYPE, T::TOGGLED, T::FOCUSED))
			{
				(*iter).second = std::make_unique<T>(
					std::forward<Args>(args)...
					);
			}

			return state->get(T::TYPE);
		}

		template <class T>
		Optional<T> get_element()
		{
			UIElement::Type type = T::TYPE;
			UIElement* element = state->get(type);

			return static_cast<T*>(element);
		}

		void remove(UIElement::Type type);

	private:
		std::unique_ptr<UIState> state;
		Keyboard keyboard;
		Cursor cursor;
		ScrollingNotice scrollingnotice;

		Optional<Textfield> focusedtextfield;
		std::unordered_map<int32_t, bool> is_key_down;

		bool enabled;
		State state_id = State::LOGIN;
		bool typing_on_top = false;
		bool quitted;
		bool caps_lock_enabled = false;
		bool mouse_held = false;
	};
}