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
#include "UILogin.h"

#include "UILoginNotice.h"
#include "UILoginWait.h"

#include "../UI.h"
#include "../UIScale.h"

#include "../Components/MapleButton.h"

#include "../../Graphics/ClassicStyle.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"

#include "../../Net/Packets/LoginPackets.h"

#include <cmath>
#include <fstream>
#include <random>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef USE_NX
#ifdef __ANDROID__
#include <SDL.h>
#include <sys/stat.h>
#endif

#include <fstream>

#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// The saved password. On Android the Settings file sits in shared
		// storage that the user and, on older versions, other apps can read,
		// so the password is kept in the app's private storage instead.
#ifdef __ANDROID__
		std::string private_password_path()
		{
			const char* internal = SDL_AndroidGetInternalStoragePath();

			return internal ? std::string(internal) + "/login" : std::string();
		}
#endif

		std::string load_saved_password()
		{
#ifdef __ANDROID__
			std::string path = private_password_path();
			std::string saved;

			if (!path.empty())
			{
				std::ifstream file(path, std::ios::binary);
				std::getline(file, saved);
			}

			// Move a password left in the shared Settings file by an older build.
			std::string legacy = Setting<DefaultPassword>::get().load();

			if (!legacy.empty())
			{
				if (saved.empty())
					saved = legacy;

				Setting<DefaultPassword>::get().save("");
				Configuration::get().save();
			}

			return saved;
#else
			return Setting<DefaultPassword>::get().load();
#endif
		}

		void save_password(const std::string& value)
		{
#ifdef __ANDROID__
			std::string path = private_password_path();

			if (path.empty())
				return;

			{
				std::ofstream file(path, std::ios::binary | std::ios::trunc);
				file << value;
			}

			::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#else
			Setting<DefaultPassword>::get().save(value);
#endif
		}

		// The guest login made for this phone: "id\npassword" in the app's
		// private storage. Empty when none was made yet, or off Android.
		std::string guest_path()
		{
#ifdef __ANDROID__
			const char* internal = SDL_AndroidGetInternalStoragePath();

			return internal ? std::string(internal) + "/guest" : std::string();
#else
			return std::string();
#endif
		}

		std::pair<std::string, std::string> load_guest()
		{
			std::string path = guest_path();
			std::string id;
			std::string password;

			if (!path.empty())
			{
				std::ifstream file(path, std::ios::binary);
				std::getline(file, id);
				std::getline(file, password);
			}

			return { id, password };
		}

		bool save_guest(const std::string& id, const std::string& password)
		{
			std::string path = guest_path();

			if (path.empty())
				return false;

			{
				std::ofstream file(path, std::ios::binary | std::ios::trunc);
				file << id << '\n' << password << '\n';

				if (!file)
					return false;
			}

#ifdef __ANDROID__
			::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#endif
			return true;
		}

		// Letters and digits that can't be mixed up when read off a screen
		std::string random_text(size_t length)
		{
			static constexpr char LETTERS[] = "abcdefghjkmnpqrstuvwxyz23456789";
			std::random_device device;
			std::uniform_int_distribution<size_t> pick(0, sizeof(LETTERS) - 2);
			std::string text;

			for (size_t i = 0; i < length; i++)
				text += LETTERS[pick(device)];

			return text;
		}

		// The phone sign-in card, in the 800x600 design space. The fields
		// and the Log In button are tall enough for a thumb, and the card
		// sits under the logo so the scene still shows.
		constexpr int16_t CARD_W = 320;
		constexpr int16_t CARD_X = (800 - CARD_W) / 2;
		constexpr int16_t CARD_Y = 250;
		constexpr int16_t CARD_PAD = 18;
		constexpr int16_t FIELD_H = 40;
		constexpr int16_t ACCOUNT_Y = 18;
		constexpr int16_t PASSWORD_Y = 68;
		constexpr int16_t SAVEID_Y = 118;
		constexpr int16_t SAVEID_H = 28;
		constexpr int16_t LOGIN_Y = 156;
		constexpr int16_t LOGIN_H = 44;
		constexpr int16_t GUEST_Y = 210;
		constexpr int16_t GUEST_H = 34;
		constexpr int16_t CARD_H = GUEST_Y + GUEST_H + CARD_PAD;
		// Text inside a field starts this far in, past the gem on the
		// field art's left end
		constexpr int16_t FIELD_TEXT_X = 34;
		constexpr int16_t FIELD_TEXT_H = 20;
		// The keyboard covers about the lower half of a phone held sideways,
		// so while typing the card slides up to the top of the screen.
		constexpr float TYPING_SHIFT = -(CARD_Y - 10.0f);

		bool uses_phone_layout()
		{
#ifdef __ANDROID__
			return true;
#else
			return false;
#endif
		}
	}

	UILogin::UILogin() : UIElement(Point<int16_t>(0, 0), Point<int16_t>(800, 600), ScaleMode::CENTER_OFFSET)
	{
		signboard_pos = Point<int16_t>(510, 330);
		phone_layout = uses_phone_layout();
		card_shift = 0.0f;
		cursor_was_down = false;

		LoginStartPacket().dispatch();

		std::string loginMusic = Configuration::get().get_login_music();

		Music(loginMusic).play();

		std::string version_text = Configuration::get().get_version();
		version = Text(Text::Font::A11B, Text::Alignment::LEFT, Color::Name::BLACK, "Ver. " + version_text);

		nl::node loginBackground = nl::nx::map["Back"]["login.img"];
		nl::node back = loginBackground["back"];

		nl::node loginUI = nl::nx::ui["Login.img"]["Title"];

		nl::node Login = nl::nx::map.resolve("Obj/login.img");
		nl::node signboard = nl::nx::map.resolve("Obj/login.img/Title/signboard/0/0");

		nl::node logo = Login["Title"]["logo"]["0"];
		nl::node frame = nl::nx::ui["Login.img"]["Common"]["frame"];

		sprites.emplace_back(back["11"], UIScale::bg_args());
		UI::get().set_side_backdrop(Texture(back["11"]));
		sprites.emplace_back(logo, Point<int16_t>(409, 144));

		if (!phone_layout)
			sprites.emplace_back(signboard, signboard_pos);
		// The light rays are 6 SEPARATE animated elements (effect/0..5, each its
		// own animation), layered at the same spot to build the full sunbeam.
		sprites.emplace_back(loginUI["effect"]["0"], Point<int16_t>(735, -10));
		sprites.emplace_back(loginUI["effect"]["1"], Point<int16_t>(735, -10));
		sprites.emplace_back(loginUI["effect"]["2"], Point<int16_t>(735, -10));
		sprites.emplace_back(loginUI["effect"]["3"], Point<int16_t>(735, -10));
		sprites.emplace_back(loginUI["effect"]["4"], Point<int16_t>(735, -10));
		sprites.emplace_back(loginUI["effect"]["5"], Point<int16_t>(735, -10));
		sprites.emplace_back(frame, UIScale::bg_args());


		// The phone card draws its own Log In, Play as guest and save
		// toggle. The classic signboard's other buttons open web pages,
		// which only works on Windows, so the card leaves them out.
		if (!phone_layout)
		{
			buttons[Buttons::BT_LOGIN] = std::make_unique<MapleButton>(loginUI["BtLogin"], signboard_pos + Point<int16_t>(85, -105));
			buttons[Buttons::BT_GUEST_LOGIN] = std::make_unique<MapleButton>(loginUI["BtGuestLogin"], signboard_pos + Point<int16_t>(85, -62));
			buttons[Buttons::BT_SAVEID] = std::make_unique<MapleButton>(loginUI["BtLoginIDSave"], signboard_pos + Point<int16_t>(-100, -25));
			buttons[Buttons::BT_IDLOST] = std::make_unique<MapleButton>(loginUI["BtLoginIDLost"], signboard_pos + Point<int16_t>(-20, -25));
			buttons[Buttons::BT_PASSLOST] = std::make_unique<MapleButton>(loginUI["BtPasswdLost"], signboard_pos + Point<int16_t>(70, -25));
			buttons[Buttons::BT_REGISTER] = std::make_unique<MapleButton>(loginUI["BtNew"], signboard_pos + Point<int16_t>(-120, 20));
			buttons[Buttons::BT_HOMEPAGE] = std::make_unique<MapleButton>(loginUI["BtHomePage"], signboard_pos + Point<int16_t>(-20, 20));
			buttons[Buttons::BT_QUIT] = std::make_unique<MapleButton>(loginUI["BtQuit"], signboard_pos + Point<int16_t>(80, 20));
		}

		checkbox[false] = loginUI["check"]["0"];
		checkbox[true] = loginUI["check"]["1"];

		background = ColorBox(UIScale::view_width(), UIScale::view_height(), Color::Name::BLACK, 1.0f);

		Point<int16_t> textbox_pos = signboard_pos + Point<int16_t>(-55, -95);
		Point<int16_t> textbox_dim = Point<int16_t>(150, 45);
		int16_t textbox_limit = 12;

		// On the card the text sits on light field art, so it is dark, and
		// the fields are the card's width
		Color::Name field_text = Color::Name::WHITE;
		int16_t password_step = 30;

		if (phone_layout)
		{
			field_text = Color::Name::BLACK;
			textbox_pos = Point<int16_t>(CARD_X + CARD_PAD + FIELD_TEXT_X, CARD_Y + ACCOUNT_Y + (FIELD_H - FIELD_TEXT_H) / 2);
			textbox_dim = Point<int16_t>(CARD_W - 2 * CARD_PAD - FIELD_TEXT_X - 8, FIELD_TEXT_H);
			password_step = PASSWORD_Y - ACCOUNT_Y;

			account_hint = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::DUSTYGRAY, "Login ID");
			password_hint = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::DUSTYGRAY, "Password");
			login_label = Text(Text::Font::A15B, Text::Alignment::CENTER, Color::Name::WHITE, "Log In");
			saveid_label = Text(Text::Font::A13M, Text::Alignment::LEFT, Color::Name::WHITE, "Remember me");
			guest_label = Text(Text::Font::A13B, Text::Alignment::CENTER, Color::Name::WHITE, "Play as guest");
			guest_note = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE, "");
			new_account_tip = Text(Text::Font::A12M, Text::Alignment::CENTER, Color::Name::WHITE,
				"New here? Type a new Login ID and password to make an account.");
		}

