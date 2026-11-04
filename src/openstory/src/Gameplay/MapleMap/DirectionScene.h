//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../../Graphics/Animation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ms
{
	// A story cutscene from Effect.wz/Direction*.img, started by the server's
	// "show intro" effect: the Aran and Cygnus tutorial scenes, the explorer
	// job clips and the boat to Lith Harbor. Each scene is a list of timed
	// steps: pictures (type 0), sounds or music (type 5), and the map to go
	// to when it ends (type 2). The game client is the one that asks for that
	// map, so a client that ignored the scene left you on a locked, empty map.
	// Avatar costume and pose steps (types 3, 4, 6) are not drawn.
	class DirectionScene
	{
	public:
		void play(const std::string& path);
		void update();
		void draw(float alpha) const;
		void clear();
		bool active() const;

	private:
		struct Picture
		{
			Animation animation;
			int16_t x;
			int16_t y;
			int32_t z;
			int32_t start;
			bool done;
		};

		struct Cue
		{
			enum class Kind { SOUND, MUSIC, FIELD } kind;
			int32_t start;
			std::string path;
			int32_t field;
			bool fired;
		};

		void fire(Cue& cue) const;

		std::vector<Picture> pictures;
		std::vector<Cue> cues;
		int32_t elapsed = 0;
	};
}
