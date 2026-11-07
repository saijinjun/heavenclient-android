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

#include "../Graphics/Color.h"
#include "../Graphics/Text.h"
#include "../Graphics/Texture.h"

namespace ms
{
	// The name plate under a character: the name text and the optional
	// NameTag.img 9-slice plate.
	class NameTagStyle
	{
	public:
		NameTagStyle(const std::string& name);

		void draw(Point<int16_t> absp) const;

		// Give GMs the distinct NameTag.img plate; regular players keep the
		// plain default name.
		void apply(bool is_gm);
		// Use the plate NameTag.img/<style>, as a name tag ring sets it.
		// Keeps the current plate and returns false if the style has no art.
		bool apply_style(int32_t style);
		std::string get_name() const;

	private:
		// Loads the plate and name color from NameTag.img/<key>; false when
		// that style has no plate art
		bool load_style(const std::string& key);

		Text namelabel;
		Color name_color;
		// Nametag 9-slice sprite pieces loaded from NameTag.img/<style>/{w,c,e}.
		// w = left edge, c = tiled center, e = right edge.
		Texture tag_w;
		Texture tag_c;
		Texture tag_e;
	};
}