#pragma region Account
		account = Textfield(Text::Font::A13M, Text::Alignment::LEFT, field_text, Color::Name::SMALT, 0.75f, Rectangle<int16_t>(textbox_pos, textbox_pos + textbox_dim), textbox_limit);

		account.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::NORMAL);
				password.set_state(Textfield::State::FOCUSED);
			}
		);

		account.set_enter_callback
		(
			[&](std::string msg)
			{
#ifdef __ANDROID__
				// The keyboard's Enter is the only way to the next field on a phone, so
				// it must not sign in before the password has been typed.
				if (password.get_text().empty())
				{
					account.set_state(Textfield::State::NORMAL);
					password.set_state(Textfield::State::FOCUSED);
					return;
				}
#endif
				login();
			}
		);

#pragma endregion

#pragma region Password
		textbox_pos.shift_y(password_step);

		password = Textfield(Text::Font::A13M, Text::Alignment::LEFT, field_text, Color::Name::PRUSSIANBLUE, 0.85f, Rectangle<int16_t>(textbox_pos, textbox_pos + textbox_dim), textbox_limit);

		password.set_key_callback
		(
			KeyAction::Id::TAB, [&]
			{
				account.set_state(Textfield::State::FOCUSED);
				password.set_state(Textfield::State::NORMAL);
			}
		);

		password.set_enter_callback
		(
			[&](std::string msg)
			{
				login();
			}
		);

		password.set_cryptchar('*');
