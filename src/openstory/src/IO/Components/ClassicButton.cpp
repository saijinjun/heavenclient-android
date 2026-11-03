//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "ClassicButton.h"

#include "../../Graphics/ClassicStyle.h"

namespace ms
{
	ClassicButton::ClassicButton(const std::string& text, Point<int16_t> pos, Point<int16_t> dim) : dimension(dim)
	{
		position = pos;
		state = Button::State::NORMAL;
		active = true;
		label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, text);
	}

	void ClassicButton::draw(Point<int16_t> parentpos) const
	{
		if (!active)
			return;

		Point<int16_t> tl = position + parentpos;
		bool lit = state == Button::State::MOUSEOVER || state == Button::State::PRESSED || state == Button::State::KEYFOCUSED;
		bool disabled = state == Button::State::DISABLED;

		classic::button(tl, dimension.x(), dimension.y(), lit && !disabled);

		Point<int16_t> at(tl.x() + dimension.x() / 2, classic::text_y(label, tl.y() + dimension.y() / 2));
		classic::shadowed(label, at, disabled ? 0.45f : 1.0f);

		if (disabled)
			classic::box(tl.x(), tl.y(), dimension.x(), dimension.y(), Color::Name::BLACK, 0.35f);
	}

	Rectangle<int16_t> ClassicButton::bounds(Point<int16_t> parentpos) const
	{
		Point<int16_t> absp = position + parentpos;

		return Rectangle<int16_t>(absp, absp + dimension);
	}

	int16_t ClassicButton::width() const
	{
		return dimension.x();
	}

	Point<int16_t> ClassicButton::origin() const
	{
		return Point<int16_t>();
	}

	void ClassicButton::set_dimensions(Point<int16_t> dim)
	{
		dimension = dim;
	}

	void ClassicButton::set_label(const std::string& text)
	{
		label.change_text(text);
	}
}
