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
#include "UIChatBar.h"

#include "../Window.h"

#include "../UI.h"
#include "../Components/MapleButton.h"
#include "../Components/ChatBalloon.h"
#include "UIStatusBar.h"

#include "../Notifications.h"

#include "../../Net/Packets/GameplayPackets.h"
#include "../../Net/Packets/InventoryPackets.h"
#include "../../Net/Packets/BuddyPackets.h"
#include "../../Net/Packets/MessagingPackets.h"
#include "../../Audio/Audio.h"
#include "../../Graphics/ClassicStyle.h"
#include "../../Constants.h"
#include "../../Gameplay/Stage.h"
#include "../../Character/Party.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <list>
#include <sstream>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		std::string trim(const std::string& value)
		{
			size_t first = value.find_first_not_of(' ');
			if (first == std::string::npos)
				return "";

			size_t last = value.find_last_not_of(' ');
			return value.substr(first, last - first + 1);
		}

		std::string lowercase(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return value;
		}

		bool parse_int32(const std::string& value, int32_t& out)
		{
			if (value.empty())
				return false;

			size_t start = 0;
			if (value[0] == '+' || value[0] == '-')
				start = 1;

			for (size_t i = start; i < value.size(); i++)
				if (!std::isdigit(static_cast<unsigned char>(value[i])))
					return false;

			try { out = std::stoi(value); }
			catch (...) { return false; }
			return true;
		}
	}

	UIChatBar::UIChatBar(Point<int16_t> pos) : UIElement(pos, Point<int16_t>(500, 60))
	{
		chatopen = false;
		chatfieldopen = false;
		chattarget = CHT_ALL;
		party_id = -1;
		party_leader_id = -1;
		pending_party_invite_id = -1;
#ifdef __ANDROID__
		chatrows = TOUCH_ROWS;
#else
		chatrows = 4;
#endif
		rowpos = 0;
		rowmax = -1;
		lastpos = 0;
		dragchattop = false;

		nl::node mainbar = nl::nx::ui["StatusBar2.img"]["mainBar"];

		buttons[BT_OPENCHAT] = std::make_unique<MapleButton>(mainbar["chatOpen"]);
		buttons[BT_CLOSECHAT] = std::make_unique<MapleButton>(mainbar["chatClose"]);
		buttons[BT_SCROLLUP] = std::make_unique<MapleButton>(mainbar["scrollUp"]);
		buttons[BT_SCROLLDOWN] = std::make_unique<MapleButton>(mainbar["scrollDown"]);
		buttons[BT_CHATTARGETS] = std::make_unique<MapleButton>(mainbar["chatTarget"]["base"]);

		// Emoticon picker, from v83's own social-chat assets.
		nl::node social = nl::nx::ui["UIWindow2.img"]["socialChat"];
		buttons[BT_IMOTICON] = std::make_unique<MapleButton>(social["BtImoticon"], Point<int16_t>(-30, -59));
		emoticon_frame = Texture(social["ImoticonFrame"]["backgrnd1"]);

		nl::node faces = nl::nx::character["Face"]["00020000.img"];
		for (int32_t i = 0; i < picker_count(); i++)
			emoticon_icons.emplace_back(faces[picker_name(i)]["0"]["face"]);

		chatspace[0] = mainbar["chatSpace"];
		chatspace[1] = mainbar["chatEnter"];
		chatenter = mainbar["chatSpace2"];
		chatcover = mainbar["chatCover"];

		chattargets[CHT_ALL] = mainbar["chatTarget"]["all"];
		chattargets[CHT_BUDDY] = mainbar["chatTarget"]["friend"];
		chattargets[CHT_GUILD] = mainbar["chatTarget"]["guild"];
		chattargets[CHT_ALLIANCE] = mainbar["chatTarget"]["association"];
		chattargets[CHT_PARTY] = mainbar["chatTarget"]["party"];
		chattargets[CHT_SQUAD] = mainbar["chatTarget"]["expedition"];

		nl::node chat = nl::nx::ui["StatusBar2.img"]["chat"];
		tapbar = chat["tapBar"];
		tapbartop = chat["tapBarOver"];

		// Small megaphone icons (UIWindow.img/Megaphone/0..3, 13x12 each)
		// painted before megaphone / super / item / triple chat lines.
		nl::node mega_icons = nl::nx::ui["UIWindow.img"]["Megaphone"];
		for (int i = 0; i < 4; i++)
			megaphone_icons[i] = Texture(mega_icons[std::to_string(i)]);

		chatbox = ColorBox(502, 1 + chatrows * CHATROWHEIGHT, Color::Name::BLACK, 0.6f);

		// Bump the font from A11M to A12M and lower the bounds top a
		// few pixels so the typed text sits centered in the input
		// strip instead of jamming against its top edge.
		chatfield = Textfield(Text::A12M, Text::LEFT, Color::Name::BLACK, Rectangle<int16_t>(Point<int16_t>(-435, -59), Point<int16_t>(-40, -39)), 0);
#ifdef __ANDROID__
		typing_hint = Text(Text::A12M, Text::LEFT, Color::Name::DUSTYGRAY, "Say something");
#else
		typing_hint = Text(Text::A12M, Text::LEFT, Color::Name::DUSTYGRAY, "Say something, then press Enter to send");
#endif
		touch_close = Text(Text::A12B, Text::CENTER, Color::Name::WHITE, "X");
		send_label = Text(Text::A12B, Text::CENTER, Color::Name::WHITE, "Send");
		whisper_chip = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, "Whisper");

		const char* chip_names[NUM_CHIPS] = { "All", "Party", "Guild", "Buddy" };

		for (size_t i = 0; i < NUM_CHIPS; i++)
			chip_labels[i] = Text(Text::Font::A12B, Text::Alignment::CENTER, Color::Name::WHITE, chip_names[i]);

		set_chat_open(chatopen);

		chatfield.set_enter_callback(
			[&](std::string msg)
			{
				submit_chat(msg);
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::UP,
			[&]()
			{
				if (lastpos > 0)
				{
					lastpos--;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::DOWN,
			[&]()
			{
				if (lastentered.size() > 0 && lastpos < lastentered.size() - 1)
				{
					lastpos++;
					chatfield.change_text(lastentered[lastpos]);
				}
			}
		);

		chatfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				toggle_chatfield(false);
			}
		);

		slider = Slider(11, Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14), -22, chatrows, 1,
			[&](bool up)
			{
				int16_t next = up ? rowpos - 1 : rowpos + 1;
				if (next >= 0 && next <= rowmax)
					rowpos = next;
			}
		);

		send_line("[Welcome] Welcome to MapleStory!", YELLOW);
	}

	void UIChatBar::submit_chat(std::string msg)
	{
		msg = trim(msg);
		if (msg.empty())
		{
			// Empty Enter minimizes the whole chat window.
			set_chat_open(false);
			return;
		}

		if (handle_whisper_command(msg))
		{
		}
		else if (handle_party_command(msg))
		{
		}
		else if (!whisper_target.empty())
		{
			send_whisper(whisper_target, msg);
		}
		else
		{
			send_chat_message(msg);
		}

		lastentered.push_back(msg);
		lastpos = lastentered.size();
		chatfield.change_text("");
	}

	namespace
	{
		// Who a chat line is from, read from its text: "Name: hi",
		// "[Party] Name: hi", "Name <Whisper>: hi" or "To Name <Whisper>: hi".
		struct LineSpeaker
		{
			std::string name;
			size_t name_start = 0;
			size_t name_end = 0;
			bool whisper = false;
			bool outgoing = false;
		};

		LineSpeaker line_speaker(const std::string& line)
		{
			constexpr size_t MAX_NAME = 13;

			LineSpeaker speaker;
			size_t start = 0;

			// "[To Party] hi" is the player's own line
			if (line.rfind("[To ", 0) == 0)
				return speaker;

			if (!line.empty() && line[0] == '[')
			{
				size_t close = line.find("] ");

				if (close == std::string::npos)
					return speaker;

				start = close + 2;
			}

			const std::string tag = " <Whisper>: ";
			size_t end = line.find(tag, start);
			bool whisper = end != std::string::npos;
			bool outgoing = false;

			if (whisper)
			{
				if (line.compare(start, 3, "To ") == 0)
				{
					outgoing = true;
					start += 3;
				}
			}
			else
			{
				end = line.find(": ", start);

				if (end == std::string::npos)
					return speaker;
			}

			if (end <= start || end - start > MAX_NAME)
				return speaker;

			for (size_t i = start; i < end; i++)
				if (!std::isalnum(static_cast<unsigned char>(line[i])))
					return speaker;

			speaker.name = line.substr(start, end - start);
			speaker.name_start = start;
			speaker.name_end = end;
			speaker.whisper = whisper;
			speaker.outgoing = outgoing;

			return speaker;
		}
	}

	bool UIChatBar::handle_whisper_command(const std::string& message)
	{
		std::istringstream in(message);
		std::string command;
		std::string name;

		in >> command;
		std::transform(command.begin(), command.end(), command.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (command == "/r")
		{
			name = last_whisper_from;

			if (name.empty())
			{
				send_line("No one has whispered to you yet.", YELLOW);

				return true;
			}
		}
		else if (command == "/w" || command == "/whisper")
		{
			in >> name;

			if (name.empty())
			{
				send_line("To whisper, type /w name message", YELLOW);

				return true;
			}
		}
		else
		{
			return false;
		}

		std::string text;
		std::getline(in, text);
		text = trim(text);

		if (text.empty())
			whisper_to(name);
		else
			send_whisper(name, text);

		return true;
	}

	void UIChatBar::send_whisper(const std::string& name, const std::string& message)
	{
		WhisperPacket(name, message).dispatch();
		send_line("To " + name + " <Whisper>: " + message, PINK);
	}

	void UIChatBar::whisper_to(const std::string& name)
	{
		if (name.empty())
			return;

		whisper_target = name;
		whisper_label = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "Whisper to " + name + ". Tap Whisper to stop.");
		focus_chatfield();
	}

	std::string UIChatBar::whisper_name_at(Point<int16_t> at) const
	{
		int16_t row = row_at(at);
		auto text = rowtexts.find(row);
		auto line = rowstrings.find(row);

		if (text == rowtexts.end() || line == rowstrings.end())
			return "";

		LineSpeaker speaker = line_speaker(line->second);

		if (speaker.name.empty() || speaker.name == Stage::get().get_player().get_name())
			return "";

		// A whisper is a conversation, so the whole line answers it. Elsewhere
		// only the name does, and the rest of the line is for chatting.
		if (!speaker.whisper)
		{
			constexpr int16_t REACH = 12;

			int16_t x = at.x() - touch_left() - 4;
			int16_t name_left = static_cast<int16_t>(text->second.advance(speaker.name_start));
			int16_t name_right = static_cast<int16_t>(text->second.advance(speaker.name_end));

			if (x < name_left - REACH || x > name_right + REACH)
				return "";
		}

		return speaker.name;
	}

	void UIChatBar::set_chat_open(bool open)
	{
		chatopen = open;

		if (open)
			unread_whisper_from.clear();

#ifdef __ANDROID__
		// Phones draw and hit test their own larger controls
		for (auto& button : buttons)
			button.second->set_active(false);

		if (open)
			rowpos = rowmax;
#else
		buttons[BT_OPENCHAT]->set_active(!open);
		buttons[BT_CLOSECHAT]->set_active(open);
		buttons[BT_CHATTARGETS]->set_active(open);
		buttons[BT_SCROLLUP]->set_active(open);
		buttons[BT_SCROLLDOWN]->set_active(open);
		buttons[BT_IMOTICON]->set_active(open);
#endif

		if (!open)
		{
			chatfieldopen = false;
			emoticons_open = false;
			chatfield.set_state(Textfield::State::DISABLED);
		}
		else
		{
			chatfield.set_state(Textfield::State::NORMAL);
		}
	}

	Point<int16_t> UIChatBar::emoticon_origin() const
	{
		int16_t rows = (picker_count() + EMOTE_COLS - 1) / EMOTE_COLS;
		int16_t h = rows * EMOTE_CELL + EMOTE_PAD * 2;

#ifdef __ANDROID__
		// Under the typing bar and above the keyboard, or over the log
		if (typing_bar_shown())
			return Point<int16_t>(typing_rect().left(), typing_rect().bottom() + 2);

		return Point<int16_t>(touch_left(), std::max<int16_t>(TYPING_TOP, touch_bottom() - h));
#endif

		// Sits directly above its own button so the panel never covers the
		// chat input the user is typing into.
		return position + Point<int16_t>(-30 - EMOTE_COLS * EMOTE_CELL / 2, -64 - h);
	}

	int32_t UIChatBar::emoticon_at(Point<int16_t> cursorpos) const
	{
		if (!emoticons_open)
			return -1;

		Point<int16_t> grid = emoticon_origin() + Point<int16_t>(EMOTE_PAD, EMOTE_PAD);
		int16_t dx = cursorpos.x() - grid.x();
		int16_t dy = cursorpos.y() - grid.y();

		if (dx < 0 || dy < 0)
			return -1;

		int16_t col = dx / EMOTE_CELL;
		int16_t row = dy / EMOTE_CELL;

		if (col >= EMOTE_COLS)
			return -1;

		int32_t index = row * EMOTE_COLS + col;

		return index < picker_count() ? index : -1;
	}

	void UIChatBar::draw_emoticons(Point<int16_t> pos) const
	{
		if (!emoticons_open)
			return;

		int16_t rows = (picker_count() + EMOTE_COLS - 1) / EMOTE_COLS;
		int16_t w = EMOTE_COLS * EMOTE_CELL + EMOTE_PAD * 2;
		int16_t h = rows * EMOTE_CELL + EMOTE_PAD * 2;

		classic::panel(pos, w, h);

		Point<int16_t> grid = pos + Point<int16_t>(EMOTE_PAD, EMOTE_PAD);

		for (int32_t i = 0; i < picker_count(); i++)
		{
			int16_t col = i % EMOTE_COLS;
			int16_t row = i / EMOTE_COLS;
			Point<int16_t> cell = grid + Point<int16_t>(col * EMOTE_CELL, row * EMOTE_CELL);

			// These sprites are only eyes and mouths, drawn to sit on a head.
			// On the dark panel they can't be made out, so each gets a skin tile.
			ColorBox(EMOTE_CELL - 2, EMOTE_CELL - 2, Color::Name::CREAM, 1.0f)
				.draw(DrawArgument(cell));

			if (i == emoticon_hover)
				ColorBox(EMOTE_CELL - 2, EMOTE_CELL - 2, Color::Name::HUD_EXP, 0.5f)
					.draw(DrawArgument(cell));

			if (i < static_cast<int32_t>(emoticon_icons.size()) && emoticon_icons[i].is_valid())
			{
				// Face sprites carry their own origin, which would scatter them
				// across the grid -- place them on the cell centre instead.
				Point<int16_t> dim = emoticon_icons[i].get_dimensions();
				emoticon_icons[i].draw(DrawArgument(
					cell + Point<int16_t>((EMOTE_CELL - dim.x()) / 2 + dim.x() / 2,
					                      (EMOTE_CELL - dim.y()) / 2 + dim.y() / 2)));
			}
		}
	}

	void UIChatBar::make_face(const std::string& name)
	{
		for (auto iter : Expression::names)
		{
			if (iter.second != name)
				continue;

			int32_t emote = Expression::toemote(iter.first);

			if (emote >= 1 && emote <= 7)
				Stage::get().get_player().send_emote(emote);

			return;
		}
	}

	void UIChatBar::toggle_emoticons()
	{
		emoticons_open = !emoticons_open;

#ifndef __ANDROID__
		// On a phone the keyboard would cover the picker, so it waits for a pick
		if (emoticons_open)
			focus_chatfield();
#endif
	}

	int32_t UIChatBar::picker_count()
	{
#ifdef __ANDROID__
		return 7;
#else
		return Text::emoticon_count();
#endif
	}

	const char* UIChatBar::picker_name(int32_t index)
	{
#ifdef __ANDROID__
		// The faces the server lets anyone make: the F1 to F7 ones
		const char* faces[] = { "smile", "troubled", "cry", "angry", "bewildered", "stunned", "hit" };

		return index >= 0 && index < 7 ? faces[index] : "default";
#else
		return Text::emoticon_name(index);
#endif
	}

	void UIChatBar::pick_emoticon(int32_t index)
	{
#ifdef __ANDROID__
		// A face plays on the character for everyone to see. It isn't a line of chat.
		Sound(Sound::Name::BUTTONCLICK).play();
		emoticons_open = false;
		emoticon_hover = -1;
		make_face(picker_name(index));

		return;
#endif

		Sound(Sound::Name::BUTTONCLICK).play();
		focus_chatfield();
		chatfield.add_string("#e" + std::to_string(index) + "#");
		emoticons_open = false;

		// The seven F1-F7 faces also play on the character, so touch
		// players (who have no F keys) can make faces too.
		make_face(picker_name(index));
		emoticon_hover = -1;
	}

	void UIChatBar::focus_chatfield()
	{
		if (!chatopen)
		{
			set_chat_open(true);
			opened_for_typing = true;
		}

		chatfieldopen = true;
		chatfield.set_state(Textfield::State::FOCUSED);
	}

	void UIChatBar::draw(float inter) const
	{
#ifdef __ANDROID__
		draw_touch();
		return;
#endif

		chatspace[chatopen ? 1 : 0].draw(position);
		chatenter.draw(position);

		UIElement::draw_buttons(inter);

		draw_emoticons(emoticon_origin());

		if (chatopen)
		{
			tapbartop.draw(Point<int16_t>(position.x() - 576, getchattop()));

			chatbox.draw(DrawArgument(Point<int16_t>(0, getchattop() + 2)));

			draw_log_rows(0);

			slider.draw(Point<int16_t>(position.x(), getchattop() + 5));

			if (chattarget >= CHT_ALL && chattarget < NUM_TARGETS)
				chattargets[chattarget].draw(DrawArgument(position + Point<int16_t>(0, 2)));

			chatcover.draw(position);
			chatfield.draw(position);
		}
		else if (rowtexts.count(rowmax))
		{
			rowtexts.at(rowmax).draw(position + Point<int16_t>(-500, -60));
		}

		if (typing_bar_shown())
			draw_typing_bar();
	}

	void UIChatBar::draw_log_rows(int16_t left) const
	{
		int16_t chattop = getchattop();
		int16_t chatbottom = chattop + chatrows * CHATROWHEIGHT;
		Range<int16_t> chatclip(chattop + 2, chatbottom);

		int16_t chatheight = CHATROWHEIGHT * chatrows;
		int16_t yshift = -chatheight;

		for (int16_t i = 0; i < chatrows; i++)
		{
			int16_t rowid = rowpos - i;

			if (!rowtexts.count(rowid))
				break;

			int16_t lines_here = rowtexts.at(rowid).height() / CHATROWHEIGHT;
			if (lines_here < 1)
				lines_here = 1;
			int16_t textheight = lines_here;

			while (textheight > 0)
			{
				yshift += CHATROWHEIGHT;
				textheight--;
			}

			int16_t msgy = chattop - yshift - 7;

			if (msgy < chattop)
				continue;

			// Megaphone rows: paint a colored strip ALIGNED WITH the
			// actual text row (so it sits behind the glyphs, not
			// above or below), then draw name → icon → message.
			auto mega_it = mega_rows.find(rowid);
			if (mega_it != mega_rows.end())
			{
				const MegaRow& mr = mega_it->second;

				Color::Name bg_color = Color::Name::WHITE;
				switch (mr.icon)
				{
				case 0: bg_color = Color::Name::BLUE;   break; // regular
				case 1: bg_color = Color::Name::PINK;   break; // super
				case 2: bg_color = Color::Name::GREEN;  break; // item
				case 3: bg_color = Color::Name::YELLOW; break; // triple
				}

				// A12M glyph top sits at msgy + (linespace - bearing) ≈
				// msgy+5; bottom ≈ msgy+17. Strip matches that band
				// exactly so it sits BEHIND the glyphs only, and uses
				// a softer alpha so the text stays legible.
				int16_t strip_top = msgy + 5;
				int16_t strip_h   = 13;
				ColorBox row_bg(496, strip_h, bg_color, 0.18f);
				row_bg.draw(DrawArgument(Point<int16_t>(left + 2, strip_top)));

				int16_t x = left + 4;

				if (!mr.namepart.empty())
				{
					mr.namepart.draw(
						DrawArgument(Point<int16_t>(x, msgy)), chatclip);
					x += mr.namepart.width() + 2;
				}

				// Center the 12-tall megaphone icon over the strip.
				if (mr.icon >= 0 && mr.icon < 4
					&& megaphone_icons[mr.icon].is_valid())
				{
					int16_t icon_y = strip_top + (strip_h - 12) / 2;
					megaphone_icons[mr.icon].draw(
						DrawArgument(Point<int16_t>(x, icon_y)));
					x += megaphone_icons[mr.icon].width() + 2;
				}

				mr.msgpart.draw(
					DrawArgument(Point<int16_t>(x, msgy)), chatclip);

				ChatBalloon::draw_inline_images(
					mr.msgpart, Point<int16_t>(x, msgy), chatclip);
			}
			else
			{
				rowtexts.at(rowid).draw(
					DrawArgument(Point<int16_t>(left + 4, msgy)), chatclip);

				// Text::draw paints glyphs only; inline #v/#i/#q/#s/#f/#e
				// sprites are a separate overlay, which the chat window
				// was never doing -- so emoticons showed in speech
				// balloons but not in the chat log.
				ChatBalloon::draw_inline_images(
					rowtexts.at(rowid), Point<int16_t>(left + 4, msgy), chatclip);
			}

			if (has_selection)
			{
				int16_t sr, sc, er, ec;
				ordered_sel(sr, sc, er, ec);

				if (rowid >= sr && rowid <= er)
				{
					auto rit = rowstrings.find(rowid);
					int16_t len = rit != rowstrings.end()
						? static_cast<int16_t>(rit->second.size()) : 0;
					int16_t from = (rowid == sr) ? sc : 0;
					int16_t to = (rowid == er) ? ec : len;
					if (to < from) to = from;

					int16_t x0 = 4 + static_cast<int16_t>(rowtexts.at(rowid).advance(from));
					int16_t x1 = 4 + static_cast<int16_t>(rowtexts.at(rowid).advance(to));
					int16_t w = x1 - x0;

					if (w > 0)
					{
						// Hug the glyph band (A12M top ≈ msgy+5, bottom ≈ msgy+18)
						// so the highlight sits on the text, and keep it bold.
						ColorBox cover(w, 13, Color::Name::WHITE, 0.55f);
						cover.draw(DrawArgument(Point<int16_t>(x0, msgy + 5)));
					}
				}
			}
		}
	}

	bool UIChatBar::typing_bar_shown() const
	{
		return UI::get().is_typing_on_top() && chatfield.get_state() == Textfield::State::FOCUSED;
	}

	void UIChatBar::draw_typing_bar() const
	{
		// A typing bar at the top of the screen, above the soft keyboard:
		// a light field in a gold rim.
		int16_t viewwidth = Constants::Constants::get().get_viewwidth();

#ifdef __ANDROID__
		Rectangle<int16_t> field = typing_field_rect();
		Point<int16_t> bar(field.left() + 14, field.top() + 4);
		int16_t width = field.width() - 20;
		int16_t bar_h = TYPING_H;
#else
		int16_t width = std::min<int16_t>(viewwidth - 32, 460);

		// Below the line the scrolling notice runs along
		Point<int16_t> bar((viewwidth - width) / 2, 30);
		int16_t bar_h = 36;
#endif

		// A light input field in a gold rim
		if (!classic::input(bar + Point<int16_t>(-14, -4), width + 20, bar_h))
		{
			classic::panel(bar + Point<int16_t>(-3, -3), width + 6, bar_h - 2);
			classic::box(bar.x(), bar.y(), width, bar_h - 8, Color::Name::GALLERY, 0.97f);
			classic::box(bar.x(), bar.y(), 4, bar_h - 8, Color::Name::HUD_EXP);
		}

		// The field's bounds start at (-435, -59) from the bar's position.
		Point<int16_t> text_pos = bar + Point<int16_t>(14, (bar_h - 26) / 2);

		if (chatfield.empty())
			typing_hint.draw(DrawArgument(text_pos));

		chatfield.draw(text_pos + Point<int16_t>(435, 59));

#ifdef __ANDROID__
		// Who the line goes to: a tap moves on to the next
		Rectangle<int16_t> target = typing_target_rect();
		size_t chip = chip_index();

		classic::tab(target.get_left_top(), target.width(), target.height(), true);

		Text& chip_label = whisper_target.empty() ? chip_labels[chip] : whisper_chip;

		chip_label.change_color(Color::Name::HUD_PANEL);
		chip_label.draw(Point<int16_t>(target.left() + target.width() / 2, classic::text_y(chip_label, target.top() + target.height() / 2)));

		draw_emoticon_button(typing_emoticon_rect());

		// Drawn after the field, so long text stops at the button
		Rectangle<int16_t> send = typing_send_rect();

		classic::button(send.get_left_top(), send.width(), send.height());
		send_label.change_text(chatfield.empty() ? "Close" : "Send");
		classic::shadowed(send_label, Point<int16_t>(send.left() + send.width() / 2, classic::text_y(send_label, send.top() + send.height() / 2)));
#endif

#ifdef __ANDROID__
		// The picker opens where the recent lines go
		if (emoticons_open)
			return;
#endif

		// The keyboard covers the log, so the latest lines stay readable
		// here, to reply to
		constexpr int16_t RECENT_LINES = 4;

#ifdef __ANDROID__
		int16_t recent_w = typing_rect().width();
		int16_t recent_x = typing_rect().left();
		int16_t recent_y = typing_rect().bottom() + 2;
#else
		int16_t recent_w = 500;
		int16_t recent_x = (viewwidth - recent_w) / 2;
		int16_t recent_y = bar.y() + 36;
#endif

		int16_t lines = 0;
		int16_t first = rowmax + 1;

		while (rowtexts.count(first - 1))
		{
			int16_t height = std::max<int16_t>(rowtexts.at(first - 1).height() / CHATROWHEIGHT, 1);

			if (lines + height > RECENT_LINES)
				break;

			lines += height;
			first--;
		}

		int16_t whisper_h = whisper_target.empty() ? 0 : CHATROWHEIGHT + 4;

		if (lines > 0 || whisper_h > 0)
		{
			int16_t y = recent_y;

			classic::box(recent_x, y, recent_w, lines * CHATROWHEIGHT + 6 + whisper_h, Color::Name::HUD_PANEL, 0.78f);

			if (whisper_h > 0)
			{
				whisper_label.draw(Point<int16_t>(recent_x + 8, y));
				y += whisper_h;
			}

			for (int16_t row = first; row <= rowmax; row++)
			{
				rowtexts.at(row).draw(Point<int16_t>(recent_x + 8, y));
				y += std::max<int16_t>(rowtexts.at(row).height() / CHATROWHEIGHT, 1) * CHATROWHEIGHT;
			}
		}
	}

	int16_t UIChatBar::touch_left() const
	{
		return std::max<int16_t>((Constants::Constants::get().get_viewwidth() - TOUCH_W) / 2, 0);
	}

	int16_t UIChatBar::touch_bottom() const
	{
		return position.y() - TOUCH_ABOVE_DOCK;
	}

	Rectangle<int16_t> UIChatBar::line_rect() const
	{
		int16_t left = touch_left();
		int16_t bottom = touch_bottom();

		return Rectangle<int16_t>(left, left + TOUCH_W, bottom - TOUCH_LINE_H - TOUCH_LINE_REACH, bottom);
	}

	Rectangle<int16_t> UIChatBar::log_rect() const
	{
		int16_t left = touch_left();

		return Rectangle<int16_t>(left, left + TOUCH_W, getchattop(), touch_bottom());
	}

	Rectangle<int16_t> UIChatBar::emoticon_button_rect() const
	{
		// In the chips' row, beside the close button
		Rectangle<int16_t> close = log_close_rect();

		return Rectangle<int16_t>(close.left() - 4 - EMOTE_BTN_W, close.left() - 4, close.top(), close.bottom());
	}

	Rectangle<int16_t> UIChatBar::log_close_rect() const
	{
		int16_t right = touch_left() + TOUCH_W;
		int16_t top = getchattop() - CHIP_H - 2;

		return Rectangle<int16_t>(right - 48, right, top, top + CHIP_H);
	}

	Rectangle<int16_t> UIChatBar::emoticon_panel_rect() const
	{
		int16_t rows = (picker_count() + EMOTE_COLS - 1) / EMOTE_COLS;
		Point<int16_t> origin = emoticon_origin();

		return Rectangle<int16_t>(origin, origin + Point<int16_t>(EMOTE_COLS * EMOTE_CELL + EMOTE_PAD * 2, rows * EMOTE_CELL + EMOTE_PAD * 2));
	}

	Rectangle<int16_t> UIChatBar::typing_rect() const
	{
		int16_t viewwidth = Constants::Constants::get().get_viewwidth();
		int16_t width = std::min<int16_t>(viewwidth - 32, TYPING_MAX_W);
		int16_t left = std::max<int16_t>(std::min<int16_t>(TYPING_LEFT, viewwidth - width - 16), 16);

		return Rectangle<int16_t>(left, left + width, TYPING_TOP, TYPING_TOP + TYPING_H);
	}

	Rectangle<int16_t> UIChatBar::typing_target_rect() const
	{
		Rectangle<int16_t> all = typing_rect();

		return Rectangle<int16_t>(all.left(), all.left() + TYPING_BTN_W, all.top(), all.bottom());
	}

	Rectangle<int16_t> UIChatBar::typing_field_rect() const
	{
		Rectangle<int16_t> all = typing_rect();

		return Rectangle<int16_t>(all.left() + TYPING_BTN_W + EMOTE_BTN_W + TYPING_GAP * 2, all.right() - TYPING_BTN_W - TYPING_GAP, all.top(), all.bottom());
	}

	Rectangle<int16_t> UIChatBar::typing_emoticon_rect() const
	{
		Rectangle<int16_t> all = typing_rect();
		int16_t left = all.left() + TYPING_BTN_W + TYPING_GAP;

		return Rectangle<int16_t>(left, left + EMOTE_BTN_W, all.top(), all.bottom());
	}

	Rectangle<int16_t> UIChatBar::typing_send_rect() const
	{
		Rectangle<int16_t> all = typing_rect();

		return Rectangle<int16_t>(all.right() - TYPING_BTN_W, all.right(), all.top(), all.bottom());
	}

	UIChatBar::ChatTarget UIChatBar::chip_target(size_t index)
	{
		const ChatTarget targets[NUM_CHIPS] = { CHT_ALL, CHT_PARTY, CHT_GUILD, CHT_BUDDY };

		return targets[index];
	}

	size_t UIChatBar::chip_index() const
	{
		for (size_t i = 0; i < NUM_CHIPS; i++)
			if (chip_target(i) == chattarget)
				return i;

		return 0;
	}

	void UIChatBar::draw_touch() const
	{
		if (chatopen)
			draw_touch_log();
		else
			draw_touch_line();

		draw_emoticons(emoticon_origin());

		if (typing_bar_shown())
			draw_typing_bar();
	}

	void UIChatBar::draw_touch_line() const
	{
		int16_t left = touch_left();
		int16_t bottom = touch_bottom();
		int16_t top = bottom - TOUCH_LINE_H;

		classic::box(left, top, TOUCH_W, TOUCH_LINE_H, Color::Name::HUD_PANEL, 0.62f);
		classic::box(left, top, 2, TOUCH_LINE_H, Color::Name::HUD_EXP, 0.9f);

		if (!unread_whisper_from.empty())
		{
			classic::box(left, top, TOUCH_W, TOUCH_LINE_H, Color::Name::PINK, 0.45f);
			whisper_alert.draw(Point<int16_t>(left + 8, classic::text_y(whisper_alert, top + TOUCH_LINE_H / 2)));
		}
		else if (rowtexts.count(rowmax))
		{
			rowtexts.at(rowmax).draw(DrawArgument(Point<int16_t>(left + 8, top + 3)), Range<int16_t>(top + 1, bottom - 1));
		}

		// An arrow on the right says the line opens upwards
		int16_t cx = left + TOUCH_W - 12;

		for (int16_t i = 0; i < 6; i++)
			classic::box(cx - i, top + 12 + i, 2 * i + 1, 1, Color::Name::HUD_EXP, 0.9f);
	}

	void UIChatBar::draw_touch_log() const
	{
		int16_t left = touch_left();
		Rectangle<int16_t> log = log_rect();

		// Navy glass behind the log with a gold top edge, and the "send to"
		// chips above it
		classic::box(left, log.top(), TOUCH_W, log.height(), Color::Name::HUD_PANEL, 0.78f);
		classic::box(left, log.top(), TOUCH_W, 2, Color::Name::HUD_EXP, 0.9f);

		for (size_t i = 0; i < NUM_CHIPS; i++)
		{
			Rectangle<int16_t> r = chip_rect(i);
			bool lit = chattarget == chip_target(i);

			classic::tab(r.get_left_top(), r.width(), r.height(), lit);
			chip_labels[i].change_color(lit ? Color::Name::HUD_PANEL : Color::Name::WHITE);
			chip_labels[i].draw(Point<int16_t>(r.left() + r.width() / 2, classic::text_y(chip_labels[i], r.top() + r.height() / 2)));
		}

		Rectangle<int16_t> close = log_close_rect();
		int16_t close_size = 28;

		if (!classic::close_button(Point<int16_t>(close.left() + (close.width() - close_size) / 2, close.top() + (close.height() - close_size) / 2), close_size))
		{
			classic::tab(close.get_left_top(), close.width(), close.height(), false);
			touch_close.draw(Point<int16_t>(close.left() + close.width() / 2, classic::text_y(touch_close, close.top() + close.height() / 2)));
		}

		draw_log_rows(left);

		// Lists scroll by swiping, so this only shows where the log is
		if (rowmax >= chatrows)
		{
			int32_t track = log.height() - 8;
			int32_t thumb = std::max<int32_t>(14, track * chatrows / (rowmax + 1));
			int32_t y = (track - thumb) * rowpos / rowmax;

			classic::box(left + TOUCH_W - 5, static_cast<int16_t>(log.top() + 4 + y), 3, static_cast<int16_t>(thumb), Color::Name::HUD_EXP, 0.6f);
		}

		draw_emoticon_button(emoticon_button_rect());
	}

	void UIChatBar::draw_emoticon_button(Rectangle<int16_t> area) const
	{
		classic::cell(area.get_left_top(), area.width(), area.height(), emoticons_open);

		if (!emoticon_icons.empty() && emoticon_icons[0].is_valid())
			emoticon_icons[0].draw(DrawArgument(area.get_left_top() + Point<int16_t>(area.width() / 2, area.height() / 2)));
	}

	void UIChatBar::scroll_log(bool older)
	{
		if (older)
		{
			if (rowpos > 0)
				rowpos--;
		}
		else if (rowpos < rowmax)
		{
			rowpos++;
		}
	}

	bool UIChatBar::touch_hit(Point<int16_t> cursorpos) const
	{
#ifndef __ANDROID__
		return false;
#else
		if (typing_bar_shown() && typing_rect().contains(cursorpos))
			return true;

		if (emoticons_open && emoticon_panel_rect().contains(cursorpos))
			return true;

		if (!chatopen)
			return line_rect().contains(cursorpos);

		return chip_at(cursorpos) >= 0
			|| log_close_rect().contains(cursorpos)
			|| log_rect().contains(cursorpos)
			|| emoticon_button_rect().contains(cursorpos);
#endif
	}

	Cursor::State UIChatBar::send_cursor_touch(bool clicking, Point<int16_t> cursorpos)
	{
		bool starting = clicking && !tap.is_pressed();
		Point<int16_t> at;
		bool tapped = tap.update(clicking, cursorpos, at);

		if (starting)
		{
			swiping_log = chatopen && !typing_bar_shown() && log_rect().contains(cursorpos);
			swipe_anchor_y = cursorpos.y();
		}
		else if (clicking && swiping_log)
		{
			// A finger moving down brings older lines into view
			int16_t dy = cursorpos.y() - swipe_anchor_y;

			while (std::abs(dy) >= CHATROWHEIGHT)
			{
				scroll_log(dy > 0);
				dy += dy > 0 ? -CHATROWHEIGHT : CHATROWHEIGHT;
				swipe_anchor_y += cursorpos.y() > swipe_anchor_y ? CHATROWHEIGHT : -CHATROWHEIGHT;
				tap.cancel();
			}
		}
		else if (!clicking)
		{
			swiping_log = false;
		}

		emoticon_hover = emoticons_open && clicking ? emoticon_at(cursorpos) : -1;

		if (tapped)
			touch_tap(at);

		if (!touch_hit(cursorpos))
			return Cursor::State::IDLE;

		return clicking ? Cursor::State::CLICKING : Cursor::State::CANCLICK;
	}

	void UIChatBar::touch_tap(Point<int16_t> at)
	{
		if (typing_bar_shown())
		{
			int32_t typing_pick = emoticon_at(at);

			if (typing_pick >= 0)
			{
				pick_emoticon(typing_pick);
			}
			else if (typing_emoticon_rect().contains(at))
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				toggle_emoticons();
				emoticons_typing = emoticons_open;
			}
			else if (typing_target_rect().contains(at))
			{
				Sound(Sound::Name::BUTTONCLICK).play();

				if (!whisper_target.empty())
					whisper_target.clear();
				else
					set_chat_target(chip_target((chip_index() + 1) % NUM_CHIPS));
			}
			else if (typing_send_rect().contains(at))
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				submit_chat(chatfield.get_text());
			}
			else if (emoticons_open && !emoticon_panel_rect().contains(at))
			{
				emoticons_open = false;
			}

			return;
		}

		if (emoticons_open)
		{
			int32_t picked = emoticon_at(at);

			if (picked >= 0)
			{
				pick_emoticon(picked);
				return;
			}

			bool on_panel = emoticon_panel_rect().contains(at);

			emoticons_open = false;

			if (on_panel)
				return;

			// A tap on the button that opened it only closes it
			if (emoticon_button_rect().contains(at))
				return;
		}

		if (!chatopen)
		{
			if (line_rect().contains(at))
			{
				Sound(Sound::Name::BUTTONCLICK).play();

				if (!unread_whisper_from.empty())
				{
					std::string from = unread_whisper_from;

					whisper_to(from);
				}
				else
				{
					set_chat_open(true);
				}
			}

			return;
		}

		int32_t chip = chip_at(at);

		if (chip >= 0)
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			set_chat_target(chip_target(chip));
		}
		else if (log_close_rect().contains(at))
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			set_chat_open(false);
		}
		else if (emoticon_button_rect().contains(at))
		{
			Sound(Sound::Name::BUTTONCLICK).play();
			toggle_emoticons();
			emoticons_typing = false;
		}
		else if (log_rect().contains(at))
		{
			// A tap on the log starts a reply, to that player on their name. A
			// swipe scrolls it instead.
			std::string name = whisper_name_at(at);

			if (!name.empty())
				whisper_to(name);
			else
				toggle_chatfield(true);
		}
	}

	void UIChatBar::update()
	{
		UIElement::update();
		chatfield.update(position);

		if (!whisper_target.empty() && chatfield.get_state() != Textfield::State::FOCUSED)
			whisper_target.clear();

#ifdef __ANDROID__
		// The finger lifted somewhere else, so this touch is over
		if (tap.is_pressed() && !UI::get().is_mouse_held())
		{
			tap.reset();
			swiping_log = false;
		}

		// The picker belongs to the typing bar while it's open from there
		Rectangle<int16_t> keep = typing_rect();

		if (emoticons_open && emoticons_typing)
		{
			if (typing_bar_shown())
				keep = Rectangle<int16_t>(keep.left(), keep.right(), keep.top(), emoticon_panel_rect().bottom());
			else
				emoticons_open = false;
		}

		chatfield.set_keep_focus_area(keep);

		if (opened_for_typing && chatfield.get_state() != Textfield::State::FOCUSED)
		{
			opened_for_typing = false;

			if (chatopen)
				set_chat_open(false);
		}
#endif
	}

	void UIChatBar::set_position(Point<int16_t> pos)
	{
		position = pos;
	}

	// Bottom-anchored. x stays at the fixed 512 origin that UIStatusBar's
	// layout is expressed against (see UIStatusBar::v83_at, which subtracts
	// 512 to reach the left edge of the view) -- only the baseline moves with
	// the viewport. UIStateGame seeds this as (512, viewheight).
	void UIChatBar::update_screen(int16_t, int16_t new_height)
	{
		position = Point<int16_t>(position.x(), new_height);
	}

	void UIChatBar::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
			{
				toggle_chatfield();
			}
			else if (escape)
			{
#ifdef __ANDROID__
				if (chatfield.get_state() != Textfield::State::FOCUSED)
				{
					set_chat_open(false);
					return;
				}
#endif
				toggle_chatfield(false);
			}
		}
	}

	void UIChatBar::send_scroll(double yoffset)
	{
#ifdef __ANDROID__
		// The log follows the finger itself, in send_cursor_touch
		return;
#endif

		if (!chatopen)
			return;

		if (yoffset > 0)
		{
			if (rowpos > 0)
				rowpos--;
		}
		else if (yoffset < 0)
		{
			if (rowpos < rowmax)
				rowpos++;
		}

		slider.setrows(rowpos, chatrows, rowmax);
	}

	UIElement::Type UIChatBar::get_type() const
	{
		return TYPE;
	}

	Button::State UIChatBar::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case BT_OPENCHAT:
			set_chat_open(true);
			break;
		case BT_CLOSECHAT:
			set_chat_open(false);
			break;
		case BT_SCROLLUP:
			if (rowpos < rowmax)
				rowpos++;
			break;
		case BT_SCROLLDOWN:
			if (rowpos > 0)
				rowpos--;
			break;
		case BT_CHATTARGETS:
			cycle_chat_target();
			return Button::State::NORMAL;
		case BT_IMOTICON:
			toggle_emoticons();
			return Button::State::NORMAL;
		}

		return Button::State::NORMAL;
	}

	bool UIChatBar::is_in_range(Point<int16_t> cursorpos) const
	{
#ifdef __ANDROID__
		return touch_hit(cursorpos);
#endif

		if (emoticons_open)
		{
			int16_t rows = (picker_count() + EMOTE_COLS - 1) / EMOTE_COLS;
			Point<int16_t> ep = emoticon_origin();
			Point<int16_t> edim(EMOTE_COLS * EMOTE_CELL + EMOTE_PAD * 2,
			                    rows * EMOTE_CELL + EMOTE_PAD * 2);

			if (Rectangle<int16_t>(ep, ep + edim).contains(cursorpos))
				return true;
		}

		Point<int16_t> absp(0, getchattop() - 16);
		Point<int16_t> dim(500, chatrows * CHATROWHEIGHT + CHATYOFFSET + 16);
		return Rectangle<int16_t>(absp, absp + dim).contains(cursorpos);
	}

	int16_t UIChatBar::row_at(Point<int16_t> cursorpos) const
	{
		if (!chatopen)
			return ROW_NONE;

		int16_t chattop = getchattop();
		int16_t yshift = -CHATROWHEIGHT * chatrows;

		for (int16_t i = 0; i < chatrows; i++)
		{
			int16_t rowid = rowpos - i;

			auto it = rowtexts.find(rowid);
			if (it == rowtexts.end())
				break;

			int16_t lines = it->second.height() / CHATROWHEIGHT;
			if (lines < 1)
				lines = 1;

			yshift += lines * CHATROWHEIGHT;
			int16_t msgy = chattop - yshift - 7;

			if (msgy < chattop)
				continue;

			if (cursorpos.y() >= msgy && cursorpos.y() < msgy + lines * CHATROWHEIGHT)
				return rowid;
		}

		return ROW_NONE;
	}

	int16_t UIChatBar::col_at(int16_t rowid, int16_t x) const
	{
		auto it = rowtexts.find(rowid);
		auto sit = rowstrings.find(rowid);
		if (it == rowtexts.end() || sit == rowstrings.end())
			return 0;

		int16_t relx = x - 4;
		if (relx <= 0)
			return 0;

		size_t len = sit->second.size();
		for (size_t i = 0; i < len; i++)
		{
			int16_t a = static_cast<int16_t>(it->second.advance(i));
			int16_t b = static_cast<int16_t>(it->second.advance(i + 1));
			if (relx < (a + b) / 2)
				return static_cast<int16_t>(i);
		}
		return static_cast<int16_t>(len);
	}

	void UIChatBar::ordered_sel(int16_t& sr, int16_t& sc, int16_t& er, int16_t& ec) const
	{
		// reading order: smaller row id is older/higher on screen, then column
		if (sel_arow < sel_frow || (sel_arow == sel_frow && sel_acol <= sel_fcol))
		{
			sr = sel_arow; sc = sel_acol; er = sel_frow; ec = sel_fcol;
		}
		else
		{
			sr = sel_frow; sc = sel_fcol; er = sel_arow; ec = sel_acol;
		}
	}

	std::string UIChatBar::get_selected_text() const
	{
		if (!has_selection)
			return "";

		int16_t sr, sc, er, ec;
		ordered_sel(sr, sc, er, ec);
		std::string out;

		for (int16_t r = sr; r <= er; r++)
		{
			auto it = rowstrings.find(r);
			if (it == rowstrings.end())
				continue;

			const std::string& line = it->second;
			int16_t from = (r == sr) ? sc : 0;
			int16_t to = (r == er) ? ec : static_cast<int16_t>(line.size());
			if (from < 0) from = 0;
			if (to > static_cast<int16_t>(line.size())) to = static_cast<int16_t>(line.size());
			if (to < from) to = from;

			if (!out.empty())
				out += "\n";
			out += line.substr(from, to - from);
		}

		return out;
	}

	void UIChatBar::clear_selection()
	{
		has_selection = false;
		sel_dragging = false;
	}

	Cursor::State UIChatBar::send_cursor(bool clicking, Point<int16_t> cursorpos)
	{
#ifdef __ANDROID__
		return send_cursor_touch(clicking, cursorpos);
#endif

		// The picker sits above the bar and must win over everything under it,
		// including the chat-log selection drag.
		if (emoticons_open)
		{
			int32_t hovered = emoticon_at(cursorpos);
			emoticon_hover = hovered;

			if (hovered >= 0)
			{
				if (clicking)
				{
					pick_emoticon(hovered);
				}

				return Cursor::State::CANCLICK;
			}
		}

		// Custom button handling: break after a button press fires.
		// UIElement::send_cursor iterates ALL buttons in one loop, so when
		// BT_CLOSECHAT fires and activates BT_OPENCHAT, the loop immediately
		// processes BT_OPENCHAT in the same frame, causing toggle bounce.
		bool button_handled = false;

		for (auto& btit : buttons)
		{
			if (btit.second->is_active() && btit.second->bounds(position).contains(cursorpos))
			{
				if (btit.second->get_state() == Button::State::NORMAL)
				{
					Sound(Sound::Name::BUTTONOVER).play();
					btit.second->set_state(Button::State::MOUSEOVER);
					return Cursor::State::CANCLICK;
				}
				else if (btit.second->get_state() == Button::State::MOUSEOVER)
				{
					if (clicking)
					{
						Sound(Sound::Name::BUTTONCLICK).play();
						btit.second->set_state(button_pressed(btit.first));
						button_handled = true;
						break;  // Stop processing buttons after a press fires
					}
					else
					{
						return Cursor::State::CANCLICK;
					}
				}
			}
			else if (btit.second->get_state() == Button::State::MOUSEOVER)
			{
				btit.second->set_state(Button::State::NORMAL);
			}
		}

		if (button_handled)
			return Cursor::State::IDLE;

		if (slider.isenabled())
		{
			auto cursoroffset = cursorpos - Point<int16_t>(position.x(), getchattop() + 5);
			Cursor::State sstate = slider.send_cursor(cursoroffset, clicking);
			if (sstate != Cursor::State::IDLE)
				return sstate;
		}

		if (chatopen)
		{
			Cursor::State tstate = chatfield.send_cursor(cursorpos, clicking);

			if (tstate == Cursor::State::CLICKING || tstate == Cursor::State::CANCLICK)
				return tstate;

			// Don't let clicks outside the textfield defocus it —
			// only Enter key should close the chatfield
			if (clicking && chatfieldopen && chatfield.get_state() != Textfield::State::FOCUSED)
				chatfield.set_state(Textfield::State::FOCUSED);
		}
		else if (clicking)
		{
			// Clicking the chat input area when chat is closed opens and focuses it
			// Use the chatfield bounds relative to position to check
			Rectangle<int16_t> input_area(
				position + Point<int16_t>(-435, -58),
				position + Point<int16_t>(-40, -35)
			);

			if (input_area.contains(cursorpos))
			{
				toggle_chatfield(true);
				return Cursor::State::IDLE;
			}
		}

		auto chattop_rect = Rectangle<int16_t>(0, 502, getchattop(), getchattop() + 6);
		bool contains = chattop_rect.contains(cursorpos);

		if (dragchattop)
		{
			if (clicking)
			{
				int16_t ydelta = cursorpos.y() - getchattop();

				while (ydelta > 0 && chatrows > MINCHATROWS)
				{
					chatrows--;
					ydelta -= CHATROWHEIGHT;
				}

				while (ydelta < 0 && chatrows < MAXCHATROWS)
				{
					chatrows++;
					ydelta += CHATROWHEIGHT;
				}

				chatbox = ColorBox(502, 1 + chatrows * CHATROWHEIGHT, Color::Name::BLACK, 0.6f);
				slider.setrows(rowpos, chatrows, rowmax);
				slider.setvertical(Range<int16_t>(0, CHATROWHEIGHT * chatrows - 14));

				return Cursor::State::CLICKING;
			}
			else
			{
				dragchattop = false;
			}
		}
		else if (contains)
		{
			if (clicking)
			{
				dragchattop = true;
				return Cursor::State::CLICKING;
			}
			else
			{
				return Cursor::State::CANCLICK;
			}
		}

		if (!clicking)
			sel_dragging = false;

		if (chatopen && clicking)
		{
			int16_t ctop = getchattop();
			int16_t cbottom = ctop + chatrows * CHATROWHEIGHT;

			if (!sel_dragging)
			{
				// Begin a selection only when the press lands on an actual line.
				if (cursorpos.x() >= 0 && cursorpos.x() <= 500
					&& cursorpos.y() >= ctop && cursorpos.y() <= cbottom)
				{
					int16_t r = row_at(cursorpos);

					if (r != ROW_NONE)
					{
						sel_arow = r;
						sel_acol = col_at(r, cursorpos.x());
						sel_frow = sel_arow;
						sel_fcol = sel_acol;
						sel_dragging = true;
						has_selection = true;
						return Cursor::State::CLICKING;
					}
				}
			}
			else
			{
				// Mid-drag: keep extending even when the cursor leaves the
				// text — clamp it back into the log so the selection tracks
				// off the end of a line / above / below like a normal drag.
				int16_t cx = cursorpos.x();
				if (cx < 0) cx = 0;
				if (cx > 500) cx = 500;
				int16_t cy = cursorpos.y();
				if (cy < ctop) cy = ctop;
				if (cy > cbottom - 1) cy = cbottom - 1;

				int16_t r = row_at(Point<int16_t>(cx, cy));
				if (r != ROW_NONE)
				{
					sel_frow = r;
					sel_fcol = col_at(r, cx);
				}
				return Cursor::State::CLICKING;
			}
		}

		return Cursor::State::IDLE;
	}

	void UIChatBar::send_line(const std::string& line, LineType type)
	{
		LineSpeaker speaker = line_speaker(line);

		if (speaker.whisper && !speaker.outgoing && !speaker.name.empty())
		{
			last_whisper_from = speaker.name;

			if (!chatopen)
			{
				unread_whisper_from = speaker.name;
				whisper_alert = Text(Text::Font::A12B, Text::Alignment::LEFT, Color::Name::WHITE, "Whisper from " + speaker.name + ". Tap to reply.");
				Sound(Sound::Name::DLGNOTICE).play();
			}
		}

		bool following = rowpos == rowmax;

		rowmax++;

#ifdef __ANDROID__
		// Someone reading older lines isn't pulled away by new ones
		if (following)
			rowpos = rowmax;
#else
		rowpos = rowmax;
#endif

		slider.setrows(rowpos, chatrows, rowmax);

		Color::Name color;
		int8_t icon = -1;

		switch (type)
		{
		case RED:
			color = Color::Name::RED;
			break;
		case BLUE:
			color = Color::Name::BLUE;
			break;
		case YELLOW:
			color = Color::Name::YELLOW;
			break;
		case PINK:
			color = Color::Name::PINK;
			break;
		case LIGHTBLUE:
			color = Color::Name::LIGHTBLUE;
			break;
		case GREEN:
			color = Color::Name::GREEN;
			break;
		case ORANGE:
			color = Color::Name::ORANGE;
			break;
		case LIGHTGREEN:
			color = Color::Name::LIGHTGREEN;
			break;
		// Megaphone lines each pick a distinct icon and text color so
		// viewers can tell the broadcast sub-type at a glance.
		case MEGAPHONE:
			color = Color::Name::BLUE;
			icon = 0;
			break;
		case SUPER_MEGAPHONE:
			color = Color::Name::PINK;
			icon = 1;
			break;
		case ITEM_MEGAPHONE:
			color = Color::Name::GREEN;
			icon = 2;
			break;
		case TRIPLE_MEGAPHONE:
			color = Color::Name::YELLOW;
			icon = 3;
			break;
		default:
			color = Color::Name::WHITE;
			break;
		}

		// Reserve room for the icon + a small gap when the row has one.
		uint16_t text_maxwidth = (icon >= 0) ? 464 : 480;

		rowtexts.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(rowmax),
			std::forward_as_tuple(Text::Font::A12M, Text::Alignment::LEFT, color, line, text_maxwidth)
		);

		rowstrings[rowmax] = line;

		if (icon >= 0)
		{
			rowicons[rowmax] = icon;

			// Split the broadcast on the name/message separator so the icon
			// can sit between the player name and the colon. Try " : "
			// first (v83 canonical), then fall back to ":" for servers
			// that skip the surrounding spaces.
			std::string name_part;
			std::string msg_part = line;
			size_t sep = line.find(" : ");
			size_t sep_len = 3;
			if (sep == std::string::npos)
			{
				sep = line.find(':');
				sep_len = 1;
			}
			if (sep != std::string::npos)
			{
				name_part = line.substr(0, sep);
				msg_part  = line.substr(sep, sep_len == 3 ? std::string::npos : std::string::npos);
			}

			MegaRow row;
			row.icon = icon;
			row.namepart = Text(Text::Font::A12M, Text::Alignment::LEFT,
				color, name_part, text_maxwidth);
			row.msgpart = Text(Text::Font::A12M, Text::Alignment::LEFT,
				color, msg_part, text_maxwidth);
			mega_rows[rowmax] = std::move(row);
		}
	}

	void UIChatBar::send_chatline(const std::string& line, LineType type)
	{
		send_line(line, type);
	}

	void chat::log(const std::string& line, chat::LineType type)
	{
		if (auto cb = UI::get().get_element<UIChatBar>())
			cb->send_chatline(line, type);
	}

	void UIChatBar::display_message(Messages::Type line, UIChatBar::LineType type)
	{
		if (message_cooldowns[line] > 0)
			return;

		std::string message = Messages::messages[line];
		send_line(message, type);

		message_cooldowns[line] = MESSAGE_COOLDOWN;
	}

	void UIChatBar::toggle_chat()
	{
		set_chat_open(!chatopen);
	}

	void UIChatBar::toggle_chatfield()
	{
		chatfieldopen = !chatfieldopen;
		toggle_chatfield(chatfieldopen);
	}

	void UIChatBar::toggle_chatfield(bool open)
	{
		chatfieldopen = open;

		if (chatfieldopen)
		{
			if (!chatopen)
				set_chat_open(true);

			// set_state(FOCUSED) internally calls UI::get().focus_textfield()
			// so we must NOT call focus_textfield again — the double call
			// would reset the state back to NORMAL.
			chatfield.set_state(Textfield::State::FOCUSED);
		}
		else
		{
			chatfield.set_state(Textfield::State::DISABLED);
		}
	}

	bool UIChatBar::is_chatopen()
	{
		return chatopen;
	}

	bool UIChatBar::is_chatfieldopen()
	{
		return chatfieldopen;
	}

	Rectangle<int16_t> UIChatBar::chip_rect(size_t index) const
	{
		int16_t x = touch_left() + static_cast<int16_t>(index) * (CHIP_W + 4);
		int16_t y = getchattop() - CHIP_H - 2;

		return Rectangle<int16_t>(Point<int16_t>(x, y), Point<int16_t>(x + CHIP_W, y + CHIP_H));
	}

	int32_t UIChatBar::chip_at(Point<int16_t> cursorpos) const
	{
		for (size_t i = 0; i < NUM_CHIPS; i++)
			if (chip_rect(i).contains(cursorpos))
				return static_cast<int32_t>(i);

		return -1;
	}

	void UIChatBar::set_chat_target(ChatTarget target)
	{
		if (target >= CHT_ALL && target < NUM_TARGETS)
			chattarget = target;
	}

	void UIChatBar::cycle_chat_target()
	{
		chattarget = static_cast<ChatTarget>((chattarget + 1) % NUM_TARGETS);
	}

	void UIChatBar::set_pending_party_invite(int32_t in_party_id, const std::string& inviter)
	{
		pending_party_invite_id = in_party_id;
		pending_party_inviter = inviter;
	}

	void UIChatBar::clear_pending_party_invite()
	{
		pending_party_invite_id = -1;
		pending_party_inviter.clear();
	}

	void UIChatBar::set_party_state(int32_t in_party_id, int32_t leader_id, const std::vector<PartyMember>& members)
	{
		party_id = in_party_id;
		party_leader_id = leader_id;
		party_members = members;

		if (pending_party_invite_id == in_party_id)
			clear_pending_party_invite();
	}

	void UIChatBar::clear_party_state()
	{
		party_id = -1;
		party_leader_id = -1;
		party_members.clear();
	}

	void UIChatBar::set_party_leader(int32_t leader_id)
	{
		party_leader_id = leader_id;
	}

	void UIChatBar::update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp)
	{
		for (PartyMember& member : party_members)
		{
			if (member.id == cid)
			{
				member.hp = hp;
				member.max_hp = max_hp;
				return;
			}
		}
	}

	int32_t UIChatBar::get_party_id() const
	{
		return party_id;
	}

	int32_t UIChatBar::get_party_leader_id() const
	{
		return party_leader_id;
	}

	int32_t UIChatBar::get_pending_party_invite_id() const
	{
		return pending_party_invite_id;
	}

	const std::string& UIChatBar::get_pending_party_inviter() const
	{
		return pending_party_inviter;
	}

	const std::vector<UIChatBar::PartyMember>& UIChatBar::get_party_members() const
	{
		return party_members;
	}

	int32_t UIChatBar::resolve_party_member_id(const std::string& token) const
	{
		int32_t member_id = 0;
		if (parse_int32(token, member_id))
			return member_id;

		std::string lowered = lowercase(token);
		for (const PartyMember& member : party_members)
		{
			if (lowercase(member.name) == lowered)
				return member.id;
		}

		return 0;
	}

	bool UIChatBar::handle_party_command(const std::string& message)
	{
		if (message.empty() || message[0] != '/')
			return false;

		std::istringstream whole(message);
		std::string command;
		whole >> command;
		command = lowercase(command);

		std::string argument_block;
		std::getline(whole, argument_block);
		argument_block = trim(argument_block);

		if (command == "/pet" || command == "/petcmd")
		{
			PetLook& pet = Stage::get().get_player().get_pet(0);

			if (pet.get_itemid() == 0)
			{
				send_line("[Pet] You don't have a pet summoned.", RED);
				return true;
			}

			if (command == "/pet")
			{
				if (argument_block.empty())
				{
					send_line("[Pet] Usage: /pet <message>", YELLOW);
					return true;
				}

				PetChatPacket(pet.get_uniqueid(), 0, argument_block).dispatch();
				return true;
			}

			int32_t cmd_id = 0;
			parse_int32(argument_block, cmd_id);

			if (cmd_id < 0 || cmd_id > 9)
				cmd_id = 0;

			PetCommandPacket(pet.get_uniqueid(), static_cast<int8_t>(cmd_id)).dispatch();
			return true;
		}

		if (command == "/pt")
		{
			if (argument_block.empty())
			{
				send_line("[Party] Usage: /pt <message>", YELLOW);
				return true;
			}

			send_party_message(argument_block);
			return true;
		}

		if (command != "/party" && command != "/p")
			return false;

		if (argument_block.empty())
		{
			send_line("[Party] /party create | leave | invite <name> | join <partyId>", YELLOW);
			send_line("[Party] /party expel <name|id> | leader <name|id> | accept | deny | list", YELLOW);
			return true;
		}

		std::istringstream args(argument_block);
		std::string action;
		args >> action;
		action = lowercase(action);

		std::string argument;
		std::getline(args, argument);
		argument = trim(argument);

		if (action == "create")
		{
			CreatePartyPacket().dispatch();
			send_line("[Party] Sent create request.", YELLOW);
			return true;
		}

		if (action == "leave")
		{
			LeavePartyPacket().dispatch();
			send_line("[Party] Sent leave request.", YELLOW);
			return true;
		}

		if (action == "join")
		{
			int32_t join_party_id = 0;
			if (!parse_int32(argument, join_party_id))
			{
				send_line("[Party] Usage: /party join <partyId>", YELLOW);
				return true;
			}

			JoinPartyPacket(join_party_id).dispatch();
			send_line("[Party] Sent join request.", YELLOW);
			return true;
		}

		if (action == "invite")
		{
			if (argument.empty())
			{
				send_line("[Party] Usage: /party invite <name>", YELLOW);
				return true;
			}

			InviteToPartyPacket(argument).dispatch();
			send_line("[Party] Sent invite to " + argument + ".", YELLOW);
			return true;
		}

		if (action == "expel" || action == "kick")
		{
			int32_t member_id = resolve_party_member_id(argument);
			if (member_id <= 0)
			{
				send_line("[Party] Usage: /party expel <name|id>", YELLOW);
				return true;
			}

			ExpelFromPartyPacket(member_id).dispatch();
			send_line("[Party] Sent expel request.", YELLOW);
			return true;
		}

		if (action == "leader" || action == "lead")
		{
			int32_t member_id = resolve_party_member_id(argument);
			if (member_id <= 0)
			{
				send_line("[Party] Usage: /party leader <name|id>", YELLOW);
				return true;
			}

			ChangePartyLeaderPacket(member_id).dispatch();
			send_line("[Party] Sent leadership transfer request.", YELLOW);
			return true;
		}

		if (action == "accept")
		{
			if (pending_party_invite_id <= 0)
			{
				send_line("[Party] There is no pending invitation.", YELLOW);
				return true;
			}

			JoinPartyPacket(pending_party_invite_id).dispatch();
			send_line("[Party] Sent invitation accept request.", YELLOW);
			clear_pending_party_invite();
			return true;
		}

		if (action == "deny")
		{
			if (pending_party_inviter.empty())
			{
				send_line("[Party] There is no pending invitation.", YELLOW);
				return true;
			}

			DenyPartyInvitePacket(pending_party_inviter).dispatch();
			send_line("[Party] Declined invitation from " + pending_party_inviter + ".", YELLOW);
			clear_pending_party_invite();
			return true;
		}

		if (action == "list")
		{
			if (party_members.empty())
			{
				send_line("[Party] No cached party members.", YELLOW);
				return true;
			}

			send_line("[Party] Current members:", YELLOW);
			for (const PartyMember& member : party_members)
			{
				std::string leader_tag = member.id == party_leader_id ? " (leader)" : "";
				send_line(" - " + member.name + " Lv." + std::to_string(member.level) + leader_tag, WHITE);
			}
			return true;
		}

		send_line("[Party] Unknown command. Use /party for help.", YELLOW);
		return true;
	}

	void UIChatBar::send_targeted_message(const std::string& target, const std::string& message)
	{
		send_line("[To " + target + "] " + message, WHITE);
	}

	void UIChatBar::send_party_message(const std::string& message)
	{
		std::list<int32_t> recipients;
		MultiChatPacket(MultiChatPacket::PARTY, recipients, message).dispatch();
		send_targeted_message("Party", message);
	}

	void UIChatBar::send_chat_message(const std::string& message)
	{

		std::list<int32_t> recipients;

		switch (chattarget)
		{
		case CHT_ALL:
			GeneralChatPacket(message, true).dispatch();
			break;
		case CHT_BUDDY:
			MultiChatPacket(MultiChatPacket::BUDDY, recipients, message).dispatch();
			send_targeted_message("Buddy", message);
			break;
		case CHT_GUILD:
			MultiChatPacket(MultiChatPacket::GUILD, recipients, message).dispatch();
			send_targeted_message("Guild", message);
			break;
		case CHT_ALLIANCE:
			MultiChatPacket(MultiChatPacket::ALLIANCE, recipients, message).dispatch();
			send_targeted_message("Alliance", message);
			break;
		case CHT_SQUAD:
			send_party_message(message);
			break;
		case CHT_PARTY:
		default:
			send_party_message(message);
			break;
		}
	}

	int16_t UIChatBar::getchattop() const
	{
#ifdef __ANDROID__
		return touch_bottom() - chatrows * CHATROWHEIGHT - 4;
#else
		return position.y() - chatrows * CHATROWHEIGHT - CHATYOFFSET;
#endif
	}
}
