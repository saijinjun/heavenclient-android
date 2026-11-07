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
#include "UINotice.h"

#include "../NotificationCenter.h"
#include "UIStatusBar.h"

#include "../UI.h"

#include "../Components/ClassicButton.h"
#include "../Components/MapleButton.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Graphics/ClassicStyle.h"

#include <algorithm>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	UINotice::UINotice(std::string message, NoticeType t, Text::Alignment a) : UIDragElement<PosNOTICE>(Point<int16_t>(250, 20)), type(t), alignment(a)
	{
		nl::node src = nl::nx::ui["Basic.img"]["Notice6"];

		top = src["t"];
		center = src["c"];
		centerbox = src["c_box"];
		box = src["box"];
		box2 = src["box2"];
		bottom = src["s"];
		bottombox = src["s_box"];

		if (type == NoticeType::YESNO)
		{
			question = Text(Text::Font::A11M, alignment, Color::Name::WHITE, message, 200);
		}
		else if (type == NoticeType::ENTERNUMBER)
		{
			question = Text(Text::Font::A12M, Text::Alignment::LEFT, Color::Name::WHITE, message, 200);
		}
		else if (type == NoticeType::OK)
		{
			uint16_t maxwidth = top.width() - 6;
			question = Text(Text::Font::A11M, Text::Alignment::CENTER, Color::Name::WHITE, message, maxwidth);
		}

		height = question.height();
		dimension = Point<int16_t>(top.width(), top.height() + height + bottom.height());

		// Settings store the top-left corner already (that's what
		// UIDragElement saves). Re-centring on every open caused the
		// popup to drift left and up each time it was reopened.
		dragarea = Point<int16_t>(dimension.x(), 20);

		if (touch())
			touch_layout(message);

		if (type != NoticeType::ENTERNUMBER)
			Sound(Sound::Name::DLGNOTICE).play();
	}

	namespace
	{
		// The touch layout, in view pixels (about 1.8 screen pixels each on
		// a 1080-high phone, so a 40-high button is about 13 mm)
		constexpr int16_t TOUCH_WIDTH = 420;
		constexpr int16_t TOUCH_PAD = 22;
		constexpr int16_t TOUCH_MSG_Y = 40;
		constexpr int16_t TOUCH_FIELD_H = 38;
		constexpr int16_t TOUCH_BUTTON_W = 132;
		constexpr int16_t TOUCH_BUTTON_H = 42;
		constexpr int16_t TOUCH_BUTTON_GAP = 18;
		constexpr int16_t TOUCH_INVITE_Y = 44;
	}

	bool UINotice::touch()
	{
#ifdef __ANDROID__
		return true;
#else
		return false;
#endif
	}

	void UINotice::touch_layout(const std::string& message)
	{
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t width = std::min<int16_t>(TOUCH_WIDTH, vw - 32);
		uint16_t text_width = static_cast<uint16_t>(width - TOUCH_PAD * 2);

		// Left-aligned questions (long NPC-style prompts) stay left-aligned
		Text::Alignment align = type == NoticeType::YESNO && alignment == Text::Alignment::LEFT
			? Text::Alignment::LEFT : Text::Alignment::CENTER;

		question = Text(Text::Font::A13M, align, Color::Name::WHITE, message, text_width);
		height = question.height();

		const char* title = "Notice";

		if (type == NoticeType::YESNO)
			title = "Confirm";
		else if (type == NoticeType::ENTERNUMBER)
			title = "Enter an amount";

		touch_title_text = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, title);

		bool field = type == NoticeType::ENTERNUMBER;
		int16_t text_h = std::max<int16_t>(height, 22);

		touch_msg_y = TOUCH_MSG_Y;
		touch_field_y = touch_msg_y + text_h + 14;
		touch_buttons_y = field ? touch_field_y + TOUCH_FIELD_H + 18 : touch_msg_y + text_h + 20;

		dimension = Point<int16_t>(width, touch_buttons_y + TOUCH_BUTTON_H + 20);

		// Fixed spot, not dragged: centred, or at the top for a text field,
		// so the phone keyboard (bottom half) leaves the box and OK visible
		int16_t y = field ? 10 : (vh - dimension.y()) / 2 - 30;
		position = Point<int16_t>((vw - width) / 2, std::max<int16_t>(y, 10));
		dragarea = Point<int16_t>(0, 0);
	}

	Rectangle<int16_t> UINotice::touch_field() const
	{
		return Rectangle<int16_t>(
			Point<int16_t>(TOUCH_PAD, touch_field_y),
			Point<int16_t>(dimension.x() - TOUCH_PAD, touch_field_y + TOUCH_FIELD_H));
	}

	Point<int16_t> UINotice::touch_button(size_t index, size_t count) const
	{
		int16_t n = static_cast<int16_t>(count);
		int16_t row = n * TOUCH_BUTTON_W + (n - 1) * TOUCH_BUTTON_GAP;
		int16_t x = (dimension.x() - row) / 2 + static_cast<int16_t>(index) * (TOUCH_BUTTON_W + TOUCH_BUTTON_GAP);

		return Point<int16_t>(x, touch_buttons_y);
	}

	Point<int16_t> UINotice::touch_button_size()
	{
		return Point<int16_t>(TOUCH_BUTTON_W, TOUCH_BUTTON_H);
	}

	void UINotice::set_touch_title(const std::string& title)
	{
		touch_title_text.change_text(title);
	}

	void UINotice::touch_without_buttons()
	{
		if (touch())
			dimension = Point<int16_t>(dimension.x(), touch_buttons_y);
	}

	UINotice::UINotice(std::string message, NoticeType t) : UINotice(message, t, Text::Alignment::CENTER) {}

	void UINotice::draw(bool textfield) const
	{
		if (touch())
		{
			classic::panel(position, dimension.x(), dimension.y());
			touch_title_text.draw(position + Point<int16_t>(dimension.x() / 2, 6));

			int16_t text_x = question_left_aligned() ? TOUCH_PAD : dimension.x() / 2;
			question.draw(position + Point<int16_t>(text_x, touch_msg_y));

			if (textfield)
			{
				Rectangle<int16_t> f = touch_field();
				Point<int16_t> tl = position + f.get_left_top();

				// A light field in a gold rim, like the chat line
				if (!classic::input(tl, f.width(), f.height()))
				{
					classic::box(tl.x(), tl.y(), f.width(), f.height(), Color::Name::GALLERY, 0.97f);
					classic::frame(tl.x(), tl.y(), f.width(), f.height(), Color::Name::HUD_EXP);
				}
			}

			return;
		}

		Point<int16_t> start = position;

		top.draw(start);
		start.shift_y(top.height());

		if (textfield)
		{
			center.draw(start);
			start.shift_y(center.height());
			centerbox.draw(start);
			start.shift_y(centerbox.height() - 1);
			box2.draw(start);
			start.shift_y(box2.height());
			box.draw(DrawArgument(start, Point<int16_t>(0, 29)));
			start.shift_y(29);

			question.draw(position + Point<int16_t>(13, 8));
		}
		else
		{
			int16_t pos_y = height >= 32 ? height : 32;

			center.draw(DrawArgument(start, Point<int16_t>(0, pos_y)));
			start.shift_y(pos_y);
			centerbox.draw(start);
			start.shift_y(centerbox.height());
			box.draw(start);
			start.shift_y(box.height());

			if (type == NoticeType::YESNO && alignment == Text::Alignment::LEFT)
				question.draw(position + Point<int16_t>(31, 14));
			else
				question.draw(position + Point<int16_t>(131, 14));
		}

		bottombox.draw(start);
	}

	bool UINotice::question_left_aligned() const
	{
		return type == NoticeType::YESNO && alignment == Text::Alignment::LEFT;
	}

	int16_t UINotice::box2offset(bool textfield) const
	{
		int16_t offset = top.height() + centerbox.height() + box.height() + height - (textfield ? 0 : 16);

		if (type == NoticeType::OK)
			if (height < 34)
				offset += 15;

		return offset;
	}

	UIYesNo::UIYesNo(std::string message, std::function<void(bool yes)> yh, Text::Alignment alignment,
		int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::YESNO, alignment)
	{
		yesnohandler = yh;

		int16_t belowtext = box2offset(false) + button_y_offset;

		nl::node src = nl::nx::ui["Basic.img"];

		if (touch())
		{
			buttons[Buttons::YES] = std::make_unique<ClassicButton>("OK", touch_button(0, 2), touch_button_size());
			buttons[Buttons::NO] = std::make_unique<ClassicButton>("Cancel", touch_button(1, 2), touch_button_size());
			return;
		}

		buttons[Buttons::YES] = std::make_unique<MapleButton>(src["BtOK4"], Point<int16_t>(156 + button_x_offset, belowtext));
		buttons[Buttons::NO] = std::make_unique<MapleButton>(src["BtCancel4"], Point<int16_t>(198 + button_x_offset, belowtext));
	}

	UIYesNo::UIYesNo(std::string message, std::function<void(bool yes)> yesnohandler) : UIYesNo(message, yesnohandler, Text::Alignment::CENTER) {}

	void UIYesNo::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIYesNo::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (!pressed)
			return;

		if (keycode == KeyAction::Id::RETURN)
		{
			yesnohandler(true);
			deactivate();
		}
		else if (escape)
		{
			yesnohandler(false);
			deactivate();
		}
	}

	UIElement::Type UIYesNo::get_type() const
	{
		return TYPE;
	}

	Button::State UIYesNo::button_pressed(uint16_t buttonid)
	{
		deactivate();

		switch (buttonid)
		{
		case Buttons::YES:
			yesnohandler(true);
			break;
		case Buttons::NO:
			yesnohandler(false);
			break;
		}

		return Button::State::PRESSED;
	}

	UIEnterNumber::UIEnterNumber(std::string message, std::function<void(int32_t)> nh, int32_t m, int32_t quantity,
		int16_t field_y_offset, int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::ENTERNUMBER)
	{
		numhandler = nh;
		max = m;

		int16_t belowtext = box2offset(true) + field_y_offset;
		int16_t pos_y = belowtext + 35 + button_y_offset;

		nl::node src = nl::nx::ui["Basic.img"];

		if (touch())
		{
			buttons[Buttons::OK] = std::make_unique<ClassicButton>("OK", touch_button(0, 2), touch_button_size());
			buttons[Buttons::CANCEL] = std::make_unique<ClassicButton>("Cancel", touch_button(1, 2), touch_button_size());

			Rectangle<int16_t> f = touch_field();
			int16_t top = static_cast<int16_t>(f.top() + (f.height() - 20) / 2);
			numfield = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::BLACK,
				Rectangle<int16_t>(static_cast<int16_t>(f.left() + 30), static_cast<int16_t>(f.right() - 12), top, static_cast<int16_t>(top + 20)), 10);
		}
		else
		{
			buttons[Buttons::OK] = std::make_unique<MapleButton>(src["BtOK4"], 156 + button_x_offset, pos_y);
			buttons[Buttons::CANCEL] = std::make_unique<MapleButton>(src["BtCancel4"], 198 + button_x_offset, pos_y);

			numfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY, Rectangle<int16_t>(24, 232, belowtext, belowtext + 20), 10);
		}

		numfield.change_text(std::to_string(quantity));

		// Typing replaces the starting quantity: with the caret after it,
		// entering 2 in a shop's "How many" box bought 12
		numfield.select_all();

		numfield.set_enter_callback(
			[&](std::string numstr)
			{
				handlestring(numstr);
			}
		);

		numfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				numfield.set_state(Textfield::State::DISABLED);
				deactivate();
			}
		);

		numfield.set_state(Textfield::State::FOCUSED);
	}

	void UIEnterNumber::draw(float alpha) const
	{
		UINotice::draw(true);
		UIElement::draw(alpha);

		numfield.draw(position);
	}

	void UIEnterNumber::update()
	{
		UIElement::update();

		numfield.update(position);
	}

	Cursor::State UIEnterNumber::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (numfield.get_state() == Textfield::State::NORMAL)
		{
			Cursor::State nstate = numfield.send_cursor(cursorpos, clicked);

			if (nstate != Cursor::State::IDLE)
				return nstate;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIEnterNumber::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (keycode == KeyAction::Id::RETURN)
		{
			handlestring(numfield.get_text());
			numfield.set_state(Textfield::State::DISABLED);
			deactivate();
		}
		else if (escape)
		{
			numfield.set_state(Textfield::State::DISABLED);
			deactivate();
		}
	}

	UIElement::Type UIEnterNumber::get_type() const
	{
		return TYPE;
	}

	Button::State UIEnterNumber::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::OK:
			handlestring(numfield.get_text());
			break;
		case Buttons::CANCEL:
			numfield.set_state(Textfield::State::DISABLED);
			deactivate();
			break;
		}

		return Button::State::NORMAL;
	}

	void UIEnterNumber::handlestring(std::string numstr)
	{
		int num = -1;
		bool has_only_digits = (numstr.find_first_not_of("0123456789") == std::string::npos);

		// okhandler must NOT capture `this` — UIOk and UIEnterNumber
		// share Type::NOTICE, so `emplace<UIOk>` destroys this very
		// window. Any reference to `numfield` / `buttons` in the
		// callback would fire against a freed object → crash on click.
		auto okhandler = [](bool) {};

		if (!has_only_digits)
		{
			UI::get().emplace<UIOk>("Only numbers are allowed.", okhandler);
			return;
		}
		else
		{
			num = std::stoi(numstr);
		}

		if (num < 1)
		{
			UI::get().emplace<UIOk>("You may only enter a number equal to or higher than 1.", okhandler);
			return;
		}
		else if (num > max)
		{
			UI::get().emplace<UIOk>("You may only enter a number equal to or lower than " + std::to_string(max) + ".", okhandler);
			return;
		}
		else
		{
			numhandler(num);
			numfield.set_state(Textfield::State::DISABLED);
			deactivate();
		}

		buttons[Buttons::OK]->set_state(Button::State::NORMAL);
	}

	UIEnterText::UIEnterText(std::string message, std::function<void(const std::string&)> th, int32_t maxlength,
		int16_t field_y_offset, int16_t button_x_offset, int16_t button_y_offset) : UINotice(message, NoticeType::ENTERNUMBER)
	{
		texthandler = th;

		int16_t belowtext = box2offset(true) - 21 + field_y_offset;
		int16_t pos_y = belowtext + 35 + button_y_offset;

		nl::node src = nl::nx::ui["Basic.img"];

		if (touch())
		{
			set_touch_title("Enter text");

			buttons[Buttons::OK] = std::make_unique<ClassicButton>("OK", touch_button(0, 2), touch_button_size());
			buttons[Buttons::CANCEL] = std::make_unique<ClassicButton>("Cancel", touch_button(1, 2), touch_button_size());

			Rectangle<int16_t> f = touch_field();
			int16_t top = static_cast<int16_t>(f.top() + (f.height() - 20) / 2);
			textfield = Textfield(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::BLACK,
				Rectangle<int16_t>(static_cast<int16_t>(f.left() + 30), static_cast<int16_t>(f.right() - 12), top, static_cast<int16_t>(top + 20)), maxlength);
		}
		else
		{
			buttons[Buttons::OK]     = std::make_unique<MapleButton>(src["BtOK4"],     156 + button_x_offset, pos_y);
			buttons[Buttons::CANCEL] = std::make_unique<MapleButton>(src["BtCancel4"], 198 + button_x_offset, pos_y);

			textfield = Textfield(Text::Font::A11M, Text::Alignment::LEFT, Color::Name::LIGHTGREY, Rectangle<int16_t>(24, 232, belowtext, belowtext + 20), maxlength);
		}

		textfield.set_enter_callback(
			[&](std::string str)
			{
				if (!str.empty())
				{
					texthandler(str);
					textfield.set_state(Textfield::State::DISABLED);
					deactivate();
				}
			}
		);

		textfield.set_key_callback(
			KeyAction::Id::ESCAPE,
			[&]()
			{
				textfield.set_state(Textfield::State::DISABLED);
				deactivate();
			}
		);

		textfield.set_state(Textfield::State::FOCUSED);
	}

	void UIEnterText::draw(float alpha) const
	{
		UINotice::draw(true);
		UIElement::draw(alpha);

		textfield.draw(position);
	}

	void UIEnterText::update()
	{
		UIElement::update();

		textfield.update(position);
	}

	Cursor::State UIEnterText::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (textfield.get_state() == Textfield::State::NORMAL)
		{
			Cursor::State nstate = textfield.send_cursor(cursorpos, clicked);

			if (nstate != Cursor::State::IDLE)
				return nstate;
		}

		return UIDragElement::send_cursor(clicked, cursorpos);
	}

	void UIEnterText::send_key(int32_t keycode, bool pressed, bool escape)
	{
		// The old body had no braces, so deactivate() ran on every key event (even a key release)
		// and closed the box with its text field still focused, which left the phone keyboard up.
		// Enter is handled by the text field's own enter callback.
		if (pressed && escape)
		{
			textfield.set_state(Textfield::State::DISABLED);
			deactivate();
		}
	}

	UIElement::Type UIEnterText::get_type() const
	{
		return TYPE;
	}

	Button::State UIEnterText::button_pressed(uint16_t buttonid)
	{
		switch (buttonid)
		{
		case Buttons::OK:
		{
			std::string str = textfield.get_text();
			if (!str.empty())
			{
				texthandler(str);
				textfield.set_state(Textfield::State::DISABLED);
				deactivate();
			}
			break;
		}
		case Buttons::CANCEL:
			textfield.set_state(Textfield::State::DISABLED);
			deactivate();
			break;
		}

		return Button::State::NORMAL;
	}

	// ---------- UIWaitNotice ----------

	UIWaitNotice::UIWaitNotice(std::string message) : UINotice(message, NoticeType::OK)
	{
		// No buttons — purely informational. The trade/quest handler
		// will UI::remove() us when the pending state resolves.
		set_touch_title("Please wait");
		touch_without_buttons();
	}

	void UIWaitNotice::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIWaitNotice::send_key(int32_t, bool, bool) { }
	UIElement::Type UIWaitNotice::get_type() const { return TYPE; }

	UIOk::UIOk(std::string message, std::function<void(bool ok)> oh) : UINotice(message, NoticeType::OK)
	{
		okhandler = oh;

		nl::node src = nl::nx::ui["Basic.img"];

		if (touch())
			buttons[Buttons::OK] = std::make_unique<ClassicButton>("OK", touch_button(0, 1), touch_button_size());
		else
			buttons[Buttons::OK] = std::make_unique<MapleButton>(src["BtOK4"], 197, box2offset(false));
	}

	void UIOk::draw(float alpha) const
	{
		UINotice::draw(false);
		UIElement::draw(alpha);
	}

	void UIOk::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed)
		{
			if (keycode == KeyAction::Id::RETURN)
			{
				okhandler(true);
				deactivate();
			}
			else if (escape)
			{
				okhandler(false);
				deactivate();
			}
		}
	}

	UIElement::Type UIOk::get_type() const
	{
		return TYPE;
	}

	Button::State UIOk::button_pressed(uint16_t buttonid)
	{
		deactivate();

		switch (buttonid)
		{
		case Buttons::OK:
			okhandler(true);
			break;
		}

		return Button::State::NORMAL;
	}

	// ---------- UIAlarmInvite ----------

	UIAlarmInvite::UIAlarmInvite(std::string message, std::function<void(bool yes)> h, int variant)
		: UIDragElement<PosNOTICE>(Point<int16_t>(0, 0))
	{
		handler = h;

		nl::node root = nl::nx::ui["UIWindow.img"]["FadeYesNo"];

		// Variant 0 = "backgrnd"; 1..11 = "backgrnd1".."backgrnd11".
		std::string bg_name = (variant == 0) ? "backgrnd"
			: "backgrnd" + std::to_string(variant);
		backdrop = Texture(root[bg_name]);
		if (!backdrop.is_valid())
			backdrop = Texture(root["backgrnd"]);

		icon = Texture(root["icon" + std::to_string(variant)]);
		if (!icon.is_valid())
			icon = Texture(root["icon0"]);

		Point<int16_t> dims = backdrop.get_dimensions();
		dimension = dims;
		dragarea  = Point<int16_t>(dims.x(), 20);

		// Anchor above the notice button, clamped to the viewport.
		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t vh = Constants::Constants::get().get_viewheight();
		int16_t notice_x = 512 + 56;
		int16_t anchor_x = notice_x - dims.x() / 2 - 75;
		if (anchor_x < 4) anchor_x = 4;
		if (anchor_x + dims.x() > vw - 4) anchor_x = vw - dims.x() - 4;
		int16_t anchor_y = vh - dims.y() - 60;           // 10px down
		position = Point<int16_t>(anchor_x, anchor_y);

		// Text next to the icon (icon on the left, text to its right).
		int16_t icon_w = icon.is_valid() ? icon.get_dimensions().x() : 18;
		int16_t text_x = 6 + icon_w + 6;
		int16_t text_area = dims.x() - text_x - 6;
		message_text = message;
		label = Text(Text::Font::A11M, Text::Alignment::LEFT,
			Color::Name::WHITE, message, text_area);

		// OK above Cancel, stacked on the right edge.
		buttons[Buttons::ACCEPT]  = std::make_unique<MapleButton>(
			root["BtOK"],     Point<int16_t>(dims.x() - 16, dims.y() / 2 - 19));
		buttons[Buttons::DECLINE] = std::make_unique<MapleButton>(
			root["BtCancel"], Point<int16_t>(dims.x() - 16, dims.y() / 2 - 3));

#ifdef __ANDROID__
		touch_layout(message);
#endif

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIAlarmInvite::touch_layout(const std::string& message)
	{
		// The NX strip and its two tiny buttons are drawn for a mouse pointer.
		// A wrong tap here joins a party or a trade, so a phone gets buttons
		// the same size as every other dialog's.
		finger_sized = true;

		int16_t vw = Constants::Constants::get().get_viewwidth();
		int16_t width = std::min<int16_t>(TOUCH_WIDTH, vw - 32);

		title = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Invite");
		label = Text(Text::Font::A13M, Text::Alignment::CENTER, Color::Name::WHITE, message,
			static_cast<uint16_t>(width - TOUCH_PAD * 2));

		int16_t buttons_y = TOUCH_MSG_Y + std::max<int16_t>(label.height(), 22) + 20;

		dimension = Point<int16_t>(width, buttons_y + TOUCH_BUTTON_H + 20);
		dragarea = Point<int16_t>(0, 0);

		// At the top, between the minimap and the quest list: an invite can
		// arrive mid-fight, and this keeps the character and the controls clear.
		position = Point<int16_t>((vw - width) / 2, TOUCH_INVITE_Y);

		int16_t row = TOUCH_BUTTON_W * 2 + TOUCH_BUTTON_GAP;
		int16_t left = (width - row) / 2;
		Point<int16_t> size(TOUCH_BUTTON_W, TOUCH_BUTTON_H);

		buttons[Buttons::ACCEPT] = std::make_unique<ClassicButton>(
			"Accept", Point<int16_t>(left, buttons_y), size);
		buttons[Buttons::DECLINE] = std::make_unique<ClassicButton>(
			"Decline", Point<int16_t>(left + TOUCH_BUTTON_W + TOUCH_BUTTON_GAP, buttons_y), size);
	}

	void UIAlarmInvite::draw(float alpha) const
	{
		if (finger_sized)
		{
			classic::panel(position, dimension.x(), dimension.y());
			title.draw(position + Point<int16_t>(dimension.x() / 2, 6));
			label.draw(position + Point<int16_t>(dimension.x() / 2, TOUCH_MSG_Y));
			UIElement::draw(alpha);
			return;
		}

		// Fade in over the first 300ms, fade out over the last 600ms
		// before the popup stashes itself into the drawer.
		float a = 1.0f;

		if (age_ms < 300)
			a = age_ms / 300.0f;
		else if (age_ms > HOLD_MS - 600)
			a = (HOLD_MS - age_ms) / 600.0f;

		if (a < 0.0f) a = 0.0f;
		if (a > 1.0f) a = 1.0f;

		backdrop.draw(DrawArgument(position, a));
		if (icon.is_valid())
			icon.draw(DrawArgument(position + Point<int16_t>(6, 6), a));

		int16_t icon_w = icon.is_valid() ? icon.get_dimensions().x() : 18;
		label.draw(DrawArgument(position + Point<int16_t>(6 + icon_w + 6, 6), a));

		UIElement::draw(alpha);
	}

	void UIAlarmInvite::update()
	{
		UIElement::update();

		age_ms += Constants::TIMESTEP;

		if (age_ms >= HOLD_MS && !stashed)
			stash();
	}

	void UIAlarmInvite::stash()
	{
		if (stashed)
			return;

		stashed = true;
		NotificationCenter::get().push("Invite", message_text, handler);

		if (auto statusbar = UI::get().get_element<UIStatusBar>())
			statusbar->notify();

		deactivate();
	}

	void UIAlarmInvite::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && escape)
		{
			handler(false);
			deactivate();
		}
	}

	UIElement::Type UIAlarmInvite::get_type() const { return TYPE; }

	Button::State UIAlarmInvite::button_pressed(uint16_t id)
	{
		switch (id)
		{
		case Buttons::ACCEPT:
			handler(true);
			deactivate();
			break;
		case Buttons::DECLINE:
			handler(false);
			deactivate();
			break;
		}
		return Button::State::NORMAL;
	}

	// ---------- UIDeathNotice ----------

	UIDeathNotice::UIDeathNotice(std::string message, std::function<void(bool ok)> oh)
		: UIDragElement<PosNOTICE>(Point<int16_t>(0, 0))
	{
		okhandler = oh;

		nl::node notice_root = nl::nx::ui["UIWindow.img"]["Notice"];
		backdrop = Texture(notice_root["0"]);
		dimension = backdrop.get_dimensions();
		dragarea = Point<int16_t>(dimension.x(), 20);

		question = Text(Text::Font::A12M, Text::Alignment::CENTER,
			Color::Name::WHITE, message, dimension.x() - 30);

		// OK button — reuse the Basic.img/BtOK4 sprite. Place it centered
		// near the bottom of the 286x146 frame.
		nl::node basic = nl::nx::ui["Basic.img"];
		buttons[Buttons::OK] = std::make_unique<MapleButton>(
			basic["BtOK4"],
			Point<int16_t>(dimension.x() / 2 - 30, dimension.y() - 32));

		Sound(Sound::Name::DLGNOTICE).play();
	}

	void UIDeathNotice::draw(float alpha) const
	{
		// The UIWindow.img/Notice tombstone sprite already has its message
		// baked in, so we don't draw our own text over it.
		backdrop.draw(DrawArgument(position));
		UIElement::draw(alpha);
	}

	void UIDeathNotice::send_key(int32_t keycode, bool pressed, bool escape)
	{
		if (pressed && (keycode == KeyAction::Id::RETURN || escape))
		{
			okhandler(true);
			deactivate();
		}
	}

	UIElement::Type UIDeathNotice::get_type() const
	{
		return TYPE;
	}

	Button::State UIDeathNotice::button_pressed(uint16_t buttonid)
	{
		if (buttonid == Buttons::OK)
		{
			okhandler(true);
			deactivate();
		}
		return Button::State::NORMAL;
	}
}