//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client
//	Copyright (C) 2015-2019  Daniel Allendorf, Ryan Payton
//
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Affero General Public License for more details.
//
//	You should have received a copy of the GNU Affero General Public License
//	along with this program.  If not, see <https://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////
//	SDL2 window, GL ES 3 context and input for the Android port (and its Linux
//	desktop twin). Replaces the GLFW-based src/IO/Window.cpp.
//////////////////////////////////////////////////////////////////////////////////
#include "IO/Window.h"

#include "IO/Gamepad.h"
#include "IO/UI.h"
#include "KeyCodes.h"
#include "TouchSprites.h"

#include "Character/Player.h"
#include "Data/ItemData.h"
#include "Data/SkillData.h"
#include "Gameplay/MapleMap/MapInfo.h"
#include "Gameplay/Stage.h"
#include "IO/Components/Icon.h"
#include "Net/Packets/PlayerPackets.h"

#include "Configuration.h"
#include "Constants.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace ms
{
	namespace
	{
		// A finger held this long without moving is a right click.
		constexpr uint32_t LONG_PRESS_MS = 550;
		// Two taps this close in time and space are a double click.
		constexpr uint32_t DOUBLE_TAP_MS = 400;
		constexpr int16_t TAP_SLOP = 24;

		// What a touch quick slot holds: the binding of the quickslot bar's
		// key in the same position.
		Keyboard::Mapping slot_mapping(uint8_t slot)
		{
			const auto& keys = UI::get().get_keyboard().get_quickslot_keys();
			if (slot >= keys.size())
				return {};

			return UI::get().get_keyboard().get_maple_mapping(keys[slot]);
		}

		const Texture* slot_icon(const Keyboard::Mapping& mapping)
		{
			if (mapping.action == 0)
				return nullptr;

			switch (mapping.type)
			{
			case KeyType::Id::SKILL:
				return &SkillData::get(mapping.action).get_icon(SkillData::Icon::NORMAL);
			case KeyType::Id::ITEM:
				return &ItemData::get(mapping.action).get_icon(false);
			default:
				return nullptr;
			}
		}

		// Puts a skill or item on a quick slot, here and on the server.
		void bind_slot(uint8_t slot, KeyType::Id type, int32_t action)
		{
			const auto& keys = UI::get().get_keyboard().get_quickslot_keys();
			if (slot >= keys.size())
				return;

			uint8_t key = keys[slot];
			UI::get().get_keyboard().assign(key, static_cast<uint8_t>(type), action);

			std::vector<std::tuple<KeyConfig::Key, KeyType::Id, int32_t>> updates;
			updates.emplace_back(static_cast<KeyConfig::Key>(key), type, action);
			ChangeKeyMapPacket(updates).dispatch();
		}

		// Translates an SDL key into the GLFW key code the game expects.
		int32_t to_glfw_key(SDL_Keycode key)
		{
			if (key >= SDLK_a && key <= SDLK_z)
				return GLFW_KEY_A + (key - SDLK_a);

			if (key >= SDLK_0 && key <= SDLK_9)
				return GLFW_KEY_0 + (key - SDLK_0);

			if (key >= SDLK_F1 && key <= SDLK_F12)
				return GLFW_KEY_F1 + (key - SDLK_F1);

			switch (key)
			{
			case SDLK_SPACE:        return GLFW_KEY_SPACE;
			case SDLK_QUOTE:        return GLFW_KEY_APOSTROPHE;
			case SDLK_COMMA:        return GLFW_KEY_COMMA;
			case SDLK_MINUS:        return GLFW_KEY_MINUS;
			case SDLK_PERIOD:       return GLFW_KEY_PERIOD;
			case SDLK_SLASH:        return GLFW_KEY_SLASH;
			case SDLK_SEMICOLON:    return GLFW_KEY_SEMICOLON;
			case SDLK_EQUALS:       return GLFW_KEY_EQUAL;
			case SDLK_LEFTBRACKET:  return GLFW_KEY_LEFT_BRACKET;
			case SDLK_BACKSLASH:    return GLFW_KEY_BACKSLASH;
			case SDLK_RIGHTBRACKET: return GLFW_KEY_RIGHT_BRACKET;
			case SDLK_BACKQUOTE:    return GLFW_KEY_GRAVE_ACCENT;
			case SDLK_ESCAPE:       return GLFW_KEY_ESCAPE;
			// Android's back button closes the top window, like Escape.
			case SDLK_AC_BACK:      return GLFW_KEY_ESCAPE;
			case SDLK_RETURN:       return GLFW_KEY_ENTER;
			case SDLK_TAB:          return GLFW_KEY_TAB;
			case SDLK_BACKSPACE:    return GLFW_KEY_BACKSPACE;
			case SDLK_INSERT:       return GLFW_KEY_INSERT;
			case SDLK_DELETE:       return GLFW_KEY_DELETE;
			case SDLK_RIGHT:        return GLFW_KEY_RIGHT;
			case SDLK_LEFT:         return GLFW_KEY_LEFT;
			case SDLK_DOWN:         return GLFW_KEY_DOWN;
			case SDLK_UP:           return GLFW_KEY_UP;
			case SDLK_PAGEUP:       return GLFW_KEY_PAGE_UP;
			case SDLK_PAGEDOWN:     return GLFW_KEY_PAGE_DOWN;
			case SDLK_HOME:         return GLFW_KEY_HOME;
			case SDLK_END:          return GLFW_KEY_END;
			case SDLK_CAPSLOCK:     return GLFW_KEY_CAPS_LOCK;
			case SDLK_SCROLLLOCK:   return GLFW_KEY_SCROLL_LOCK;
			case SDLK_KP_ENTER:     return GLFW_KEY_KP_ENTER;
			case SDLK_LSHIFT:       return GLFW_KEY_LEFT_SHIFT;
			case SDLK_LCTRL:        return GLFW_KEY_LEFT_CONTROL;
			case SDLK_LALT:         return GLFW_KEY_LEFT_ALT;
			case SDLK_LGUI:         return GLFW_KEY_LEFT_SUPER;
			case SDLK_RSHIFT:       return GLFW_KEY_RIGHT_SHIFT;
			case SDLK_RCTRL:        return GLFW_KEY_RIGHT_CONTROL;
			case SDLK_RALT:         return GLFW_KEY_RIGHT_ALT;
			case SDLK_RGUI:         return GLFW_KEY_RIGHT_SUPER;
			default:                return GLFW_KEY_UNKNOWN;
			}
		}

		// Keys whose characters arrive as SDL_TEXTINPUT while typing.
		bool is_printable(SDL_Keycode key)
		{
			return key >= SDLK_SPACE && key <= SDLK_z;
		}

		// Decodes UTF-8 text from SDL_TEXTINPUT into code points.
		void send_text(const char* text)
		{
			auto bytes = reinterpret_cast<const unsigned char*>(text);

			while (*bytes)
			{
				uint32_t cp = *bytes;
				int extra = 0;

				if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
				else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
				else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }

				bytes++;

				for (; extra > 0 && (*bytes & 0xC0) == 0x80; extra--, bytes++)
					cp = (cp << 6) | (*bytes & 0x3F);

				if (extra == 0)
					UI::get().send_char(cp);
			}
		}

		// The logical (game) view keeps the classic proportions: the UI scale
		// is chosen so the view is at least 1280 wide or 720 high, whichever
		// fits the screen, matching SetFieldHandler's formula.
		void apply_screen_size(int32_t drawable_w, int32_t drawable_h)
		{
			Constants::Constants::get().set_ui_scale(Constants::ui_scale_for(drawable_w, drawable_h));
			Constants::Constants::get().set_viewwidth(static_cast<int16_t>(drawable_w));
			Constants::Constants::get().set_viewheight(static_cast<int16_t>(drawable_h));

			// SetFieldHandler reads these on every map load.
			Setting<Width>::get().save(static_cast<int16_t>(drawable_w));
			Setting<Height>::get().save(static_cast<int16_t>(drawable_h));
		}
	}

	Window::Window()
	{
		glwnd = nullptr;
		context = nullptr;
		closed = false;
		text_input = false;
		f11_down = false;
		cursor_finger_down = false;
		cursor_finger = 0;
		finger_down_time = 0;
		long_press_sent = false;
		last_tap_time = 0;
		touch_seen = false;
		touch_editing = false;
		haptic = nullptr;
		fullscreen = false;
		opacity = 1.0f;
		opcstep = 0.0f;
		width = Constants::Constants::get().get_physicalwidth();
		height = Constants::Constants::get().get_physicalheight();
	}

	Window::~Window()
	{
		if (haptic)
			SDL_HapticClose(haptic);

		if (context)
			SDL_GL_DeleteContext(context);

		if (glwnd)
			SDL_DestroyWindow(glwnd);

		SDL_Quit();
	}

	Error Window::init()
	{
		fullscreen = Setting<Fullscreen>::get().load();

		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
		// Touches are read as finger events; SDL's emulated mouse would send
		// every touch twice.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
		{
			std::cerr << "[Window] SDL error: " << SDL_GetError() << std::endl;
			return Error::Code::GLFW;
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef PLATFORM_ANDROID
		flags |= SDL_WINDOW_FULLSCREEN;
#else
		if (fullscreen)
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif

		int16_t start_w = Setting<Width>::get().load();
		int16_t start_h = Setting<Height>::get().load();

		if (start_w <= 0 || start_h <= 0)
		{
			start_w = 1280;
			start_h = 720;
		}

		glwnd = SDL_CreateWindow(
			Configuration::get().get_title().c_str(),
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			start_w,
			start_h,
			flags
		);

		if (!glwnd)
		{
			std::cerr << "[Window] SDL error: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		context = SDL_GL_CreateContext(glwnd);

		if (!context)
		{
			std::cerr << "[Window] SDL error: " << SDL_GetError() << std::endl;
			return Error::Code::WINDOW;
		}

		SDL_GL_MakeCurrent(glwnd, context);

		// Desktop SDL starts with text input on; only text fields need it.
		SDL_StopTextInput();

		int32_t drawable_w = 0;
		int32_t drawable_h = 0;
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);
		apply_screen_size(drawable_w, drawable_h);

		width = Constants::Constants::get().get_physicalwidth();
		height = Constants::Constants::get().get_physicalheight();

		if (Error error = GraphicsGL::get().init())
			return error;

		return initwindow();
	}

	Error Window::initwindow()
	{
		GraphicsGL::get().bind_context_vao();

		bool vsync = Setting<VSync>::get().load();
		SDL_GL_SetSwapInterval(vsync ? 1 : 0);

		touch.set_layout(Setting<TouchLayout>::get().load());

		if (!haptic && Setting<TouchVibration>::get().load() &&
			SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0 && SDL_NumHaptics() > 0)
		{
			haptic = SDL_HapticOpen(0);

			if (haptic && SDL_HapticRumbleInit(haptic) != 0)
			{
				SDL_HapticClose(haptic);
				haptic = nullptr;
			}
		}

		update_viewport();
		SDL_ShowCursor(SDL_DISABLE);

		GraphicsGL::get().reinit();

		return Error::Code::NONE;
	}

	void Window::update_viewport()
	{
		int32_t drawable_w = 0;
		int32_t drawable_h = 0;
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

		if (drawable_w <= 0 || drawable_h <= 0)
			return;

		int32_t phys_w = Constants::Constants::get().get_physicalwidth();
		int32_t phys_h = Constants::Constants::get().get_physicalheight();

		view_x = 0;
		view_y = 0;
		view_w = drawable_w;
		view_h = drawable_h;

		if (phys_w > 0 && phys_h > 0 && phys_w * drawable_h != phys_h * drawable_w)
		{
			float scale = std::min(static_cast<float>(drawable_w) / phys_w, static_cast<float>(drawable_h) / phys_h);
			view_w = static_cast<int32_t>(phys_w * scale);
			view_h = static_cast<int32_t>(phys_h * scale);
			view_x = (drawable_w - view_w) / 2;
			view_y = (drawable_h - view_h) / 2;
		}

		glViewport(view_x, view_y, view_w, view_h);
		touch.set_screen(drawable_w, drawable_h);
	}

	Point<int16_t> Window::to_view(int32_t x, int32_t y) const
	{
		// Mouse and touch positions are in window units, which differ from
		// drawable pixels on high-density screens.
		int32_t window_w = 1;
		int32_t window_h = 1;
		int32_t drawable_w = 1;
		int32_t drawable_h = 1;
		SDL_GetWindowSize(glwnd, &window_w, &window_h);
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

		float px = static_cast<float>(x) * drawable_w / std::max(window_w, 1);
		float py = static_cast<float>(y) * drawable_h / std::max(window_h, 1);

		int32_t vw = Constants::Constants::get().get_viewwidth();
		int32_t vh = Constants::Constants::get().get_viewheight();

		float vx = (px - view_x) * vw / std::max(view_w, 1);
		float vy = (py - view_y) * vh / std::max(view_h, 1);

		return {
			static_cast<int16_t>(std::clamp(vx, 0.0f, static_cast<float>(vw - 1))),
			static_cast<int16_t>(std::clamp(vy, 0.0f, static_cast<float>(vh - 1)))
		};
	}

	void Window::handle_finger(const SDL_TouchFingerEvent& finger, uint32_t type)
	{
		int32_t window_w = 1;
		int32_t window_h = 1;
		SDL_GetWindowSize(glwnd, &window_w, &window_h);

		Point<int16_t> pos = to_view(
			static_cast<int32_t>(finger.x * window_w),
			static_cast<int32_t>(finger.y * window_h)
		);

		if (!touch_seen && Setting<RopeAssist>::get().load())
		{
			// About a character's width instead of the classic 10 pixels.
			Ladder::set_reach(24);
		}

		touch_seen = true;

		int32_t drawable_w = 0;
		int32_t drawable_h = 0;
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

		float px = finger.x * drawable_w;
		float py = finger.y * drawable_h;
		std::vector<TouchControls::Action> actions;

		// Controls only exist on the game screen; on login and in the cash
		// shop every touch is the cursor.
		bool controls = UI::get().in_game();

		switch (type)
		{
		case SDL_FINGERDOWN:
		{
			// A skill or item picked up from its window goes on the touch
			// quick slot it is put down on.
			uint8_t slot = 0;
			Icon* dragged = UI::get().get_dragged_icon();

			if (controls && dragged && !touch.is_editing() && touch.slot_at(px, py, slot))
			{
				Icon::IconType icontype = dragged->get_type();
				int32_t action = dragged->get_action_id();

				if (action != 0 && (icontype == Icon::IconType::SKILL || icontype == Icon::IconType::ITEM))
				{
					bind_slot(slot, icontype == Icon::IconType::SKILL ? KeyType::Id::SKILL : KeyType::Id::ITEM, action);
					UI::get().cancel_drag();
					break;
				}
			}

			if (controls && touch.finger_down(finger.fingerId, px, py, actions))
				break;

			if (cursor_finger_down)
				break;

			cursor_finger_down = true;
			cursor_finger = finger.fingerId;
			finger_down_time = SDL_GetTicks();
			finger_down_pos = pos;
			long_press_sent = false;

			// A tap has no hover beforehand, so move the cursor first.
			UI::get().send_cursor(pos);
			UI::get().send_cursor(true);

			uint32_t now = finger_down_time;
			Point<int16_t> moved = pos - last_tap_pos;
			bool near = std::abs(moved.x()) < TAP_SLOP && std::abs(moved.y()) < TAP_SLOP;

			if (last_tap_time != 0 && now - last_tap_time < DOUBLE_TAP_MS && near)
			{
				UI::get().doubleclick();
				last_tap_time = 0;
			}
			else
			{
				last_tap_time = now;
				last_tap_pos = pos;
			}

			break;
		}
		case SDL_FINGERMOTION:
			if (touch.finger_move(finger.fingerId, px, py, actions))
				break;

			if (cursor_finger_down && finger.fingerId == cursor_finger)
				UI::get().send_cursor(pos);

			break;
		case SDL_FINGERUP:
			if (touch.finger_up(finger.fingerId, actions))
				break;

			if (cursor_finger_down && finger.fingerId == cursor_finger)
			{
				UI::get().send_cursor(pos);
				UI::get().send_cursor(false);
				cursor_finger_down = false;
			}

			break;
		default:
			break;
		}

		send_touch_actions(actions);

		if (touch.take_layout_changed())
		{
			Setting<TouchLayout>::get().save(touch.get_layout());

			// Written to disk when the editor closes, not on every drag.
			if (!touch.is_editing())
				Configuration::get().save();
		}
	}

	void Window::send_touch_actions(const std::vector<TouchControls::Action>& actions)
	{
		for (const TouchControls::Action& action : actions)
		{
			// A short buzz for each button press; walking does not buzz.
			bool walking = action.output == TouchControls::Output::ACTION &&
				(action.id == KeyAction::Id::LEFT || action.id == KeyAction::Id::RIGHT ||
				 action.id == KeyAction::Id::UP || action.id == KeyAction::Id::DOWN);

			if (action.pressed && !walking && haptic)
				SDL_HapticRumblePlay(haptic, 0.35f, 12);

			switch (action.output)
			{
			case TouchControls::Output::ACTION:
				UI::get().send_action(KeyType::Id::ACTION, action.id, action.pressed);
				break;
			case TouchControls::Output::MENU:
				UI::get().send_action(KeyType::Id::MENU, action.id, action.pressed);
				break;
			case TouchControls::Output::SLOT:
			{
				// Whatever the player put on the slot: a skill, a potion.
				Keyboard::Mapping mapping = slot_mapping(static_cast<uint8_t>(action.id));

				if (mapping.type != KeyType::Id::NONE && mapping.action != 0)
					UI::get().send_action(mapping.type, mapping.action, action.pressed);

				break;
			}
			}
		}
	}

	void Window::draw_touch_controls() const
	{
		int32_t drawable_w = 0;
		int32_t drawable_h = 0;
		SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

		GraphicsGL& graphics = GraphicsGL::get();

		if (!graphics.has_overlay_texture())
			graphics.set_overlay_texture(TouchSprites::render(), TouchSprites::WIDTH, TouchSprites::HEIGHT);

		touch.set_time(SDL_GetTicks());

		std::array<const Texture*, TouchControls::NUM_SLOTS> icons = {};
		const Player& player = Stage::get().get_player();

		for (uint8_t slot = 0; slot < TouchControls::NUM_SLOTS; slot++)
		{
			Keyboard::Mapping mapping = slot_mapping(slot);
			icons[slot] = slot_icon(mapping);
			touch.set_slot_filled(slot, icons[slot] && icons[slot]->is_valid());

			// Potions show how many are left; skills dim while cooling down.
			int32_t count = mapping.type == KeyType::Id::ITEM ? player.get_inventory().get_total_item_count(mapping.action) : -1;
			bool cooling = mapping.type == KeyType::Id::SKILL && player.has_cooldown(mapping.action);
			touch.set_slot_info(slot, count, cooling);
		}

		for (const TouchControls::Draw& draw : touch.draws())
		{
			auto left = static_cast<int16_t>(draw.x - draw.half);
			auto top = static_cast<int16_t>(draw.y - draw.half);
			auto right = static_cast<int16_t>(draw.x + draw.half);
			auto bottom = static_cast<int16_t>(draw.y + draw.half);
			Color color(draw.red, draw.green, draw.blue, draw.alpha);

			if (draw.kind == TouchControls::Draw::SPRITE)
			{
				TouchSprites::Cell cell = TouchSprites::cell(static_cast<TouchSprites::Sprite>(draw.sprite));
				graphics.draw_screen_overlay(left, top, right, bottom, cell.left, cell.top, cell.right, cell.bottom, color);
			}
			else if (const Texture* icon = icons[draw.slot])
			{
				icon->draw_screen(left, top, right, bottom, color);
			}
		}

		glViewport(0, 0, drawable_w, drawable_h);
		graphics.flush_screen(static_cast<int16_t>(drawable_w), static_cast<int16_t>(drawable_h));
		glViewport(view_x, view_y, view_w, view_h);
	}

	void Window::on_app_event(uint32_t type)
	{
		switch (type)
		{
		case SDL_APP_WILLENTERBACKGROUND:
			// Let go of anything held so the character doesn't keep walking,
			// and save settings: Android may end the app without warning.
			UI::get().release_held_keys();
			{
				std::vector<TouchControls::Action> released;
				touch.release_all(released);
				send_touch_actions(released);
			}
			UI::get().send_focus(0);
			Configuration::get().save();
			break;
		case SDL_APP_DIDENTERFOREGROUND:
			UI::get().send_focus(1);
			break;
		default:
			break;
		}
	}

	void Window::update_text_input()
	{
		// Show the soft keyboard exactly while a text field has focus.
		bool wanted = UI::get().has_focused_textfield();

		if (wanted == text_input)
			return;

		text_input = wanted;

		if (text_input)
			SDL_StartTextInput();
		else
			SDL_StopTextInput();
	}

	void Window::handle_event(const SDL_Event& event)
	{
		switch (event.type)
		{
		case SDL_QUIT:
			UI::get().send_close();
			break;
		case SDL_APP_WILLENTERBACKGROUND:
		case SDL_APP_DIDENTERFOREGROUND:
			on_app_event(event.type);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		{
			SDL_Keycode key = event.key.keysym.sym;
			bool pressed = event.type == SDL_KEYDOWN;

			// While typing, characters come from SDL_TEXTINPUT instead.
			if (text_input && is_printable(key))
				break;

			if (pressed && event.key.repeat && (key == SDLK_ESCAPE || key == SDLK_RETURN))
				break;

#ifndef PLATFORM_ANDROID
			if (key == SDLK_F11)
			{
				if (pressed && !f11_down)
					toggle_fullscreen();

				f11_down = pressed;
				break;
			}

			if (key == SDLK_F12 && pressed && !event.key.repeat)
			{
				take_screenshot();
				break;
			}
#endif

			int32_t glfw_key = to_glfw_key(key);

			if (glfw_key != GLFW_KEY_UNKNOWN)
				UI::get().send_key(glfw_key, pressed);

			break;
		}
		case SDL_TEXTINPUT:
			send_text(event.text.text);
			break;
		case SDL_FINGERDOWN:
		case SDL_FINGERMOTION:
		case SDL_FINGERUP:
			handle_finger(event.tfinger, event.type);
			break;
		case SDL_MOUSEMOTION:
			if (event.motion.which != SDL_TOUCH_MOUSEID)
				UI::get().send_cursor(to_view(event.motion.x, event.motion.y));

			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		{
			if (event.button.which == SDL_TOUCH_MOUSEID)
				break;

			bool pressed = event.type == SDL_MOUSEBUTTONDOWN;

			UI::get().send_cursor(to_view(event.button.x, event.button.y));

			if (event.button.button == SDL_BUTTON_LEFT)
			{
				UI::get().send_cursor(pressed);

				if (pressed && event.button.clicks == 2)
					UI::get().doubleclick();
			}
			else if (event.button.button == SDL_BUTTON_RIGHT && pressed)
			{
				UI::get().rightclick();
			}

			break;
		}
		case SDL_MOUSEWHEEL:
			UI::get().send_scroll(event.wheel.preciseY);
			break;
		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
			Gamepad::get().poll();
			break;
		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				UI::get().send_focus(1);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				UI::get().send_focus(0);
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			{
				int32_t drawable_w = 0;
				int32_t drawable_h = 0;
				SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

				if (drawable_w > 0 && drawable_h > 0)
				{
					apply_screen_size(drawable_w, drawable_h);
					width = Constants::Constants::get().get_physicalwidth();
					height = Constants::Constants::get().get_physicalheight();
					update_viewport();
					GraphicsGL::get().reinit();
				}

				break;
			}
			default:
				break;
			}

			break;
		default:
			break;
		}
	}

	bool Window::not_closed() const
	{
		return !closed;
	}

	void Window::update()
	{
		updateopc();
	}

	void Window::updateopc()
	{
		if (opcstep != 0.0f)
		{
			opacity += opcstep;

			if (opacity >= 1.0f)
			{
				opacity = 1.0f;
				opcstep = 0.0f;
			}
			else if (opacity <= 0.0f)
			{
				opacity = 0.0f;
				opcstep = -opcstep;

				fadeprocedure();
			}
		}
	}

	void Window::check_events()
	{
		update_text_input();

		// Leaving the game screen lets go of any held stick or button.
		if (!UI::get().in_game())
		{
			std::vector<TouchControls::Action> released;
			touch.release_all(released);
			send_touch_actions(released);
		}

		// Something else (a map load, the cash shop) changed the view size.
		int16_t new_width = Constants::Constants::get().get_physicalwidth();
		int16_t new_height = Constants::Constants::get().get_physicalheight();

		if (width != new_width || height != new_height)
		{
			width = new_width;
			height = new_height;
			update_viewport();
		}

		SDL_Event event;

		while (SDL_PollEvent(&event))
			handle_event(event);

		// A finger held still is a right click (e.g. to open a menu).
		if (cursor_finger_down && !long_press_sent && SDL_GetTicks() - finger_down_time >= LONG_PRESS_MS)
		{
			Point<int16_t> moved = UI::get().get_cursor_position() - finger_down_pos;

			if (std::abs(moved.x()) < TAP_SLOP && std::abs(moved.y()) < TAP_SLOP)
				UI::get().rightclick();

			long_press_sent = true;
		}

		Gamepad::get().poll();
	}

	void Window::begin() const
	{
		// The HD-2D effects follow the options menu's Graphics quality
		// slider: below a third none, below two thirds only the color grade.
		uint8_t quality = Setting<GraphicsQuality>::get().load();
		PostProcess::Options effects;
		effects.glow = quality >= 67 && Setting<HdGlow>::get().load();
		effects.depth = quality >= 67 && Setting<HdDepthBlur>::get().load();
		effects.grade = quality >= 34 && Setting<HdColorGrade>::get().load();
		GraphicsGL::get().set_effects(effects);

		GraphicsGL::get().clearscene();
	}

	void Window::end() const
	{
		GraphicsGL::get().flush(opacity);

		if (touch_seen && UI::get().in_game())
			draw_touch_controls();

		SDL_GL_SwapWindow(glwnd);
	}

	void Window::fadeout(float step, std::function<void()> fadeproc)
	{
		opcstep = -step;
		fadeprocedure = std::move(fadeproc);
	}

	void Window::setclipboard(const std::string& text) const
	{
		SDL_SetClipboardText(text.c_str());
	}

	std::string Window::getclipboard() const
	{
		char* text = SDL_GetClipboardText();
		std::string result = text ? text : "";
		SDL_free(text);

		return result;
	}

	void Window::toggle_fullscreen()
	{
#ifndef PLATFORM_ANDROID
		fullscreen = !fullscreen;
		Setting<Fullscreen>::get().save(fullscreen);
		SDL_SetWindowFullscreen(glwnd, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#endif
	}

	void Window::apply_mouse_speed(int)
	{
		// The system pointer speed is not the game's to change here.
	}

	void Window::take_screenshot()
	{
		int32_t fb_w = 0;
		int32_t fb_h = 0;
		SDL_GL_GetDrawableSize(glwnd, &fb_w, &fb_h);

		if (fb_w <= 0 || fb_h <= 0)
			return;

		std::vector<uint8_t> pixels(static_cast<size_t>(fb_w) * fb_h * 4);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		// GL's origin is bottom-left; PNG rows go top-down.
		std::vector<uint8_t> flipped(pixels.size());
		size_t row_bytes = static_cast<size_t>(fb_w) * 4;

		for (int32_t y = 0; y < fb_h; ++y)
			std::memcpy(flipped.data() + (fb_h - 1 - y) * row_bytes, pixels.data() + y * row_bytes, row_bytes);

		std::string folder = Setting<ScreenshotFolder>::get().load();

		if (folder.empty())
			folder = "screenshots";

		std::error_code ec;
		std::filesystem::create_directories(folder, ec);

		std::time_t t = std::time(nullptr);
		std::tm tm_local{};

		if (localtime_r(&t, &tm_local) == nullptr)
			return;

		std::ostringstream name;
		name << "maple_" << std::put_time(&tm_local, "%Y%m%d_%H%M%S") << ".png";

		std::filesystem::path out = std::filesystem::path(folder) / name.str();
		stbi_write_png(out.string().c_str(), fb_w, fb_h, 4, flipped.data(), static_cast<int>(row_bytes));
	}
}
