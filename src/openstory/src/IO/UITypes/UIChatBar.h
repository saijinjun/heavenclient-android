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
#include "../Messages.h"

#include "../Components/Slider.h"
#include "../Components/TapGesture.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Geometry.h"
#include "../../Graphics/Texture.h"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace ms
{
	class UIChatBar : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::CHATBAR;
		static constexpr bool FOCUSED = false;
		static constexpr bool TOGGLED = true;

		enum ChatTarget : uint8_t
		{
			CHT_ALL,
			CHT_BUDDY,
			CHT_GUILD,
			CHT_ALLIANCE,
			CHT_PARTY,
			CHT_SQUAD,
			NUM_TARGETS
		};

		enum LineType
		{
			UNK0,
			WHITE,
			RED,
			BLUE,
			YELLOW,
			PINK,
			LIGHTBLUE,
			GREEN,
			// Channel-chat colors mandated by the v83 protocol —
			// Buddy chat reads orange, Alliance chat reads as a soft
			// light-green band; both are distinct from PINK (party)
			// and GREEN (guild).
			ORANGE,
			LIGHTGREEN,
			// Dedicated megaphone line styles — render a tiny megaphone
			// icon (UIWindow.img/Megaphone/0..3) before the text so these
			// broadcasts are visually distinct from regular chat.
			MEGAPHONE,        // Cheap / Regular  (5070 / 5071) — icon 0
			SUPER_MEGAPHONE,  // Super            (5072)        — icon 1
			ITEM_MEGAPHONE,   // Item             (5076)        — icon 2
			TRIPLE_MEGAPHONE  // Triple           (5077)        — icon 3
		};

		struct PartyMember
		{
			int32_t id = 0;
			std::string name;
			int32_t job_id = 0;
			int32_t level = 0;
			int32_t channel = -2;
			int32_t map_id = 0;
			int32_t hp = 0;
			int32_t max_hp = 0;
		};

		UIChatBar(Point<int16_t> position);

		void draw(float inter) const override;
		void update() override;
		void update_screen(int16_t new_width, int16_t new_height) override;
		void set_position(Point<int16_t> pos);

		void send_key(int32_t keycode, bool pressed, bool escape) override;
		void send_scroll(double yoffset) override;

		bool is_in_range(Point<int16_t> cursorpos) const override;

		// Whether a touch here is for the chat bar on a phone, not for the
		// touch controls or the world
		bool touch_hit(Point<int16_t> cursorpos) const;

		// Windows-style chat-log text selection (drag to highlight, Ctrl+C to copy).
		std::string get_selected_text() const;
		void clear_selection();
		Cursor::State send_cursor(bool clicking, Point<int16_t> cursorpos) override;

		UIElement::Type get_type() const override;

		void send_line(const std::string& line, LineType type);
		void send_chatline(const std::string& line, LineType type);
		void display_message(Messages::Type line, UIChatBar::LineType type);
		void focus_chatfield();
		// Opens typing with the next lines going privately to this player
		void whisper_to(const std::string& name);
		// Sends a line to party chat and echoes it, as if typed with the Party chip on
		void say_in_party(const std::string& message);
		// Numbered options someone offered ("1. What's up?", ...). On a phone they show
		// as buttons above the chat line for this many seconds, and a tap answers with
		// the option's number, which is what typing would have sent.
		void set_reply_options(const std::vector<std::string>& options, int16_t seconds);
		void clear_reply_options();
		void toggle_chat();
		void toggle_chatfield();
		void toggle_chatfield(bool open);
		bool is_chatopen();
		bool is_chatfieldopen();

		void set_chat_target(ChatTarget target);
		void cycle_chat_target();

		void set_pending_party_invite(int32_t party_id, const std::string& inviter);
		void clear_pending_party_invite();
		void set_party_state(int32_t party_id, int32_t leader_id, const std::vector<PartyMember>& members);
		void clear_party_state();
		void set_party_leader(int32_t leader_id);
		void update_party_member_hp(int32_t cid, int32_t hp, int32_t max_hp);
		int32_t get_party_id() const;
		int32_t get_party_leader_id() const;
		int32_t get_pending_party_invite_id() const;
		const std::string& get_pending_party_inviter() const;
		const std::vector<PartyMember>& get_party_members() const;

	protected:
		Button::State button_pressed(uint16_t buttonid) override;

	private:
		void set_chat_open(bool open);
		int32_t resolve_party_member_id(const std::string& token) const;
		bool handle_party_command(const std::string& message);
		bool handle_whisper_command(const std::string& message);
		void send_whisper(const std::string& name, const std::string& message);
		// The player a tap on the log at this point means, or empty
		std::string whisper_name_at(Point<int16_t> at) const;
		void submit_chat(std::string message);
		void send_chat_message(const std::string& message);
		void send_targeted_message(const std::string& target, const std::string& message);
		void send_party_message(const std::string& message);
		int16_t getchattop() const;

		enum Buttons : uint16_t
		{
			BT_OPENCHAT,
			BT_CLOSECHAT,
			BT_SCROLLUP,
			BT_SCROLLDOWN,
			BT_CHATTARGETS,
			BT_IMOTICON
		};

		// Emoticon picker. Sprites are v83's own: socialChat/BtImoticon for the
		// button and socialChat/ImoticonFrame for the panel.
		void toggle_emoticons();
		void pick_emoticon(int32_t index);
		// What the picker offers. On a phone, the faces the character can make.
		static int32_t picker_count();
		static const char* picker_name(int32_t index);
		void make_face(const std::string& name);
		void draw_emoticons(Point<int16_t> position) const;
		// Grid cell under the cursor, or -1. Shared by hover and click so the
		// highlight can never disagree with what a click inserts.
		int32_t emoticon_at(Point<int16_t> cursorpos) const;
		Point<int16_t> emoticon_origin() const;

#ifdef __ANDROID__
		// One row, to fit between the typing bar and the keyboard
		static constexpr int16_t EMOTE_COLS = 7;
		static constexpr int16_t EMOTE_CELL = 40;
#else
		static constexpr int16_t EMOTE_COLS = 6;
		static constexpr int16_t EMOTE_CELL = 28;
#endif
		static constexpr int16_t EMOTE_PAD = 8;

		bool emoticons_open = false;
		int32_t emoticon_hover = -1;
		Texture emoticon_frame;
		std::vector<Texture> emoticon_icons;

		static constexpr int16_t CHATYOFFSET = 65;
		static constexpr int16_t CHATROWHEIGHT = 16;
		static constexpr int16_t MAXCHATROWS = 16;
		static constexpr int16_t MINCHATROWS = 1;
		static constexpr time_t MESSAGE_COOLDOWN = 1000;

		Textfield chatfield;
		// Shown in the typing bar at the top while the chat field is empty.
		Text typing_hint;

		// Phone: big "send to" chips above the chat log in place of the
		// small classic target menu
		// Five tabs fit beside the emoticon and close buttons at this width
		static constexpr int16_t CHIP_W = 76;
		static constexpr int16_t CHIP_H = 34;
		// The first chips are also "send to" targets. The Whisper tab after
		// them only filters, since a whisper needs a name to go to.
		static constexpr size_t NUM_CHIPS = 4;
		static constexpr size_t NUM_TABS = NUM_CHIPS + 1;
		Rectangle<int16_t> chip_rect(size_t index) const;
		int32_t chip_at(Point<int16_t> cursorpos) const;
		mutable Text chip_labels[NUM_TABS];

		// Which lines the log shows. Tabs are in the same order as the chips.
		enum ChatView : uint8_t
		{
			VIEW_ALL,
			VIEW_PARTY,
			VIEW_GUILD,
			VIEW_BUDDY,
			VIEW_WHISPER
		};

		// Every line received, so switching tabs can rebuild the log from it
		struct LogEntry
		{
			std::string line;
			LineType type;
			ChatView channel;
		};

		// Old lines drop off past this, so a tab switch stays quick
		static constexpr size_t MAX_HISTORY = 1000;
		std::vector<LogEntry> history;
		ChatView view = VIEW_ALL;

		static ChatView line_channel(const std::string& line);
		bool shown_in_view(ChatView channel) const;
		void set_view(ChatView next);
		// Adds a line to the log rows the current tab shows
		void add_row(const std::string& line, LineType type);

		// The phone layout. Everything sits in the free space above the dock,
		// between the touch stick's zone and the attack buttons, so nothing
		// lies under the stick or covers the dock.
		static constexpr int16_t TOUCH_W = 512;
		static constexpr int16_t TOUCH_ABOVE_DOCK = 60;
		static constexpr int16_t TOUCH_LINE_H = 30;
		static constexpr int16_t TOUCH_LINE_REACH = 8;
		static constexpr int16_t TOUCH_ROWS = 5;
		static constexpr int16_t TYPING_MAX_W = 600;
		static constexpr int16_t TYPING_LEFT = 430;
		static constexpr int16_t TYPING_TOP = 26;
		static constexpr int16_t TYPING_H = 40;
		static constexpr int16_t TYPING_BTN_W = 78;
		static constexpr int16_t TYPING_GAP = 6;
		static constexpr int16_t EMOTE_BTN_W = 46;

		int16_t touch_left() const;
		int16_t touch_bottom() const;
		Rectangle<int16_t> line_rect() const;
		Rectangle<int16_t> log_rect() const;
		Rectangle<int16_t> emoticon_button_rect() const;
		Rectangle<int16_t> log_close_rect() const;
		Rectangle<int16_t> emoticon_panel_rect() const;
		Rectangle<int16_t> typing_rect() const;
		Rectangle<int16_t> typing_target_rect() const;
		Rectangle<int16_t> typing_emoticon_rect() const;
		Rectangle<int16_t> typing_field_rect() const;
		Rectangle<int16_t> typing_send_rect() const;
		bool typing_bar_shown() const;
		size_t chip_index() const;
		static ChatTarget chip_target(size_t index);
		void draw_touch() const;
		void draw_touch_line() const;
		void draw_emoticon_button(Rectangle<int16_t> area) const;
		void draw_touch_log() const;
		void draw_log_rows(int16_t left) const;
		void draw_typing_bar() const;
		Cursor::State send_cursor_touch(bool clicking, Point<int16_t> cursorpos);
		void touch_tap(Point<int16_t> at);
		void scroll_log(bool older);

		static constexpr int16_t REPLY_H = 34;
		static constexpr int16_t REPLY_GAP = 6;
		static constexpr int16_t REPLY_PAD = 14;
		std::vector<Rectangle<int16_t>> reply_rects() const;
		int32_t reply_at(Point<int16_t> cursorpos) const;
		void draw_reply_options() const;

		TapGesture tap;
		bool swiping_log = false;
		int16_t swipe_anchor_y = 0;
		// The picker was opened from the typing bar, so it ends with typing
		bool emoticons_typing = false;
		Text touch_close;
		mutable Text send_label;
		// Who typed lines are whispered to. Ends when typing does.
		std::string whisper_target;
		// Who whispered last, for /r
		std::string last_whisper_from;
		// Who whispered while the log was closed, until it's next opened
		std::string unread_whisper_from;
		Text whisper_alert;
		std::vector<Text> reply_labels;
		// Counted down in update(); the options stop being offered when it reaches 0
		int32_t reply_ticks_left = 0;
		mutable Text whisper_chip;
		Text whisper_label;
		Texture chatspace[2];
		Texture chattargets[NUM_TARGETS];
		Texture chatenter;
		Texture chatcover;
		Texture tapbar;
		Texture tapbartop;

		bool chatopen;
		bool chatfieldopen;
		// The log opened only to type a line, so it closes again afterwards
		bool opened_for_typing = false;
		ChatTarget chattarget;
		int32_t party_id;
		int32_t party_leader_id;
		int32_t pending_party_invite_id;
		std::string pending_party_inviter;
		std::vector<PartyMember> party_members;

		EnumMap<Messages::Type, time_t> message_cooldowns;
		std::vector<std::string> lastentered;
		size_t lastpos;

		std::unordered_map<int16_t, Text> rowtexts;
		// Raw line string per row, kept so a selection can be copied to the clipboard.
		std::unordered_map<int16_t, std::string> rowstrings;
		static constexpr int16_t ROW_NONE = -30000;
		int16_t row_at(Point<int16_t> cursorpos) const;
		int16_t col_at(int16_t rowid, int16_t x) const;
		// Selection anchor/focus as (row id, character column). Ordered into
		// start/end by (row, col) reading order for highlight + copy.
		void ordered_sel(int16_t& sr, int16_t& sc, int16_t& er, int16_t& ec) const;
		int16_t sel_arow = 0, sel_acol = 0;
		int16_t sel_frow = 0, sel_fcol = 0;
		bool has_selection = false;
		bool sel_dragging = false;
		// Per-row icon id (0..3 from UIWindow.img/Megaphone) painted before
		// the text on megaphone-broadcast rows. -1 = no icon.
		std::unordered_map<int16_t, int8_t> rowicons;
		// For megaphone rows we split "Name : message" into two Text
		// fragments so the icon can sit between them.
		struct MegaRow
		{
			Text namepart;
			Text msgpart;
			int8_t icon = -1;
		};
		std::unordered_map<int16_t, MegaRow> mega_rows;
		// Cached megaphone icon textures loaded once in the constructor.
		Texture megaphone_icons[4];
		ColorBox chatbox;
		int16_t chatrows;
		int16_t rowpos;
		int16_t rowmax;
		Slider slider;
		bool dragchattop;
	};

	// Convenience namespace for the very common
	//   `if (auto cb = UI::get().get_element<UIChatBar>())
	//        cb->send_chatline(text, type);`
	// pattern. Null-safe — silently no-ops if the chat bar isn't
	// present (e.g. on the title screen).
	namespace chat
	{
		using LineType = UIChatBar::LineType;
		void log(const std::string& line, LineType type = LineType::YELLOW);
	}
}
