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
#include "UIOptionMenu.h"

#include "../KeyAction.h"

#include "../Components/MapleButton.h"
#include "../UI.h"
#include "../Window.h"
#include "UIMiniMap.h"

#include "../../Audio/Audio.h"
#include "../../Configuration.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Graphics/ClassicStyle.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIOptionMenu::UIOptionMenu() : UIDragElement<PosOPTIONMENU>(Point<int16_t>(283, 20)), active_slider(SL_NONE)
	{
		// Always position the menu at the center of the screen
		int16_t screen_w = Constants::Constants::get().get_viewwidth();
		int16_t screen_h = Constants::Constants::get().get_viewheight();
		position = Point<int16_t>(
			(screen_w - 283) / 2,
			(screen_h - 558) / 2
		);

		nl::node OptionMenu = nl::nx::ui["UIWindow2.img"]["OptionMenu"];

		if (OptionMenu)
		{
			sprites.emplace_back(OptionMenu["backgrnd"]);
			sprites.emplace_back(OptionMenu["backgrnd2"]);
			sprites.emplace_back(OptionMenu["backgrnd3"]);

			buttons[BT_OK] = std::make_unique<MapleButton>(OptionMenu["BtOK"]);
			buttons[BT_CANCEL] = std::make_unique<MapleButton>(OptionMenu["BtCancle"]);

			nl::node scroll = OptionMenu["scroll"];
			scroll_normal = scroll["0"];
			scroll_mouseover = scroll["1"];
			scroll_pressed = scroll["2"];
			scroll_disabled = scroll["3"];

			check_sprite = OptionMenu["check"];

			dimension = Point<int16_t>(283, 558);
			dragarea = Point<int16_t>(283, 20);
		}
		else
		{
			dimension = Point<int16_t>(283, 558);
			dragarea = Point<int16_t>(283, 20);
		}

		// Slider layout — per-slider track position and width
		// Common sliders
		for (int i = 0; i < NUM_SLIDERS; i++)
		{
			slider_x[i] = 95;
			slider_w[i] = 110;
		}

		// Sound sliders have a shorter track
		slider_x[SL_BGM] = 95;
		slider_w[SL_BGM] = 65;
		slider_x[SL_SFX] = 95;
		slider_w[SL_SFX] = 65;

		// Slider Y positions (matching background sections)
		slider_y[SL_GRAPHICS]    = 30;
		slider_y[SL_EFFECTS]     = 60;
		slider_y[SL_BGM]         = 150;
		slider_y[SL_SFX]         = 180;
		slider_y[SL_MOUSE_SPEED] = 240;
		slider_y[SL_HP_WARNING]  = 270;
		slider_y[SL_MP_WARNING]  = 300;

		// Checkbox positions — using confirmed slider Y positions as anchors:
		// Sliders: GRAPHICS=30, EFFECTS=60, BGM=150, SFX=180, MOUSE=240, HP=270, MP=300
		// RESOLUTION is between EFFECTS(60) and BGM(150)
		check_positions[CHK_RES_800]  = Point<int16_t>(69, 94);
		check_positions[CHK_RES_1024] = Point<int16_t>(170, 94);
		check_positions[CHK_WINDOWED] = Point<int16_t>(70, 123);
		check_positions[CHK_MUTE_BGM] = Point<int16_t>(228, 154);
		check_positions[CHK_MUTE_SFX] = Point<int16_t>(228, 184);
		check_positions[CHK_SCREEN_SHAKE] = Point<int16_t>(70, 335);
		check_positions[CHK_MINIMAP_NORMAL] = Point<int16_t>(69, 393);
		check_positions[CHK_MINIMAP_SIMPLE] = Point<int16_t>(171, 392);

		// Social — left column
		check_positions[CHK_WHISPER]           = Point<int16_t>(128, 423);
		check_positions[CHK_CHAT_INVITE]       = Point<int16_t>(127, 442);
		check_positions[CHK_PARTY_INVITE]      = Point<int16_t>(129, 459);
		check_positions[CHK_GUILD_CHAT]        = Point<int16_t>(127, 477);
		check_positions[CHK_ALLIANCE_CHAT]     = Point<int16_t>(128, 495);
		check_positions[CHK_FAMILY_INVITE]     = Point<int16_t>(128, 512);

		// Social — right column
		check_positions[CHK_FRIEND_CHAT]       = Point<int16_t>(261, 423);
		check_positions[CHK_TRADE_REQUEST]     = Point<int16_t>(260, 442);
		check_positions[CHK_EXPEDITION_INVITE] = Point<int16_t>(260, 459);
		check_positions[CHK_GUILD_INVITE]      = Point<int16_t>(262, 479);
		check_positions[CHK_ALLIANCE_INVITE]   = Point<int16_t>(261, 496);
		check_positions[CHK_FOLLOW]            = Point<int16_t>(260, 513);

		// Initialize all checkbox values
		for (int i = 0; i < NUM_CHECKS; i++)
		{
			check_val[i] = false;
			check_orig[i] = false;
		}

		// Default social checkboxes to checked (ALLOW)
		for (int i = CHK_WHISPER; i <= CHK_FOLLOW; i++)
		{
			check_val[i] = true;
			check_orig[i] = true;
		}

		// Default minimap to normal
		check_val[CHK_MINIMAP_NORMAL] = true;
		check_orig[CHK_MINIMAP_NORMAL] = true;

		// Initialize slider percentage labels
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_labels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::BLACK);

		load_settings();

