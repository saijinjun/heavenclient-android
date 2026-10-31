//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

// Startup pieces of the SDL port that main() in MapleStory.cpp needs.
// <SDL.h> renames main() to SDL_main, which Android's activity calls.
#include <SDL.h>

#include <string>

namespace ms
{
	namespace sdl
	{
		// Moves into the folder holding the .nx files and the Settings file.
		// Android uses the app's external files folder, reachable over USB at
		// Android/data/<package>/files. Desktop builds take the folder as the
		// first argument and otherwise stay in the current directory.
		std::string prepare_data_directory(int argc, char* argv[]);

		// Shows a startup error in a dialog, with where the files go.
		void show_error(const std::string& message);

		// Asks whether to try connecting again. True for Reconnect, false
		// for Quit. Blocks until the player answers.
		bool ask_reconnect(const std::string& message);
	}
}
