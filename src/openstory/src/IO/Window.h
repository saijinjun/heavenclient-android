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

#include "../../platform/shared/PlatformConfig.h"
#include "../Error.h"

#include "../Template/Singleton.h"
#include "../Template/Point.h"

#if defined(PLATFORM_SDL)
	#include "../Graphics/GraphicsGL.h"
	#include "TouchControls.h"
	#include <SDL.h>
#elif defined(PLATFORM_IOS)
	#include <OpenGLES/ES3/gl.h>
#else
	#define GLEW_STATIC
	#include <glew.h>
	#include <glfw3.h>
#endif

#include <functional>
#include <string>

namespace ms
{
	class Window : public Singleton<Window>
	{
	public:
		Window();
		~Window();

		Error init();
		Error initwindow();

		bool not_closed() const;
		void update();
		void begin() const;
		void end() const;
		void fadeout(float step, std::function<void()> fadeprocedure);
		void check_events();

		void setclipboard(const std::string& text) const;
		std::string getclipboard() const;

		void toggle_fullscreen();

		// Applies the given slider value (0-100 range, as stored by
		// UIOptionMenu) to the system pointer speed via Windows'
		// SPI_SETMOUSESPEED. Windows accepts 1..20, where 10 is the default.
		// Pass -1 to apply the currently saved MouseSpeed setting.
		void apply_mouse_speed(int slider_value = -1);

		// Captures the current framebuffer contents and saves a timestamped
		// PNG under ./screenshots/. Called from the F12 key handler.
		void take_screenshot();

	private:
		void updateopc();

#if defined(PLATFORM_SDL)
	public:
		// Android lifecycle: the app went to the background or came back.
		void on_app_event(uint32_t type);

	private:
		void handle_event(const SDL_Event& event);
		void handle_finger(const SDL_TouchFingerEvent& finger, uint32_t type);
		void send_touch_actions(const std::vector<TouchControls::Action>& actions);
		void draw_touch_controls() const;
		void update_viewport();
		void update_text_input();
		Point<int16_t> to_view(int32_t x, int32_t y) const;

		SDL_Window* glwnd;
		SDL_GLContext context;
		bool closed;
		bool text_input;
		bool f11_down;
		// Touch-as-mouse: one finger drives the cursor.
		bool cursor_finger_down;
		SDL_FingerID cursor_finger;
		uint32_t finger_down_time;
		Point<int16_t> finger_down_pos;
		bool long_press_sent;
		uint32_t last_tap_time;
		Point<int16_t> last_tap_pos;
		// On-screen controls, shown on the game screen once a touch is seen.
		mutable TouchControls touch;
		bool touch_seen;
		bool touch_editing;
		SDL_Haptic* haptic;
#elif !defined(PLATFORM_IOS)
		GLFWwindow* glwnd;
		GLFWwindow* context;
#else
		bool closed;
#endif
		bool fullscreen;
		float opacity;
		float opcstep;
		std::function<void()> fadeprocedure;
		int16_t width;
		int16_t height;
	};
}