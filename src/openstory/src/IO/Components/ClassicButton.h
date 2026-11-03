//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Button.h"

#include "../../Graphics/Text.h"

#include <string>

namespace ms
{
	// A labelled button in the navy+gold skin (classic::button), sized for a
	// finger. Dialogs on phones use it in place of the small NX buttons, so
	// every OK, Cancel, Yes and Next looks and feels the same.
	class ClassicButton : public Button
	{
	public:
		ClassicButton(const std::string& label, Point<int16_t> position, Point<int16_t> dimensions);

		void draw(Point<int16_t> parentpos) const override;
		void update() override {}
		Rectangle<int16_t> bounds(Point<int16_t> parentpos) const override;
		int16_t width() const override;
		Point<int16_t> origin() const override;
		Cursor::State send_cursor(bool, Point<int16_t>) override { return Cursor::State::IDLE; }

		void set_dimensions(Point<int16_t> dimensions);
		void set_label(const std::string& label);

	private:
		Point<int16_t> dimension;
		Text label;
	};
}