#pragma endregion

#ifdef __ANDROID__
		// A focused field opens the phone keyboard, which would cover the
		// sign-in board before the player has even seen it. On a phone the
		// fields start unfocused and a tap on one opens the keyboard.
		constexpr Textfield::State initial_focus = Textfield::State::NORMAL;
#else
		constexpr Textfield::State initial_focus = Textfield::State::FOCUSED;
#endif

		saveid = Setting<SaveLogin>::get().load();

		if (saveid)
		{
			account.change_text(Setting<DefaultAccount>::get().load());

			std::string saved_pass = load_saved_password();
			if (!saved_pass.empty())
			{
				password.change_text(saved_pass);
				password.set_state(Textfield::State::NORMAL);
			}
			else
			{
				password.set_state(initial_focus);
			}
		}
		else
		{
			account.set_state(initial_focus);
		}


	}

	void UILogin::draw(float alpha) const
	{
		background.draw(Point<int16_t>(0, 0));

		UIElement::draw(alpha);

		auto drawpos = get_draw_position();

		version.draw(drawpos + Point<int16_t>(707, 4));

		if (phone_layout)
		{
			draw_card(drawpos);
			return;
		}

		account.draw(drawpos);
		password.draw(drawpos);
		checkbox[saveid].draw(drawpos + signboard_pos + Point<int16_t>(-120, -25));
	}

	Point<int16_t> UILogin::card_origin(Point<int16_t> drawpos) const
	{
		return drawpos + Point<int16_t>(CARD_X, static_cast<int16_t>(CARD_Y + card_shift));
	}

	Rectangle<int16_t> UILogin::card_part(Point<int16_t> drawpos, int16_t y, int16_t h) const
	{
		Point<int16_t> tl = card_origin(drawpos) + Point<int16_t>(CARD_PAD, y);

		return Rectangle<int16_t>(tl, tl + Point<int16_t>(CARD_W - 2 * CARD_PAD, h));
	}

	void UILogin::draw_card(Point<int16_t> drawpos) const
	{
		Point<int16_t> card = card_origin(drawpos);
		Point<int16_t> field_offset(0, static_cast<int16_t>(card_shift));

		classic::panel(card, CARD_W, CARD_H);

		// The two fields: the light field art, a grey hint while empty, and
		// the typed text drawn by the Textfield itself
		auto field = [&](const Textfield& textfield, const Text& hint, int16_t y)
		{
			Rectangle<int16_t> r = card_part(drawpos, y, FIELD_H);

			if (!classic::input(r.get_left_top(), r.width(), r.height()))
			{
				classic::box(r.left(), r.top(), r.width(), r.height(), Color::Name::GALLERY, 0.97f);
				classic::frame(r.left(), r.top(), r.width(), r.height(), Color::Name::HUD_EXP);
			}

			// A disabled field hides its text while the login is sent, so
			// the hint stays away then too
			Textfield::State state = textfield.get_state();

			if (textfield.empty() && state == Textfield::State::NORMAL)
				hint.draw(Point<int16_t>(r.left() + FIELD_TEXT_X, classic::text_y(hint, r.top() + r.height() / 2)));
		};

		field(account, account_hint, ACCOUNT_Y);
		field(password, password_hint, PASSWORD_Y);

		account.draw(drawpos + field_offset);
		password.draw(drawpos + field_offset);

		// Remember me: a small cell that lights up gold when on
		Rectangle<int16_t> save = card_part(drawpos, SAVEID_Y, SAVEID_H);
		constexpr int16_t BOX = 20;
		Point<int16_t> box_tl(save.left() + 2, save.top() + (SAVEID_H - BOX) / 2);

		classic::cell(box_tl, BOX, BOX, saveid);

		if (saveid)
			classic::box(box_tl.x() + 6, box_tl.y() + 6, BOX - 12, BOX - 12, Color::Name::WHITE, 0.9f);

		saveid_label.draw(Point<int16_t>(box_tl.x() + BOX + 10, classic::text_y(saveid_label, save.top() + SAVEID_H / 2)));

		Rectangle<int16_t> login_rect = card_part(drawpos, LOGIN_Y, LOGIN_H);
		classic::button(login_rect.get_left_top(), login_rect.width(), login_rect.height());
		classic::shadowed(login_label, Point<int16_t>(login_rect.left() + login_rect.width() / 2,
			classic::text_y(login_label, login_rect.top() + login_rect.height() / 2)));

		Rectangle<int16_t> guest_rect = card_part(drawpos, GUEST_Y, GUEST_H);
		classic::cell(guest_rect.get_left_top(), guest_rect.width(), guest_rect.height(), false);
		classic::shadowed(guest_label, Point<int16_t>(guest_rect.left() + guest_rect.width() / 2,
			classic::text_y(guest_label, guest_rect.top() + guest_rect.height() / 2)));

		// The server signs up a Login ID it hasn't seen on its first login,
		// so the tip under the card stands in for the classic New button.
		// Right after a guest login is made, its details take that spot.
		// Both are hidden while typing, when the card covers their spot.
		if (card_shift > TYPING_SHIFT / 2)
		{
			const Text& tip = guest_note.get_text().empty() ? new_account_tip : guest_note;
			classic::shadowed(tip, card + Point<int16_t>(CARD_W / 2, CARD_H + 12));
		}
	}

	Cursor::State UILogin::send_card_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		bool pressed_now = clicked && !cursor_was_down;
		cursor_was_down = clicked;

		Point<int16_t> drawpos = get_draw_position();
		Rectangle<int16_t> account_rect = card_part(drawpos, ACCOUNT_Y, FIELD_H);
		Rectangle<int16_t> password_rect = card_part(drawpos, PASSWORD_Y, FIELD_H);

		// A Textfield lets go of focus on any press outside its text line, so
		// it only hears presses on that line, where it places the caret. A
		// lift always reaches both, which ends a text selection.
		if (!clicked)
		{
			account.send_cursor(cursorpos, false);
			password.send_cursor(cursorpos, false);
		}
		else if (account.get_bounds().contains(cursorpos))
		{
			return account.send_cursor(cursorpos, true);
		}
		else if (password.get_bounds().contains(cursorpos))
		{
			return password.send_cursor(cursorpos, true);
		}

		// Buttons act the moment the finger lands. The card slides when the
		// keyboard opens or closes, so by the lift it may have moved away
		// from under the finger.
		if (!pressed_now)
			return clicked ? Cursor::State::CLICKING : Cursor::State::IDLE;

		auto focus = [&](Textfield& on, Textfield& off)
		{
			if (on.get_state() == Textfield::State::DISABLED)
				return;

			off.set_state(Textfield::State::NORMAL);
			on.set_state(Textfield::State::FOCUSED);
		};

		if (account_rect.contains(cursorpos))
		{
			focus(account, password);
			return Cursor::State::CLICKING;
		}

		if (password_rect.contains(cursorpos))
		{
			focus(password, account);
			return Cursor::State::CLICKING;
		}

		if (card_part(drawpos, SAVEID_Y, SAVEID_H).contains(cursorpos))
		{
			toggle_saveid();
			return Cursor::State::CLICKING;
		}

		if (card_part(drawpos, LOGIN_Y, LOGIN_H).contains(cursorpos))
		{
			if (account.get_state() != Textfield::State::DISABLED)
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				login();
			}

			return Cursor::State::CLICKING;
		}

		if (card_part(drawpos, GUEST_Y, GUEST_H).contains(cursorpos))
		{
			if (account.get_state() != Textfield::State::DISABLED)
			{
				Sound(Sound::Name::BUTTONCLICK).play();
				guest_login();
			}

			return Cursor::State::CLICKING;
		}

		// A tap on the scene puts the keyboard away
		if (account.get_state() == Textfield::State::FOCUSED)
			account.set_state(Textfield::State::NORMAL);

		if (password.get_state() == Textfield::State::FOCUSED)
			password.set_state(Textfield::State::NORMAL);

		UI::get().remove_textfield();

		return Cursor::State::CLICKING;
	}

	void UILogin::guest_login()
	{
		// Each phone gets its own guest account: the first tap makes a
		// random Login ID and password and keeps them on the phone, and the
		// server signs the new ID up on its first login. A fixed "guest"
		// account would put every player into the same characters.
		auto [id, guest_password] = load_guest();

		if (id.empty() || guest_password.empty())
		{
			id = "guest" + random_text(6);
			guest_password = random_text(8);

			if (!save_guest(id, guest_password))
			{
				guest_note.change_text("This phone's storage isn't available, so no guest login could be made.");
				return;
			}

			// Shown before the first sign in, so the player can note the
			// details down and play the same account on another device
			guest_note.change_text("Your guest login: " + id + " / " + guest_password + ". Note it down, then tap Play as guest.");
			return;
		}

		guest_note.change_text("");
		account.change_text(id);
		password.change_text(guest_password);
		login();
	}

	void UILogin::toggle_saveid()
	{
		saveid = !saveid;

		// Turning "save login" off forgets the saved password.
		if (!saveid)
			save_password("");

		Setting<SaveLogin>::get().save(saveid);
		Configuration::get().save();
	}

	void UILogin::update()
	{
		// if (Configuration::get().get_auto_login())
		// if (true)
		// {
		// 	UI::get().emplace<UILoginWait>([]() {});

		// 	auto loginwait = UI::get().get_element<UILoginWait>();

		// 	if (loginwait && loginwait->is_active())
		// 		LoginPacket(
		// 			Configuration::get().get_auto_acc(),
		// 			Configuration::get().get_auto_pass()
		// 		).dispatch();
		// }

		UIElement::update();

		Point<int16_t> drawpos = get_draw_position();

		if (phone_layout)
		{
			// Glide toward the spot above the keyboard while it is open, and
			// back under the logo when it closes
			float target = UI::get().is_typing_on_top() ? TYPING_SHIFT : 0.0f;
			card_shift += (target - card_shift) * 0.3f;

			if (std::abs(target - card_shift) < 0.5f)
				card_shift = target;

			drawpos.shift_y(static_cast<int16_t>(card_shift));
		}

		account.update(drawpos);
		password.update(drawpos);
	}

	void UILogin::login()
	{
		account.set_state(Textfield::State::DISABLED);
		password.set_state(Textfield::State::DISABLED);

		std::string account_text = account.get_text();
		std::string password_text = password.get_text();

		if(Configuration::get().get_auto_login()){
			account_text = Configuration::get().get_auto_acc();
			password_text = Configuration::get().get_auto_pass();
		}

		std::function<void()> okhandler = [&, password_text]()
		{
			account.set_state(Textfield::State::NORMAL);
			password.set_state(Textfield::State::NORMAL);

			if (!password_text.empty())
				password.set_state(Textfield::State::FOCUSED);
			else
				account.set_state(Textfield::State::FOCUSED);
		};

		if (account_text.empty())
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::NOT_REGISTERED, okhandler);
			return;
		}

		if (password_text.length() <= 4)
		{
			UI::get().emplace<UILoginNotice>(UILoginNotice::Message::WRONG_PASSWORD, okhandler);
			return;
		}

		if (saveid)
		{
			Setting<DefaultAccount>::get().save(account_text);
			save_password(password_text);
			Configuration::get().save();
		}

		UI::get().emplace<UILoginWait>(okhandler);

		auto loginwait = UI::get().get_element<UILoginWait>();

		if (loginwait && loginwait->is_active())
			LoginPacket(account_text, password_text).dispatch();
	}

	void UILogin::open_url(uint16_t id)
	{
		std::string url;

		switch (id)
		{
			case Buttons::BT_REGISTER:
				url = Configuration::get().get_joinlink();
				break;
			case Buttons::BT_HOMEPAGE:
				url = Configuration::get().get_website();
				break;
			case Buttons::BT_PASSLOST:
				url = Configuration::get().get_findpass();
				break;
			case Buttons::BT_IDLOST:
				url = Configuration::get().get_findid();
				break;
			default:
				return;
		}

#ifdef _WIN32
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
	}

	Button::State UILogin::button_pressed(uint16_t id)
	{
		switch (id)
		{
			case Buttons::BT_LOGIN:
			{
				login();

				return Button::State::NORMAL;
			}
			case Buttons::BT_GUEST_LOGIN:
			{
				// No dedicated guest protocol on this server, so log in with a
				// fixed guest account. Create a "guest" account on Cosmic (pw
				// "guest") for this to succeed; otherwise the server replies
				// "not registered".
				account.change_text("guest");
				password.change_text("guest");
				login();

				return Button::State::NORMAL;
			}
			case Buttons::BT_REGISTER:
			case Buttons::BT_HOMEPAGE:
			case Buttons::BT_PASSLOST:
			case Buttons::BT_IDLOST:
			{
				open_url(id);

				return Button::State::NORMAL;
			}
			case Buttons::BT_SAVEID:
			{
				toggle_saveid();

				return Button::State::MOUSEOVER;
			}
			case Buttons::BT_QUIT:
			{
				UI::get().quit();

				return Button::State::PRESSED;
			}
			default:
			{
				return Button::State::DISABLED;
			}
		}
	}

	Cursor::State UILogin::send_cursor(bool clicked, Point<int16_t> cursorpos)
	{
		if (phone_layout)
			return send_card_cursor(clicked, cursorpos);

		if (Cursor::State new_state = account.send_cursor(cursorpos, clicked))
			return new_state;

		if (Cursor::State new_state = password.send_cursor(cursorpos, clicked))
			return new_state;

		return UIElement::send_cursor(clicked, cursorpos);
	}

	UIElement::Type UILogin::get_type() const
	{
		return TYPE;
	}
}