#ifdef __ANDROID__
		// Drawn and tapped by hand, in the navy style of the phone panels.
		// Resolution, windowed mode, mouse speed and the desktop hotkeys don't
		// apply on a phone, so they aren't offered; their values are kept.
		sprites.clear();
		buttons.clear();

		dimension = Point<int16_t>(PHONE_W, PHONE_H);
		dragarea = Point<int16_t>(0, 0);
		position = Point<int16_t>((screen_w - PHONE_W) / 2, (screen_h - PHONE_H) / 2);

		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_labels[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::WHITE);

		auto label = [](const char* text, Color::Name color = Color::Name::WHITE)
		{
			return Text(Text::Font::A11B, Text::Alignment::CENTER, color, text);
		};

		phone_title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Options");
		phone_hint = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::SILVERCHALICE, "Lit means other players can do this.");
		phone_ok = label("OK");
		phone_cancel = label("Cancel");
		phone_mute = label("Mute");
		phone_mute_on = label("Mute", Color::Name::HUD_PANEL);
		phone_shake = label("Screen shake");
		phone_shake_on = label("Screen shake", Color::Name::HUD_PANEL);
		phone_minimap = label("Simple minimap");
		phone_minimap_on = label("Simple minimap", Color::Name::HUD_PANEL);
		phone_tabs[0] = label("Game");
		phone_tabs[1] = label("Social");

		const char* slider_names[PHONE_SLIDERS] = { "Graphics", "Effects", "Music", "Sound", "HP warning", "MP warning" };

		for (size_t i = 0; i < PHONE_SLIDERS; i++)
			phone_slider_names[i] = Text(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::HUD_EXP, slider_names[i]);

		const char* social_names[PHONE_SOCIAL] = {
			"Whisper", "Friend chat", "Chat invite", "Trade request", "Party invite", "Expedition invite",
			"Guild chat", "Guild invite", "Alliance chat", "Alliance invite", "Family invite", "Follow"
		};

		for (size_t i = 0; i < PHONE_SOCIAL; i++)
		{
			phone_social_names[i] = label(social_names[i]);
			phone_social_names_on[i] = label(social_names[i], Color::Name::HUD_PANEL);
		}
