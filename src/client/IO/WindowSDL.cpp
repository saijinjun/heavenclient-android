//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
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
#include "Window.h"
// SDL2 window, GL ES context and input for the desktop and Android builds.
#ifdef MS_PLATFORM_SDL

#include "KeyCodes.h"
#include "UI.h"

#include "../Console.h"
#include "../Constants.h"
#include "../Configuration.h"
#include "../Graphics/GraphicsGL.h"

#include <algorithm>

namespace jrc
{
    namespace
    {
        // Translates an SDL key into the GLFW key code the game expects.
        int32_t to_glfw_key(SDL_Keycode key)
        {
            if (key >= SDLK_a && key <= SDLK_z)
            {
                return GLFW_KEY_A + (key - SDLK_a);
            }

            if (key >= SDLK_0 && key <= SDLK_9)
            {
                return GLFW_KEY_0 + (key - SDLK_0);
            }

            if (key >= SDLK_F1 && key <= SDLK_F12)
            {
                return GLFW_KEY_F1 + (key - SDLK_F1);
            }

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
            case SDLK_SCROLLLOCK:   return GLFW_KEY_SCROLL_LOCK;
            case SDLK_KP_ENTER:     return GLFW_KEY_KP_ENTER;
            case SDLK_LSHIFT:       return GLFW_KEY_LEFT_SHIFT;
            case SDLK_LCTRL:        return GLFW_KEY_LEFT_CONTROL;
            case SDLK_LALT:         return GLFW_KEY_LEFT_ALT;
            case SDLK_RSHIFT:       return GLFW_KEY_RIGHT_SHIFT;
            case SDLK_RCTRL:        return GLFW_KEY_RIGHT_CONTROL;
            case SDLK_RALT:         return GLFW_KEY_RIGHT_ALT;
            default:                return GLFW_KEY_UNKNOWN;
            }
        }

        // Keys whose characters arrive as SDL_TEXTINPUT while typing.
        bool is_printable(SDL_Keycode key)
        {
            return key >= SDLK_SPACE && key <= SDLK_z;
        }

        struct TypedKey
        {
            int32_t key;
            bool shift;
        };

        // The key presses that type 'ch' on a US layout. Soft keyboards only
        // report characters, but the game reads text as key presses.
        TypedKey to_typed_key(char ch)
        {
            if (ch >= 'a' && ch <= 'z')
            {
                return { GLFW_KEY_A + (ch - 'a'), false };
            }

            if (ch >= 'A' && ch <= 'Z')
            {
                return { GLFW_KEY_A + (ch - 'A'), true };
            }

            if (ch >= '0' && ch <= '9')
            {
                return { GLFW_KEY_0 + (ch - '0'), false };
            }

            switch (ch)
            {
            case ' ':  return { GLFW_KEY_SPACE, false };
            case '-':  return { GLFW_KEY_MINUS, false };
            case '=':  return { GLFW_KEY_EQUAL, false };
            case '[':  return { GLFW_KEY_LEFT_BRACKET, false };
            case ']':  return { GLFW_KEY_RIGHT_BRACKET, false };
            case '\\': return { GLFW_KEY_BACKSLASH, false };
            case ';':  return { GLFW_KEY_SEMICOLON, false };
            case '\'': return { GLFW_KEY_APOSTROPHE, false };
            case ',':  return { GLFW_KEY_COMMA, false };
            case '.':  return { GLFW_KEY_PERIOD, false };
            case '/':  return { GLFW_KEY_SLASH, false };
            case '`':  return { GLFW_KEY_GRAVE_ACCENT, false };
            case '!':  return { GLFW_KEY_1, true };
            case '@':  return { GLFW_KEY_2, true };
            case '#':  return { GLFW_KEY_3, true };
            case '$':  return { GLFW_KEY_4, true };
            case '%':  return { GLFW_KEY_5, true };
            case '^':  return { GLFW_KEY_6, true };
            case '&':  return { GLFW_KEY_7, true };
            case '*':  return { GLFW_KEY_8, true };
            case '(':  return { GLFW_KEY_9, true };
            case ')':  return { GLFW_KEY_0, true };
            case '_':  return { GLFW_KEY_MINUS, true };
            case '+':  return { GLFW_KEY_EQUAL, true };
            case '{':  return { GLFW_KEY_LEFT_BRACKET, true };
            case '}':  return { GLFW_KEY_RIGHT_BRACKET, true };
            case '|':  return { GLFW_KEY_BACKSLASH, true };
            case ':':  return { GLFW_KEY_SEMICOLON, true };
            case '"':  return { GLFW_KEY_APOSTROPHE, true };
            case '<':  return { GLFW_KEY_COMMA, true };
            case '>':  return { GLFW_KEY_PERIOD, true };
            case '?':  return { GLFW_KEY_SLASH, true };
            case '~':  return { GLFW_KEY_GRAVE_ACCENT, true };
            default:   return { GLFW_KEY_UNKNOWN, false };
            }
        }

