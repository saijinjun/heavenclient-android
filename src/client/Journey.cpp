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
#include "Configuration.h"
#ifdef MS_PLATFORM_WASM
#include <emscripten.h>
#endif
#include "Console.h"
#include "Constants.h"
#include "Error.h"
#include "Timer.h"

#include "Audio/Audio.h"
#include "Character/Char.h"
#include "Gameplay/Combat/DamageNumber.h"
#include "Gameplay/Stage.h"
#include "Gameplay/CashShop.h"
#include "Graphics/GraphicsGL.h"
#include "IO/UI.h"
#include "IO/Window.h"
#include "Net/Session.h"
#include "Util/NxFiles.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef MS_PLATFORM_SDL
#include <SDL.h>

#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#endif


namespace jrc
{
    Error init()
    {
#ifdef MS_PLATFORM_WASM
        auto loadConfigString = [](const char* key, auto& setting) {
            char* val = (char*)EM_ASM_INT({
                var k = UTF8ToString($0);
                if (typeof Module !== 'undefined' && Module.LazyFS && Module.LazyFS[k] !== undefined && Module.LazyFS[k] !== null) {
                    var str = Module.LazyFS[k].toString();
                    var lengthBytes = lengthBytesUTF8(str) + 1;
                    var stringOnWasmHeap = _malloc(lengthBytes);
                    stringToUTF8(str, stringOnWasmHeap, lengthBytes);
                    return stringOnWasmHeap;
                }
                return 0;
            }, key);
            
            if (val) {
                setting.save(std::string(val));
                free(val);
            }
        };

        loadConfigString("MapleStoryServerIp", Setting<MapleStoryServerIp>::get());
        loadConfigString("MapleStoryServerPort", Setting<MapleStoryServerPort>::get());
        loadConfigString("ProxyIP", Setting<ProxyIP>::get());
        loadConfigString("ProxyPort", Setting<ProxyPort>::get());
        loadConfigString("AssetsServerProtocol", Setting<AssetsServerProtocol>::get());
#endif

        if (Error error = Session::get().init())
        {
            return error;
        }

        if (Error error = NxFiles::init())
        {
            return error;
        }

        if (Error error = Window::get().init())
        {
            return error;
        }

        if (Error error = Sound::init())
        {
            return error;
        }

        if (Error error = Music::init())
        {
            return error;
        }

        Char::init();
        DamageNumber::init();
        MapPortals::init();
        Stage::get().init();
        UI::get().init();

        return Error::NONE;
    }

    void update()
    {
        Window::get().check_events();
        Window::get().update();
        CashShop::get().update();
        if (!CashShop::get().active())
            Stage::get().update();
        UI::get().update();
        Session::get().read();
    }

    void draw(float alpha)
    {
        Window::get().begin();
        if (CashShop::get().state != CashShop::State::OPEN && CashShop::get().state != CashShop::State::EXITING)
            Stage::get().draw(alpha);
        GraphicsGL::get().mark_ui();
        UI::get().draw(alpha);
        Window::get().end();
    }

    bool running()
    {
        return Session::get().is_connected()
            && UI::get().not_quitted()
            && Window::get().not_closed();
    }

#ifdef MS_PLATFORM_WASM
    static int64_t timestep = Constants::TIMESTEP * 1000;
    static int64_t accumulator = timestep;
    static int64_t period = 0;
    static int32_t samples = 0;

    void main_tick()
    {
        if (!running())
        {
            emscripten_cancel_main_loop();
            Sound::close();
            return;
        }

        int64_t elapsed = Timer::get().stop();

        for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep)
            update();

        float alpha = static_cast<float>(accumulator) / timestep;
        draw(alpha);

        bool show_fps = true; // Hardcoded or from config
        if (show_fps)
        {
            if (samples < 100)
            {
                period += elapsed;
                samples++;
            }
            else if (period)
            {
                period = 0;
                samples = 0;
            }
        }
    }