#endif
	}

	void UIOptionMenu::draw(float inter) const
	{
#ifdef __ANDROID__
		classic::panel(position, PHONE_W, PHONE_H);
		phone_title.draw(position + Point<int16_t>(PHONE_W / 2, 6));

		for (size_t i = 0; i < PHONE_TABS; i++)
		{
			Rectangle<int16_t> r = phone_tab_rect(i);

			classic::tab(r.get_left_top(), r.width(), r.height(), i == phone_tab);
			draw_label(r, phone_tabs[i]);
		}

		if (phone_tab == 0)
		{
			for (size_t i = 0; i < PHONE_SLIDERS; i++)
			{
				SliderType slider = phone_slider(i);
				int16_t cy = position.y() + phone_slider_y(i);
				int16_t track_x = position.x() + PHONE_TRACK_X;
				int16_t filled = static_cast<int16_t>(slider_val[slider] * PHONE_TRACK_W / 100);

				phone_slider_names[i].draw(Point<int16_t>(position.x() + 24, classic::text_y(phone_slider_names[i], cy)));

				classic::box(track_x, cy - 3, PHONE_TRACK_W, 6, Color::Name::HUD_TRACK, 0.95f);
				classic::box(track_x, cy - 3, filled, 6, Color::Name::HUD_EXP);
				classic::button(Point<int16_t>(track_x + filled - 10, cy - 14), 20, 28, active_slider == slider);

				slider_labels[slider].change_text(std::to_string(slider_val[slider]) + "%");
				slider_labels[slider].draw(Point<int16_t>(track_x + PHONE_TRACK_W + 16, classic::text_y(slider_labels[slider], cy)));

				if (slider == SL_BGM || slider == SL_SFX)
				{
					bool muted = check_val[slider == SL_BGM ? CHK_MUTE_BGM : CHK_MUTE_SFX];

					draw_toggle(phone_mute_rect(i), muted ? phone_mute_on : phone_mute, muted);
				}
			}

			draw_toggle(phone_shake_rect(), check_val[CHK_SCREEN_SHAKE] ? phone_shake_on : phone_shake, check_val[CHK_SCREEN_SHAKE]);
			draw_toggle(phone_minimap_rect(), check_val[CHK_MINIMAP_SIMPLE] ? phone_minimap_on : phone_minimap, check_val[CHK_MINIMAP_SIMPLE]);
		}
		else
		{
			phone_hint.draw(position + Point<int16_t>(24, 76));

			for (size_t i = 0; i < PHONE_SOCIAL; i++)
			{
				bool on = check_val[phone_social(i)];

				draw_toggle(phone_social_rect(i), on ? phone_social_names_on[i] : phone_social_names[i], on);
			}
		}

		Rectangle<int16_t> ok = phone_ok_rect();
		Rectangle<int16_t> cancel = phone_cancel_rect();

		classic::button(ok.get_left_top(), ok.width(), ok.height());
		classic::shadowed(phone_ok, Point<int16_t>(ok.left() + ok.width() / 2, classic::text_y(phone_ok, ok.top() + ok.height() / 2)));
		classic::button(cancel.get_left_top(), cancel.width(), cancel.height());
		classic::shadowed(phone_cancel, Point<int16_t>(cancel.left() + cancel.width() / 2, classic::text_y(phone_cancel, cancel.top() + cancel.height() / 2)));

		return;
#endif

		UIElement::draw_sprites(inter);

		// Draw all slider thumbs and percentage labels
		for (int i = 0; i < NUM_SLIDERS; i++)
		{
			int16_t thumb_x = slider_x[i] + static_cast<int16_t>(slider_val[i] * slider_w[i] / 100);
			Point<int16_t> thumb_pos = position + Point<int16_t>(thumb_x, slider_y[i]);

			if (active_slider == i && scroll_pressed.is_valid())
				scroll_pressed.draw(DrawArgument(thumb_pos));
			else if (scroll_normal.is_valid())
				scroll_normal.draw(DrawArgument(thumb_pos));

			// Draw percentage text to the right of the slider track
			slider_labels[i].change_text(std::to_string(slider_val[i]) + "%");
			int16_t label_x = slider_x[i] + slider_w[i] + 14;
			Point<int16_t> label_pos = position + Point<int16_t>(label_x, slider_y[i] - 6);
			slider_labels[i].draw(DrawArgument(label_pos));
		}

		// Draw checkboxes
		if (check_sprite.is_valid())
		{
			for (int i = 0; i < NUM_CHECKS; i++)
			{
				if (check_val[i])
					check_sprite.draw(DrawArgument(position + check_positions[i], 1.0f));
			}
		}

		UIElement::draw_buttons(inter);
	}

	Button::State UIOptionMenu::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case BT_CANCEL:
			revert_settings();
			deactivate();
			return Button::State::NORMAL;
		case BT_OK:
			save_settings();
			deactivate();
			return Button::State::NORMAL;
		default:
			return Button::State::DISABLED;
		}
	}

	Cursor::State UIOptionMenu::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
