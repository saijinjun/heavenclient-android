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
#include "UIChannel.h"

#include "../KeyAction.h"
#include "../UI.h"

#include "../Components/ClassicButton.h"
#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Graphics/ClassicStyle.h"
#include "UIChatBar.h"

#include <chrono>
#include "../../Configuration.h"
#include "../../Net/ChannelLoadData.h"
#include "../../Net/Packets/GameplayPackets.h"

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UIChannel::UIChannel() : UIDragElement<PosCHANNEL>(Point<int16_t>(WIDTH, DRAG_HEIGHT))
	{
		uint8_t selected_world = Configuration::get().get_worldid();
		current_channel = Configuration::get().get_channelid();
		selected_channel = current_channel;

		// Prefer the real channel count published by the server; fall back
		// to 20 only if the login handshake never populated it.
		uint8_t real_count = ChannelLoadData::get().get_count(
			static_cast<int8_t>(selected_world));
		channel_count = real_count > 0 ? real_count : 20;

#ifdef __ANDROID__
		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Change Channel");
		hint = Text(Text::Font::A13M, Text::Alignment::CENTER, Color::Name::HUD_EXP, "");

		for (uint8_t i = 0; i < channel_count; i++)
			ch_labels.emplace_back(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Ch " + std::to_string(i + 1));

		int16_t rows_used = (channel_count + COLS - 1) / COLS;
		int16_t height = COVER_OFFSET.y() + rows_used * STRIDE_VERT + 56;
		int16_t button_y = height - 50;

		buttons[BT_CHANGE] = std::make_unique<ClassicButton>("Change", Point<int16_t>(WIDTH / 2 - 141, button_y), Point<int16_t>(132, 42));
		buttons[BT_CANCEL] = std::make_unique<ClassicButton>("Cancel", Point<int16_t>(WIDTH / 2 + 9, button_y), Point<int16_t>(132, 42));

		dimension = Point<int16_t>(WIDTH, height);
		dragarea = Point<int16_t>(0, 0);
		position = Point<int16_t>(
			(Constants::Constants::get().get_viewwidth() - WIDTH) / 2,
			(Constants::Constants::get().get_viewheight() - height) / 2
		);
#else
		nl::node source = nl::nx::ui["UIWindow2.img"]["Channel"];

		sprites.emplace_back(source["backgrnd"]);
		sprites.emplace_back(source["backgrnd2"]);
		sprites.emplace_back(source["backgrnd3"]);
		sprites.emplace_back(source["world"][selected_world], DrawArgument(Point<int16_t>(16, 30)));

		buttons[BT_CANCEL] = std::make_unique<MapleButton>(source["BtCancel"]);
		buttons[BT_CHANGE] = std::make_unique<MapleButton>(source["BtChange"]);

		channel_cover[true] = source["channel1"];
		channel_cover[false] = source["channel0"];

		nl::node ch_src = source["ch"];
		Point<int16_t> ch_pos = CH_SPRITE_OFFSET;

		for (uint8_t row = 0; row < ROWS; row++)
		{
			for (uint8_t col = 0; col < COLS; col++)
			{
				uint8_t n = row * COLS + col;

				if (n >= channel_count)
					break;

				ch_sprites.emplace_back(ch_src[n], DrawArgument(ch_pos));
				ch_pos.shift_x(STRIDE_HORIZ);
			}

			ch_pos.set_x(CH_SPRITE_OFFSET.x());
			ch_pos.shift_y(STRIDE_VERT);
		}

		dimension = Point<int16_t>(WIDTH, HEIGHT);
		dragarea = Point<int16_t>(WIDTH, DRAG_HEIGHT);
#endif
	}

	void UIChannel::draw(float inter) const
	{
#ifdef __ANDROID__
		classic::panel(position, dimension.x(), dimension.y());
		title.draw(position + Point<int16_t>(dimension.x() / 2, 6));
		hint.draw(position + Point<int16_t>(dimension.x() / 2, 34));

		int16_t cell_w = STRIDE_HORIZ - 4;
		int16_t cell_h = STRIDE_VERT - 4;

		for (size_t i = 0; i < ch_labels.size(); i++)
		{
			Point<int16_t> tl = position + COVER_OFFSET + Point<int16_t>(
				static_cast<int16_t>(i % COLS) * STRIDE_HORIZ,
				static_cast<int16_t>(i / COLS) * STRIDE_VERT
			);

			classic::cell(tl, cell_w, cell_h, i == selected_channel);
			ch_labels[i].draw(Point<int16_t>(tl.x() + cell_w / 2, classic::text_y(ch_labels[i], tl.y() + cell_h / 2)));
		}

		UIElement::draw(inter);
		return;
#endif

		UIElement::draw(inter);

		Point<int16_t> base_pos = COVER_OFFSET + position;

		// Draw current channel marker
		Point<int16_t> curr_pos = base_pos + Point<int16_t>(
			(current_channel % COLS) * STRIDE_HORIZ,
			(current_channel / COLS) * STRIDE_VERT
		);
		channel_cover[false].draw(DrawArgument(curr_pos));

		// Draw selected channel marker (if different)
		if (selected_channel != current_channel)
		{
			Point<int16_t> sel_pos = base_pos + Point<int16_t>(
				(selected_channel % COLS) * STRIDE_HORIZ,
				(selected_channel / COLS) * STRIDE_VERT
			);
			channel_cover[true].draw(DrawArgument(sel_pos));
		}

		// Draw channel number sprites
		for (auto& sprite : ch_sprites)
			sprite.draw(position, inter);
	}

	void UIChannel::update()
	{
		UIElement::update();

#ifdef __ANDROID__
		position = Point<int16_t>(
			(Constants::Constants::get().get_viewwidth() - dimension.x()) / 2,
			(Constants::Constants::get().get_viewheight() - dimension.y()) / 2
		);

		std::string now = "You are on Channel " + std::to_string(current_channel + 1);

		if (hint.get_text() != now)
			hint.change_text(now);

		for (size_t i = 0; i < ch_labels.size(); i++)
			ch_labels[i].change_color(classic::row_text(i == selected_channel, i == current_channel ? Color::Name::HUD_EXP : Color::Name::WHITE));
#endif

		for (auto& sprite : ch_sprites)
			sprite.update();
	}

	int8_t UIChannel::channel_by_pos(Point<int16_t> cursorpos) const
	{
		Point<int16_t> relative = cursorpos - position - COVER_OFFSET;

		if (relative.x() < 0 || relative.y() < 0)
			return -1;

		if (relative.x() >= COLS * STRIDE_HORIZ || relative.y() >= ROWS * STRIDE_VERT)
			return -1;

		int16_t col = relative.x() / STRIDE_HORIZ;
		int16_t row = relative.y() / STRIDE_VERT;
		uint8_t ch = row * COLS + col;

		if (ch >= channel_count)
			return -1;

		return ch;
	}

	void UIChannel::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (escape)
		{
			deactivate();
			return;
		}

		switch (keycode)
		{
		case KeyAction::Id::RETURN:
			change_channel();
			break;
		case KeyAction::Id::UP:
			if (selected_channel >= COLS)
				selected_channel -= COLS;
			else
				selected_channel += COLS * (ROWS - 1);

			if (selected_channel >= channel_count)
				selected_channel = current_channel;
			break;
		case KeyAction::Id::DOWN:
			selected_channel += COLS;

			if (selected_channel >= channel_count)
				selected_channel %= COLS;
			break;
		case KeyAction::Id::LEFT:
			if (selected_channel > 0)
				selected_channel--;
			else
				selected_channel = channel_count - 1;
			break;
		case KeyAction::Id::RIGHT:
			if (selected_channel < channel_count - 1)
				selected_channel++;
			else
				selected_channel = 0;
			break;
		}
	}

	Cursor::State UIChannel::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		Cursor::State dstate = UIDragElement::send_cursor(clicked, cursorpos);

		if (dragged)
			return dstate;

		if (clicked)
		{
			int8_t clicked_ch = channel_by_pos(cursorpos);

			// The current channel can be picked again, as a way to back out
			if (clicked_ch >= 0)
				selected_channel = clicked_ch;
		}

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UIChannel::get_type() const
	{
		return TYPE;
	}

	Button::State UIChannel::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case BT_CANCEL:
			deactivate();
			return Button::State::NORMAL;
		case BT_CHANGE:
			change_channel();
			return Button::State::NORMAL;
		default:
			return Button::State::NORMAL;
		}
	}

	namespace
	{
		using Clock = std::chrono::steady_clock;

		// The channel asked for, or -1 when no change is in flight
		int16_t pending_channel = -1;
		Clock::time_point pending_since;

		// The server answers within a second on a healthy connection; past
		// this it refused without saying so (a mini game or shop was open)
		constexpr auto PENDING_TIMEOUT = std::chrono::seconds(8);

		void tell_player(const std::string& message)
		{
			if (auto chatbar = UI::get().get_element<UIChatBar>())
				chatbar->send_line(message, UIChatBar::LineType::RED);
		}
	}

	void UIChannel::change_channel()
	{
		if (pending_channel >= 0)
		{
			deactivate();
			return;
		}

		if (selected_channel != current_channel)
		{
			pending_channel = selected_channel;
			pending_since = Clock::now();

			UI::get().disable();
			ChangeChannelPacket(selected_channel).dispatch();
		}

		deactivate();
	}

	void UIChannel::change_succeeded()
	{
		if (pending_channel < 0)
			return;

		// Persist the new channel so this window and everything else that
		// reads Configuration::get_channelid() see it after the reconnect
		Configuration::get().set_channelid(static_cast<uint8_t>(pending_channel));
		pending_channel = -1;

		if (auto channel = UI::get().get_element<UIChannel>())
		{
			channel->current_channel = Configuration::get().get_channelid();
			channel->selected_channel = channel->current_channel;
		}
	}

	void UIChannel::update_pending()
	{
		if (pending_channel < 0)
			return;

		if (Clock::now() - pending_since < PENDING_TIMEOUT)
			return;

		int16_t wanted = pending_channel;
		pending_channel = -1;

		UI::get().enable();
		tell_player("Couldn't change to Channel " + std::to_string(wanted + 1)
			+ ". Channels can't be changed while dead, in some maps, or with a shop or mini game open.");
	}
}