#endif

    void loop()
    {
        Timer::get().start();

#ifdef MS_PLATFORM_WASM
        accumulator = timestep;
        emscripten_set_main_loop(main_tick, 0, 1);
#else
        int64_t timestep    = Constants::TIMESTEP * 1000;
        int64_t accumulator = timestep;

        int64_t period  = 0;
        int32_t samples = 0;

        // Frames are spaced out to at most MaxFps per second, which saves
        // battery on phones whose screens refresh at 90 or 120 Hz.
        using clock = std::chrono::steady_clock;
        uint8_t max_fps = Setting<MaxFps>::get().load();
        auto frame_time = max_fps > 0 ? std::chrono::microseconds(1000000 / max_fps) : std::chrono::microseconds(0);
        auto next_frame = clock::now();

        while (running())
        {
            if (max_fps > 0)
            {
                std::this_thread::sleep_until(next_frame);
                // Skip frames that are already late rather than racing to catch up.
                next_frame = std::max(next_frame + frame_time, clock::now());
            }

            int64_t elapsed = Timer::get().stop();

            // Update game with constant timestep as many times as possible.
            for (accumulator += elapsed; accumulator >= timestep; accumulator -= timestep)
            {
                update();
            }

            // Draw the game. Interpolate to account for remaining time.
            float alpha = static_cast<float>(accumulator) / timestep;
            draw(alpha);

            if (samples < 100)
            {
                period += elapsed;
                samples++;
            }
            else if (period)
            {
                //int64_t fps = (samples * 1000000) / period;
                //std::cout << "FPS: " << fps << std::endl;

                period  = 0;
                samples = 0;
            }
        }

        Sound::close();
#endif
    }

#ifdef MS_PLATFORM_SDL
    // Copies a file bundled with the app into the working directory, unless a
    // copy is already there. Android bundles files as APK assets; desktop
    // builds keep them next to the executable.
    void extract_bundled_file(const std::string& path)
    {
        if (std::ifstream{ path }.good())
        {
            return;
        }

#ifdef __ANDROID__
        std::string bundled = path;
#else
        char* base = SDL_GetBasePath();
        std::string bundled = (base ? base : "") + path;
        SDL_free(base);
#endif

        SDL_RWops* source = SDL_RWFromFile(bundled.c_str(), "rb");

        if (!source)
        {
            return;
        }

        size_t size = 0;
        void* data = SDL_LoadFile_RW(source, &size, 1);

        if (!data)
        {
            return;
        }

        // Create the parent folders, e.g. fonts and fonts/Arial.
        for (size_t slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1))
        {
            ::mkdir(path.substr(0, slash).c_str(), 0755);
        }

        std::ofstream out(path, std::ios::binary);
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        SDL_free(data);
    }

    // Moves into the folder holding the NX files and the Settings file.
    // Android uses the app's external files folder, which is reachable over
    // USB at Android/data/<package>/files. Desktop builds take the folder as
    // the first argument and otherwise stay in the current directory.
    std::string prepare_data_directory(int argc, char* argv[])
    {
        std::string directory;

#ifdef __ANDROID__
        const char* external = SDL_AndroidGetExternalStoragePath();
        directory = external ? external : "";
#else
        if (argc > 1)
        {
            directory = argv[1];
        }
#endif

        if (!directory.empty() && ::chdir(directory.c_str()) != 0)
        {
            Console::get().print("Could not open data folder " + directory);
        }

        char buffer[4096];
        directory = ::getcwd(buffer, sizeof(buffer)) ? buffer : directory;

        extract_bundled_file(FontPathNormal::DEFAULT);
        extract_bundled_file(FontPathBold::DEFAULT);

        // Write the Settings file now so it exists to edit before the first
        // successful start. Android often ends the app without running the
        // destructor that would otherwise save it.
        Configuration::get().save();

        return directory;
    }

    void show_error(const Error& error, const std::string& directory)
    {
        std::string message = std::string(error.get_message()) + error.get_args();
        message += "\n\nGame files and the Settings file go in:\n" + directory;
        message += "\n\nThe server address is MapleStoryServerIp in Settings.";

        std::cout << "Error: " << message << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Journey", message.c_str(), nullptr);
    }

    void start(int argc, char* argv[])
    {
        std::string directory = prepare_data_directory(argc, argv);

        if (Error error = init())
        {
            show_error(error, directory);
        }
        else
        {
            loop();
        }

        Configuration::get().save();
    }
#else
    void start()
    {
        // Initialize and check for errors.
        if (Error error = init())
        {
            const char* message   = error.get_message();
            const char* args      = error.get_args();
            const bool  can_retry = error.can_retry();

            std::cout << "Error: " << message << args << std::endl;

            std::string command;
            std::cin >> command;

            if (can_retry && command == "retry")
            {
                start();
            }
        }
        else
        {
            loop();
        }
    }
#endif
}

#ifdef MS_PLATFORM_SDL
// SDL renames main so that Android can call it from its Java activity.
int main(int argc, char* argv[])
{
    jrc::start(argc, argv);
    return 0;
}
#else
int main()
{
    jrc::start();
    return 0;
}
#endif
