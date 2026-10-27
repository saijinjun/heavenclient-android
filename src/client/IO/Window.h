//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../Error.h"
#include "../Template/Singleton.h"
#include "../Template/Point.h"

#include "../Graphics/GLHeaders.h"

#ifdef MS_PLATFORM_SDL
#include <SDL.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <string>
#include <functional>

namespace jrc
{
    class Window : public Singleton<Window>
    {
    public:
        Window();
        ~Window() override;

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

    private:
        void updateopc();

#ifdef MS_PLATFORM_SDL
        void handle_event(const SDL_Event& event);
        void update_viewport();
        void update_text_input();
        Point<int16_t> to_view(int32_t x, int32_t y) const;

        SDL_Window* glwnd;
        SDL_GLContext context;
        bool closed;
        bool text_input;
        // Where the fixed-size game view is drawn inside the window, in pixels.
        int32_t view_x;
        int32_t view_y;
        int32_t view_w;
        int32_t view_h;
#else
        GLFWwindow* glwnd;
        GLFWwindow* context;
#endif
        bool fullscreen;
        bool f11_down;
        float opacity;
        float opcstep;
        std::function<void()> fadeprocedure;
    };
}
