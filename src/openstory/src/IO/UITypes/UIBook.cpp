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
#include "UIBook.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Data/ItemData.h"
#include "../../Graphics/ClassicStyle.h"

#include <string>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		nl::node book_node(int32_t itemid)
		{
			return nl::nx::item["Etc"]["0" + std::to_string(itemid / 10000) + ".img"]["0" + std::to_string(itemid)]["book"];
		}

		// The script codes are for a parchment page: colors that suit it don't
		// suit this navy one, and the links and pictures don't work here.
		std::string plain_text(const std::string& text)
		{
			std::string out;

			for (size_t i = 0; i < text.size(); i++)
			{
				if (text[i] != '#' || i + 1 >= text.size())
				{
					out += text[i];
					continue;
				}

				char code = text[i + 1];

				if (code == 'L' || code == 'i' || code == 'v' || code == 't' || code == 'z' || code == 'h' || code == 'p' || code == 'o' || code == 'm' || code == 's')
				{
					size_t end = text.find('#', i + 1);

					if (end == std::string::npos)
						break;

					i = end;
				}
				else
				{
					i++;
				}
			}

			return out;
		}

		void add_text(std::string& body, nl::node node)
		{
			std::string text = plain_text(node.get_string());

			if (text.empty())
				return;

			if (!body.empty())
				body += "\\n";

			body += text;
		}
	}

	UIBook::UIBook(int32_t itemid)
		: UIElement(
			Point<int16_t>(
				(Constants::Constants::get().get_viewwidth() - W) / 2,
				(Constants::Constants::get().get_viewheight() - H) / 2),
			Point<int16_t>(W, H),
			true)
	{
		nl::node book = book_node(itemid);

		for (int32_t p = 0; book[std::to_string(p)]; p++)
		{
			nl::node source = book[std::to_string(p)];
			std::string body;

			if (source["text"])
				add_text(body, source["text"]);

			for (int32_t n = 0; source[std::to_string(n)]; n++)
				add_text(body, source[std::to_string(n)]["text"]);

			pages.emplace_back(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, body, static_cast<uint16_t>(W - 40));
		}

		title = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, ItemData::get(itemid).get_name());
		page_count = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE);
		prev_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Back");
		next_label = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Next");

		turn_to(0);
	}

	bool UIBook::has_book(int32_t itemid)
	{
		return itemid / 10000 == 416 && book_node(itemid)["0"];
	}

	Rectangle<int16_t> UIBook::close_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(W - CLOSE_SIZE - 8, 6);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(CLOSE_SIZE, CLOSE_SIZE));
	}

	Rectangle<int16_t> UIBook::prev_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(14, H - BTN_H - 10);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(BTN_W, BTN_H));
	}

	Rectangle<int16_t> UIBook::next_rect() const
	{
		Point<int16_t> tl = position + Point<int16_t>(W - BTN_W - 14, H - BTN_H - 10);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(BTN_W, BTN_H));
	}

	void UIBook::turn_to(int32_t index)
	{
		if (index < 0 || index >= static_cast<int32_t>(pages.size()))
			return;

		page = index;
		page_count.change_text(std::to_string(page + 1) + " / " + std::to_string(pages.size()));
	}

	void UIBook::draw(float inter) const
	{
		classic::panel(position, W, H);
		title.draw(position + Point<int16_t>(W / 2, 10));

		Rectangle<int16_t> close = close_rect();

		if (!classic::close_button(close.get_left_top(), close.width()))
			classic::button(close.get_left_top(), close.width(), close.height());

		int16_t text_top = position.y() + 42;
		int16_t text_bottom = position.y() + H - BTN_H - 16;

		if (page < static_cast<int32_t>(pages.size()))
			pages[page].draw(DrawArgument(Point<int16_t>(position.x() + 20, text_top)), Range<int16_t>(text_top, text_bottom));

		Rectangle<int16_t> prev = prev_rect();
		Rectangle<int16_t> next = next_rect();

		if (page > 0)
		{
			classic::button(prev.get_left_top(), prev.width(), prev.height());
			classic::shadowed(prev_label, Point<int16_t>(prev.left() + prev.width() / 2, classic::text_y(prev_label, prev.top() + prev.height() / 2)));
		}

		if (page + 1 < static_cast<int32_t>(pages.size()))
		{
			classic::button(next.get_left_top(), next.width(), next.height());
			classic::shadowed(next_label, Point<int16_t>(next.left() + next.width() / 2, classic::text_y(next_label, next.top() + next.height() / 2)));
		}

		page_count.draw(Point<int16_t>(position.x() + W / 2, classic::text_y(page_count, prev.top() + prev.height() / 2)));
	}

	Cursor::State UIBook::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Point<int16_t> at;

		if (!gesture.update(clicked, cursorpos, at))
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		if (close_rect().contains(at))
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			deactivate();
		}
		else if (prev_rect().contains(at) && page > 0)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			turn_to(page - 1);
		}
		else if (next_rect().contains(at) && page + 1 < static_cast<int32_t>(pages.size()))
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			turn_to(page + 1);
		}

		return Cursor::State::IDLE;
	}

	void UIBook::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
			deactivate();
	}

	UIElement::Type UIBook::get_type() const
	{
		return TYPE;
	}
}