        void type_text(const char* text)
        {
            for (const char* ch = text; *ch != '\0'; ++ch)
            {
                TypedKey typed = to_typed_key(*ch);

                if (typed.key == GLFW_KEY_UNKNOWN)
                {
                    continue;
                }

                if (typed.shift)
                {
                    UI::get().send_key(GLFW_KEY_LEFT_SHIFT, true);
                }

                UI::get().send_key(typed.key, true);
                UI::get().send_key(typed.key, false);

                if (typed.shift)
                {
                    UI::get().send_key(GLFW_KEY_LEFT_SHIFT, false);
                }
            }
        }
    }

    Window::Window()
    {
        glwnd = nullptr;
        context = nullptr;
        closed = false;
        text_input = false;
        view_x = 0;
        view_y = 0;
        view_w = Constants::VIEWWIDTH;
        view_h = Constants::VIEWHEIGHT;
        opacity = 1.0f;
        opcstep = 0.0f;
        fullscreen = false;
        f11_down = false;
    }

    Window::~Window()
    {
        if (context)
        {
            SDL_GL_DeleteContext(context);
        }

        if (glwnd)
        {
            SDL_DestroyWindow(glwnd);
        }

        SDL_Quit();
    }

    Error Window::init()
    {
        fullscreen = Setting<Fullscreen>::get().load();

        SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
        // Taps arrive as left mouse clicks, which the game UI already handles.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
        {
            Console::get().print("SDL error: " + std::string(SDL_GetError()));
            return Error::GLFW;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#ifdef __ANDROID__
        flags |= SDL_WINDOW_FULLSCREEN;
#else
        if (fullscreen)
        {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
#endif

        glwnd = SDL_CreateWindow(
            "Journey",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            Constants::VIEWWIDTH,
            Constants::VIEWHEIGHT,
            flags
        );

        if (!glwnd)
        {
            Console::get().print("SDL error: " + std::string(SDL_GetError()));
            return Error::WINDOW;
        }

        context = SDL_GL_CreateContext(glwnd);

        if (!context)
        {
            Console::get().print("SDL error: " + std::string(SDL_GetError()));
            return Error::WINDOW;
        }

        SDL_GL_MakeCurrent(glwnd, context);

        // Desktop SDL starts with text input on; only text fields need it.
        SDL_StopTextInput();

        if (Error error = GraphicsGL::get().init())
        {
            return error;
        }

        return initwindow();
    }

    Error Window::initwindow()
    {
        bool vsync = Setting<VSync>::get().load();
        SDL_GL_SetSwapInterval(vsync ? 1 : 0);

        // The game draws at its classic fixed size and is scaled to the screen.
        Constants::set_viewsize(Constants::VIEWWIDTH, Constants::VIEWHEIGHT);
        update_viewport();

        GraphicsGL::get().reinit();
        GraphicsGL::get().set_screensize(Constants::viewwidth(), Constants::viewheight());

        SDL_ShowCursor(SDL_DISABLE);

        return Error::NONE;
    }

    void Window::update_viewport()
    {
        int32_t drawable_w = 0;
        int32_t drawable_h = 0;
        SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

        if (drawable_w <= 0 || drawable_h <= 0)
        {
            return;
        }

        // Largest view with the game's aspect ratio, centered with black bars.
        float scale = std::min(
            static_cast<float>(drawable_w) / Constants::viewwidth(),
            static_cast<float>(drawable_h) / Constants::viewheight()
        );

        view_w = static_cast<int32_t>(Constants::viewwidth() * scale);
        view_h = static_cast<int32_t>(Constants::viewheight() * scale);
        view_x = (drawable_w - view_w) / 2;
        view_y = (drawable_h - view_h) / 2;
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

        float vx = (px - view_x) * Constants::viewwidth() / std::max(view_w, 1);
        float vy = (py - view_y) * Constants::viewheight() / std::max(view_h, 1);

        return {
            static_cast<int16_t>(std::clamp(vx, 0.0f, static_cast<float>(Constants::viewwidth() - 1))),
            static_cast<int16_t>(std::clamp(vy, 0.0f, static_cast<float>(Constants::viewheight() - 1)))
        };
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

    void Window::update_text_input()
    {
        // Show the soft keyboard exactly while a text field has focus.
        bool wanted = UI::get().has_focused_textfield();

        if (wanted == text_input)
        {
            return;
        }

        text_input = wanted;

        if (text_input)
        {
            SDL_StartTextInput();
        }
        else
        {
            SDL_StopTextInput();
        }
    }

    void Window::handle_event(const SDL_Event& event)
    {
        switch (event.type)
        {
        case SDL_QUIT:
            UI::get().send_close();
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            SDL_Keycode key = event.key.keysym.sym;
            bool pressed = event.type == SDL_KEYDOWN;

            // While typing, characters come from SDL_TEXTINPUT instead.
            if (text_input && is_printable(key))
            {
                break;
            }

#ifndef __ANDROID__
            if (key == SDLK_F11)
            {
                if (pressed && !f11_down)
                {
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(glwnd, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }

                f11_down = pressed;
                break;
            }
#endif

            int32_t glfw_key = to_glfw_key(key);

            if (glfw_key != GLFW_KEY_UNKNOWN)
            {
                UI::get().send_key(glfw_key, pressed);
            }
            break;
        }
        case SDL_TEXTINPUT:
            type_text(event.text.text);
            break;
        case SDL_MOUSEMOTION:
            UI::get().send_cursor(to_view(event.motion.x, event.motion.y));
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            bool pressed = event.type == SDL_MOUSEBUTTONDOWN;

            // A tap has no hover beforehand, so move the cursor first.
            UI::get().send_cursor(to_view(event.button.x, event.button.y));

            if (event.button.button == SDL_BUTTON_LEFT)
            {
                UI::get().send_cursor(pressed);

                if (pressed && event.button.clicks >= 2)
                {
                    UI::get().cancel_drag();
                    UI::get().doubleclick();
                }
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
                update_viewport();
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }

    void Window::check_events()
    {
        update_text_input();

        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            handle_event(event);
        }
    }

    void Window::begin() const
    {
        GraphicsGL::get().clearscene();
    }

    void Window::end() const
    {
        int32_t drawable_w = 0;
        int32_t drawable_h = 0;
        SDL_GL_GetDrawableSize(glwnd, &drawable_w, &drawable_h);

        // Black bars around the view, then the scene clipped to the view.
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, drawable_w, drawable_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(view_x, view_y, view_w, view_h);
        glScissor(view_x, view_y, view_w, view_h);
        glEnable(GL_SCISSOR_TEST);

        GraphicsGL::get().flush(opacity);
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
}
#endif
