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

#include "../UIElement.h"
#include "../Components/TapGesture.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Text.h"

#include <cstdint>
#include <vector>

namespace ms
{
	// Reads the pages of a guide or diary item (416xxxx), such as a pet
	// command guide
	class UIBook : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::BOOK;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = true;

		UIBook(int32_t itemid);

		static bool has_book(int32_t itemid);

		void draw(float inter) const override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;

		UIElement::Type get_type() const override;

	private:
		static constexpr int16_t W = 480;
		static constexpr int16_t H = 300;
		static constexpr int16_t CLOSE_SIZE = 28;
		static constexpr int16_t BTN_W = 100;
		static constexpr int16_t BTN_H = 36;

		Rectangle<int16_t> close_rect() const;
		Rectangle<int16_t> prev_rect() const;
		Rectangle<int16_t> next_rect() const;
		void turn_to(int32_t index);

		std::vector<Text> pages;
		int32_t page = 0;

		Text title;
		Text page_count;
		mutable Text prev_label;
		mutable Text next_label;
		TapGesture gesture;
	};
}
