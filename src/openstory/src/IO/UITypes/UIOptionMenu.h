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

#include "../Components/TapGesture.h"
#include "../UIDragElement.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"

namespace ms
{
	class UIOptionMenu : public UIDragElement<PosOPTIONMENU>
	{
	public:
		static constexpr Type TYPE = UIElement::Type::OPTIONMENU;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = false;

		UIOptionMenu();

		void draw(float inter) const override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
#ifdef __ANDROID__
		void remove_cursor() override;
#endif

		UIElement::Type get_type() const override;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void load_settings();
		void save_settings();
		void revert_settings();

		enum Buttons : uint16_t
		{
			BT_OK,
			BT_CANCEL
		};

		// NX sprites
		Texture scroll_normal;
		Texture scroll_mouseover;
		Texture scroll_pressed;
		Texture scroll_disabled;
		Texture check_sprite;

		// Sliders
		enum SliderType : int16_t
		{
			SL_NONE = -1,
			SL_GRAPHICS,
			SL_EFFECTS,
			SL_BGM,
			SL_SFX,
			SL_MOUSE_SPEED,
			SL_HP_WARNING,
			SL_MP_WARNING,
			NUM_SLIDERS
		};

		int16_t slider_y[NUM_SLIDERS];
		int16_t slider_x[NUM_SLIDERS];
		int16_t slider_w[NUM_SLIDERS];
		uint8_t slider_val[NUM_SLIDERS];
		uint8_t slider_orig[NUM_SLIDERS];
		SliderType active_slider = SL_NONE;

		// Checkboxes — each has its own position
		enum CheckType : uint16_t
		{
			CHK_RES_800, CHK_RES_1024,
			CHK_WINDOWED,
			CHK_MUTE_BGM, CHK_MUTE_SFX,
			CHK_SCREEN_SHAKE,
			CHK_MINIMAP_NORMAL, CHK_MINIMAP_SIMPLE,
			// Social (left column)
			CHK_WHISPER, CHK_CHAT_INVITE, CHK_PARTY_INVITE,
			CHK_GUILD_CHAT, CHK_ALLIANCE_CHAT, CHK_FAMILY_INVITE,
			// Social (right column)
			CHK_FRIEND_CHAT, CHK_TRADE_REQUEST, CHK_EXPEDITION_INVITE,
			CHK_GUILD_INVITE, CHK_ALLIANCE_INVITE, CHK_FOLLOW,
			NUM_CHECKS
		};

		Point<int16_t> check_positions[NUM_CHECKS];
		bool check_val[NUM_CHECKS];
		bool check_orig[NUM_CHECKS];
		bool day_night = true;
		bool day_night_orig = true;

		static constexpr int16_t CHECK_HIT_SIZE = 14;

		// Slider percentage labels
		mutable Text slider_labels[NUM_SLIDERS];

#ifdef __ANDROID__
		static constexpr int16_t PHONE_W = 660;
		static constexpr int16_t PHONE_H = 464;
		static constexpr int16_t PHONE_ROWS_Y = 84;
		static constexpr int16_t PHONE_ROW_STEP = 46;
		static constexpr int16_t PHONE_TRACK_X = 150;
		static constexpr int16_t PHONE_TRACK_W = 300;
		static constexpr size_t PHONE_SLIDERS = 6;
		static constexpr size_t PHONE_SOCIAL = 12;
		static constexpr size_t PHONE_TABS = 2;

		void draw_toggle(Rectangle<int16_t> r, const Text& label, bool on) const;
		void draw_label(Rectangle<int16_t> r, const Text& label) const;
		void set_slider(SliderType slider, int16_t cursor_x);
		void toggle(uint16_t check);
		Cursor::State phone_tap(Point<int16_t> at);
		SliderType slider_at(Point<int16_t> cursorpos) const;
		SliderType phone_slider(size_t index) const;
		uint16_t phone_social(size_t index) const;
		int16_t phone_slider_y(size_t index) const;
		Rectangle<int16_t> phone_tab_rect(size_t index) const;
		Rectangle<int16_t> phone_mute_rect(size_t index) const;
		Rectangle<int16_t> phone_shake_rect() const;
		Rectangle<int16_t> phone_minimap_rect() const;
		Rectangle<int16_t> phone_night_rect() const;
		Rectangle<int16_t> phone_edit_rect() const;
		Rectangle<int16_t> phone_social_rect(size_t index) const;
		Rectangle<int16_t> phone_ok_rect() const;
		Rectangle<int16_t> phone_cancel_rect() const;

		TapGesture gesture;
		uint8_t phone_tab = 0;
		Text phone_title;
		Text phone_hint;
		Text phone_ok;
		Text phone_cancel;
		Text phone_edit;
		Text phone_mute;
		Text phone_mute_on;
		Text phone_shake;
		Text phone_shake_on;
		Text phone_minimap;
		Text phone_minimap_on;
		Text phone_night;
		Text phone_night_on;
		Text phone_tabs[PHONE_TABS];
		Text phone_slider_names[PHONE_SLIDERS];
		Text phone_social_names[PHONE_SOCIAL];
		Text phone_social_names_on[PHONE_SOCIAL];
#endif
	};
}
