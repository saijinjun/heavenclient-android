//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "StartSDL.h"

#include <iostream>

#include <unistd.h>

namespace ms
{
	namespace sdl
	{
		namespace
		{
			std::string data_directory;
		}

		std::string prepare_data_directory(int argc, char* argv[])
		{
			std::string directory;

#ifdef __ANDROID__
			(void)argc;
			(void)argv;
			const char* external = SDL_AndroidGetExternalStoragePath();
			directory = external ? external : "";
#else
			if (argc > 1)
				directory = argv[1];
#endif

			if (!directory.empty() && ::chdir(directory.c_str()) != 0)
				std::cerr << "[Init] Could not open data folder " << directory << std::endl;

			char buffer[4096];
			data_directory = ::getcwd(buffer, sizeof(buffer)) ? buffer : directory;

			return data_directory;
		}

		void show_error(const std::string& message)
		{
			std::string text = message;
			text += "\n\nGame files and the Settings file go in:\n" + data_directory;
			text += "\n\nThe server address is ServerIP in Settings.";

			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenStory", text.c_str(), nullptr);
		}
	}
}