#ifdef __ANDROID__
		if (active_slider != SL_NONE)
		{
			if (clicked)
			{
				set_slider(active_slider, cursorpos.x());
				return Cursor::State::CLICKING;
			}

			active_slider = SL_NONE;
			gesture.reset();

			return Cursor::State::IDLE;
		}

		// A slider moves with the finger from the moment it lands
		if (clicked && !gesture.is_pressed() && phone_tab == 0)
		{
			SliderType slider = slider_at(cursorpos);

			if (slider != SL_NONE)
			{
				active_slider = slider;
				set_slider(slider, cursorpos.x());

				return Cursor::State::CLICKING;
			}
		}

		// Everything else acts when the finger lifts where it landed
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		return phone_tap(at);
#endif

		// Continue slider drag
		if (clicked && active_slider != SL_NONE)
		{
			int16_t tx = slider_x[active_slider];
			int16_t tw = slider_w[active_slider];
			int16_t relative_x = cursorpos.x() - position.x() - tx;
			if (relative_x < 0) relative_x = 0;
			if (relative_x > tw) relative_x = tw;

			slider_val[active_slider] = static_cast<uint8_t>(relative_x * 100 / tw);

			// Apply live audio preview for BGM/SFX
			if (active_slider == SL_BGM && !check_val[CHK_MUTE_BGM])
				Music::set_bgmvolume(slider_val[SL_BGM]);
			else if (active_slider == SL_SFX && !check_val[CHK_MUTE_SFX])
				Sound::set_sfxvolume(slider_val[SL_SFX]);

			return Cursor::State::CLICKING;
		}
		else if (!clicked && active_slider != SL_NONE)
		{
			active_slider = SL_NONE;
		}

		if (clicked)
		{
			int16_t rel_x = cursorpos.x() - position.x();
			int16_t rel_y = cursorpos.y() - position.y();
			int16_t thumb_half = 10;



			// Check all slider areas
			for (int i = 0; i < NUM_SLIDERS; i++)
			{
				int16_t tx = slider_x[i];
				int16_t tw = slider_w[i];

				if (rel_x >= tx - thumb_half && rel_x <= tx + tw + thumb_half &&
					rel_y >= slider_y[i] - thumb_half && rel_y <= slider_y[i] + thumb_half)
				{
					active_slider = static_cast<SliderType>(i);
					int16_t track_x = rel_x - tx;
					if (track_x < 0) track_x = 0;
					if (track_x > tw) track_x = tw;
					slider_val[i] = static_cast<uint8_t>(track_x * 100 / tw);

					if (i == SL_BGM && !check_val[CHK_MUTE_BGM])
						Music::set_bgmvolume(slider_val[SL_BGM]);
					else if (i == SL_SFX && !check_val[CHK_MUTE_SFX])
						Sound::set_sfxvolume(slider_val[SL_SFX]);

					return Cursor::State::CLICKING;
				}
			}

			// Check all checkbox areas
			for (int i = 0; i < NUM_CHECKS; i++)
			{
				Point<int16_t> chk = check_positions[i];
				if (rel_x >= chk.x() - 2 && rel_x <= chk.x() + CHECK_HIT_SIZE &&
					rel_y >= chk.y() - 2 && rel_y <= chk.y() + CHECK_HIT_SIZE)
				{
					// Handle radio button groups (only one can be active)
					if (i == CHK_RES_800 || i == CHK_RES_1024)
					{
						check_val[CHK_RES_800] = (i == CHK_RES_800);
						check_val[CHK_RES_1024] = (i == CHK_RES_1024);
					}
					else if (i == CHK_MINIMAP_NORMAL || i == CHK_MINIMAP_SIMPLE)
					{
						check_val[CHK_MINIMAP_NORMAL] = (i == CHK_MINIMAP_NORMAL);
						check_val[CHK_MINIMAP_SIMPLE] = (i == CHK_MINIMAP_SIMPLE);
					}
					else
					{
						check_val[i] = !check_val[i];

						// Apply mute immediately
						if (i == CHK_MUTE_BGM)
							Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
						else if (i == CHK_MUTE_SFX)
							Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);
					}

					return Cursor::State::CANCLICK;
				}
			}
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIOptionMenu::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (escape)
			{
				revert_settings();
				deactivate();
			}
			else if (keycode == KeyAction::Id::RETURN)
				button_pressed(BT_OK);
		}
	}

	UIElement::Type UIOptionMenu::get_type() const
	{
		return TYPE;
	}

	void UIOptionMenu::load_settings()
	{
		slider_val[SL_BGM] = Setting<BGMVolume>::get().load();
		slider_val[SL_SFX] = Setting<SFXVolume>::get().load();
		slider_val[SL_GRAPHICS] = Setting<GraphicsQuality>::get().load();
		slider_val[SL_EFFECTS] = Setting<EffectsQuality>::get().load();
		slider_val[SL_MOUSE_SPEED] = Setting<MouseSpeed>::get().load();
		slider_val[SL_HP_WARNING] = Setting<HPWarning>::get().load();
		slider_val[SL_MP_WARNING] = Setting<MPWarning>::get().load();

		// Resolution radio buttons (cosmetic only — does not change resolution)
		check_val[CHK_RES_800] = false;
		check_val[CHK_RES_1024] = true;

		// Windowed checkbox (inverted from Fullscreen setting)
		check_val[CHK_WINDOWED] = !Setting<Fullscreen>::get().load();

		// Screen shake
		check_val[CHK_SCREEN_SHAKE] = Setting<ScreenShake>::get().load();

		// Minimap mode
		bool simple_mode = Setting<MiniMapSimpleMode>::get().load();
		check_val[CHK_MINIMAP_NORMAL] = !simple_mode;
		check_val[CHK_MINIMAP_SIMPLE] = simple_mode;

		// Social filters
		check_val[CHK_WHISPER]           = Setting<AllowWhisper>::get().load();
		check_val[CHK_CHAT_INVITE]       = Setting<AllowChatInvite>::get().load();
		check_val[CHK_PARTY_INVITE]      = Setting<AllowPartyInvite>::get().load();
		check_val[CHK_GUILD_CHAT]        = Setting<AllowGuildChat>::get().load();
		check_val[CHK_ALLIANCE_CHAT]     = Setting<AllowAllianceChat>::get().load();
		check_val[CHK_FAMILY_INVITE]     = Setting<AllowFamilyInvite>::get().load();
		check_val[CHK_FRIEND_CHAT]       = Setting<AllowFriendChat>::get().load();
		check_val[CHK_TRADE_REQUEST]     = Setting<AllowTradeRequest>::get().load();
		check_val[CHK_EXPEDITION_INVITE] = Setting<AllowExpeditionInvite>::get().load();
		check_val[CHK_GUILD_INVITE]      = Setting<AllowGuildInvite>::get().load();
		check_val[CHK_ALLIANCE_INVITE]   = Setting<AllowAllianceInvite>::get().load();
		check_val[CHK_FOLLOW]            = Setting<AllowFollow>::get().load();

		// Save originals for cancel/revert
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_orig[i] = slider_val[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_orig[i] = check_val[i];
	}

	void UIOptionMenu::save_settings()
	{
		// Audio sliders
		Setting<BGMVolume>::get().save(slider_val[SL_BGM]);
		Setting<SFXVolume>::get().save(slider_val[SL_SFX]);

		// Other sliders
		Setting<GraphicsQuality>::get().save(slider_val[SL_GRAPHICS]);
		Setting<EffectsQuality>::get().save(slider_val[SL_EFFECTS]);
		Setting<MouseSpeed>::get().save(slider_val[SL_MOUSE_SPEED]);
		Window::get().apply_mouse_speed(slider_val[SL_MOUSE_SPEED]);
		Setting<HPWarning>::get().save(slider_val[SL_HP_WARNING]);
		Setting<MPWarning>::get().save(slider_val[SL_MP_WARNING]);

		// Windowed checkbox (inverted — windowed means NOT fullscreen)
		bool want_fullscreen = !check_val[CHK_WINDOWED];
		bool was_fullscreen = Setting<Fullscreen>::get().load();

		if (want_fullscreen != was_fullscreen)
		{
			Setting<Fullscreen>::get().save(want_fullscreen);
			Window::get().toggle_fullscreen();
		}

		// Screen shake
		Setting<ScreenShake>::get().save(check_val[CHK_SCREEN_SHAKE]);

		// Minimap mode — recreate minimap if mode changed
		bool old_simple = Setting<MiniMapSimpleMode>::get().load();
		Setting<MiniMapSimpleMode>::get().save(check_val[CHK_MINIMAP_SIMPLE]);

		if (check_val[CHK_MINIMAP_SIMPLE] != old_simple)
		{
			UI::get().remove(UIElement::Type::MINIMAP);
			UI::get().emplace<UIMiniMap>(Stage::get().get_player().get_stats());
		}

		// Social filters
		Setting<AllowWhisper>::get().save(check_val[CHK_WHISPER]);
		Setting<AllowChatInvite>::get().save(check_val[CHK_CHAT_INVITE]);
		Setting<AllowPartyInvite>::get().save(check_val[CHK_PARTY_INVITE]);
		Setting<AllowGuildChat>::get().save(check_val[CHK_GUILD_CHAT]);
		Setting<AllowAllianceChat>::get().save(check_val[CHK_ALLIANCE_CHAT]);
		Setting<AllowFamilyInvite>::get().save(check_val[CHK_FAMILY_INVITE]);
		Setting<AllowFriendChat>::get().save(check_val[CHK_FRIEND_CHAT]);
		Setting<AllowTradeRequest>::get().save(check_val[CHK_TRADE_REQUEST]);
		Setting<AllowExpeditionInvite>::get().save(check_val[CHK_EXPEDITION_INVITE]);
		Setting<AllowGuildInvite>::get().save(check_val[CHK_GUILD_INVITE]);
		Setting<AllowAllianceInvite>::get().save(check_val[CHK_ALLIANCE_INVITE]);
		Setting<AllowFollow>::get().save(check_val[CHK_FOLLOW]);

		// Apply final audio
		Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
		Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);

		Configuration::get().save();

		// Update originals
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_orig[i] = slider_val[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_orig[i] = check_val[i];
	}

#ifdef __ANDROID__
	void UIOptionMenu::remove_cursor()
	{
		active_slider = SL_NONE;
		gesture.reset();
		UIDragElement::remove_cursor();
	}

	UIOptionMenu::SliderType UIOptionMenu::phone_slider(size_t index) const
	{
		const SliderType sliders[PHONE_SLIDERS] = { SL_GRAPHICS, SL_EFFECTS, SL_BGM, SL_SFX, SL_HP_WARNING, SL_MP_WARNING };

		return sliders[index];
	}

	uint16_t UIOptionMenu::phone_social(size_t index) const
	{
		const uint16_t checks[PHONE_SOCIAL] = {
			CHK_WHISPER, CHK_FRIEND_CHAT, CHK_CHAT_INVITE, CHK_TRADE_REQUEST, CHK_PARTY_INVITE, CHK_EXPEDITION_INVITE,
			CHK_GUILD_CHAT, CHK_GUILD_INVITE, CHK_ALLIANCE_CHAT, CHK_ALLIANCE_INVITE, CHK_FAMILY_INVITE, CHK_FOLLOW
		};

		return checks[index];
	}

	int16_t UIOptionMenu::phone_slider_y(size_t index) const
	{
		return PHONE_ROWS_Y + static_cast<int16_t>(index) * PHONE_ROW_STEP + 12;
	}

	Rectangle<int16_t> UIOptionMenu::phone_tab_rect(size_t index) const
	{
		Point<int16_t> tl = position + Point<int16_t>(12 + static_cast<int16_t>(index) * 126, 36);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(120, 32));
	}

	Rectangle<int16_t> UIOptionMenu::phone_mute_rect(size_t index) const
	{
		Point<int16_t> tl = position + Point<int16_t>(PHONE_W - 100, phone_slider_y(index) - 17);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(84, 34));
	}

	Rectangle<int16_t> UIOptionMenu::phone_shake_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(24, PHONE_ROWS_Y + static_cast<int16_t>(PHONE_SLIDERS) * PHONE_ROW_STEP);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(150, 34));
	}

	Rectangle<int16_t> UIOptionMenu::phone_minimap_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(186, PHONE_ROWS_Y + static_cast<int16_t>(PHONE_SLIDERS) * PHONE_ROW_STEP);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(150, 34));
	}

	Rectangle<int16_t> UIOptionMenu::phone_social_rect(size_t index) const
	{
		constexpr int16_t W = 300;
		constexpr int16_t H = 42;
		constexpr int16_t STEP = 48;

		int16_t col = static_cast<int16_t>(index % 2);
		int16_t row = static_cast<int16_t>(index / 2);
		Point<int16_t> tl = position + Point<int16_t>(24 + col * (W + 12), 100 + row * STEP);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(W, H));
	}

	Rectangle<int16_t> UIOptionMenu::phone_ok_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(PHONE_W - 256, PHONE_H - 46);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(116, 36));
	}

	Rectangle<int16_t> UIOptionMenu::phone_cancel_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(PHONE_W - 132, PHONE_H - 46);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(116, 36));
	}

	void UIOptionMenu::draw_label(Rectangle<int16_t> r, const Text& label) const
	{
		label.draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(label, r.top() + r.height() / 2)));
	}

	void UIOptionMenu::draw_toggle(Rectangle<int16_t> r, const Text& label, bool on) const
	{
		classic::cell(r.get_left_top(), r.width(), r.height(), on);
		draw_label(r, label);
	}

	UIOptionMenu::SliderType UIOptionMenu::slider_at(Point<int16_t> cursorpos) const
	{
		constexpr int16_t REACH = 20;

		int16_t x = cursorpos.x() - position.x();
		int16_t y = cursorpos.y() - position.y();

		if (x < PHONE_TRACK_X - REACH || x > PHONE_TRACK_X + PHONE_TRACK_W + REACH)
			return SL_NONE;

		for (size_t i = 0; i < PHONE_SLIDERS; i++)
			if (std::abs(y - phone_slider_y(i)) <= REACH)
				return phone_slider(i);

		return SL_NONE;
	}

	void UIOptionMenu::set_slider(SliderType slider, int16_t cursor_x)
	{
		int16_t x = cursor_x - position.x() - PHONE_TRACK_X;

		if (x < 0)
			x = 0;

		if (x > PHONE_TRACK_W)
			x = PHONE_TRACK_W;

		slider_val[slider] = static_cast<uint8_t>(x * 100 / PHONE_TRACK_W);

		if (slider == SL_BGM && !check_val[CHK_MUTE_BGM])
			Music::set_bgmvolume(slider_val[SL_BGM]);
		else if (slider == SL_SFX && !check_val[CHK_MUTE_SFX])
			Sound::set_sfxvolume(slider_val[SL_SFX]);
	}

	void UIOptionMenu::toggle(uint16_t check)
	{
		check_val[check] = !check_val[check];

		if (check == CHK_MINIMAP_SIMPLE)
			check_val[CHK_MINIMAP_NORMAL] = !check_val[CHK_MINIMAP_SIMPLE];
		else if (check == CHK_MUTE_BGM)
			Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
		else if (check == CHK_MUTE_SFX)
			Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);
	}

	Cursor::State UIOptionMenu::phone_tap(Point<int16_t> at)
	{
		if (phone_ok_rect().contains(at))
		{
			button_pressed(BT_OK);
			return Cursor::State::IDLE;
		}

		if (phone_cancel_rect().contains(at))
		{
			button_pressed(BT_CANCEL);
			return Cursor::State::IDLE;
		}

		for (size_t i = 0; i < PHONE_TABS; i++)
		{
			if (phone_tab_rect(i).contains(at))
			{
				phone_tab = static_cast<uint8_t>(i);
				return Cursor::State::CLICKING;
			}
		}

		if (phone_tab == 1)
		{
			for (size_t i = 0; i < PHONE_SOCIAL; i++)
			{
				if (phone_social_rect(i).contains(at))
				{
					toggle(phone_social(i));
					return Cursor::State::CLICKING;
				}
			}

			return Cursor::State::IDLE;
		}

		for (size_t i = 0; i < PHONE_SLIDERS; i++)
		{
			SliderType slider = phone_slider(i);

			if ((slider == SL_BGM || slider == SL_SFX) && phone_mute_rect(i).contains(at))
			{
				toggle(slider == SL_BGM ? CHK_MUTE_BGM : CHK_MUTE_SFX);
				return Cursor::State::CLICKING;
			}
		}

		if (phone_shake_rect().contains(at))
			toggle(CHK_SCREEN_SHAKE);
		else if (phone_minimap_rect().contains(at))
			toggle(CHK_MINIMAP_SIMPLE);

		return Cursor::State::CLICKING;
	}
#endif

	void UIOptionMenu::revert_settings()
	{
		for (int i = 0; i < NUM_SLIDERS; i++)
			slider_val[i] = slider_orig[i];

		for (int i = 0; i < NUM_CHECKS; i++)
			check_val[i] = check_orig[i];

		Music::set_bgmvolume(check_val[CHK_MUTE_BGM] ? 0 : slider_val[SL_BGM]);
		Sound::set_sfxvolume(check_val[CHK_MUTE_SFX] ? 0 : slider_val[SL_SFX]);
	}

}
