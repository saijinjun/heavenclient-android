//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "../UIElement.h"
#include "../Components/TapGesture.h"
#include "../Components/Textfield.h"

#include "../../Graphics/Text.h"

#include <array>
#include <set>
#include <string>
#include <vector>

namespace ms
{
	// Buddies and party as a touch panel in the navy and gold skin. There is
	// no double-click or right-click on a phone, so tapping a name selects it
	// and the buttons along the bottom become what you can do with that name.
	class UIMobileUserList : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEUSERLIST;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		enum Tab : uint8_t
		{
			BUDDIES,
			ONLINE,
			PARTY,
			NUM_TABS
		};

		UIMobileUserList(uint8_t tab);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_scroll(double yoffset) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		static constexpr size_t MAX_ROWS = 8;
		static constexpr size_t MAX_ACTIONS = 5;

		enum class RowKind : uint8_t
		{
			NONE,
			GROUP,
			BUDDY,
			MEMBER,
			INVITE
		};

		enum class Action : uint8_t
		{
			ADD_FRIEND,
			GROUPS,
			WHISPER,
			INVITE_BUDDY,
			NICKNAME,
			MOVE_GROUP,
			REMOVE_BUDDY,
			CREATE_PARTY,
			INVITE_PLAYER,
			PARTY_SETTINGS,
			FIND_MEMBERS,
			LEAVE_PARTY,
			ADD_MEMBER,
			MAKE_LEADER,
			KICK,
			JOIN_PARTY,
			DECLINE_PARTY
		};

		struct Row
		{
			RowKind kind = RowKind::NONE;
			int32_t id = 0;
			std::string name;
			std::string label;
			std::string detail;
			std::string group;
			bool online = false;
			bool selectable = true;
		};

		struct Layout
		{
			Rectangle<int16_t> panel;
			Rectangle<int16_t> close;
			std::array<Rectangle<int16_t>, NUM_TABS> tabs;
			Rectangle<int16_t> list;
			int16_t footer_y = 0;
			int16_t rows = 1;
		};

		Layout layout() const;
		Rectangle<int16_t> row_rect(const Layout& l, int16_t index) const;
		Rectangle<int16_t> action_rect(const Layout& l, size_t index) const;
		void refresh();
		void build_buddies();
		void build_party();
		void build_actions();
		const Row* selected_row() const;
		void run(Action action);

		uint8_t tab;
		int16_t first_row = 0;
		RowKind selected_kind = RowKind::NONE;
		int32_t selected_id = 0;
		std::set<std::string> collapsed;
		std::vector<Row> rows;
		std::vector<Action> actions;
		TapGesture gesture;

		Text title;
		Text count;
		Text empty;
		Text close_label;
		std::array<Text, NUM_TABS> tab_labels;
		std::array<Text, MAX_ROWS> row_names;
		std::array<Text, MAX_ROWS> row_details;
		std::array<Text, MAX_ACTIONS> action_labels;
	};

	// Add a friend, or move one to another group. On a phone the keyboard
	// covers the lower half of the screen, so this sits at the top and asks
	// only for what it uses: the nickname and memo of someone who isn't on the
	// list yet had nowhere to be saved. Nickname has its own button on the list.
	class UIMobileAddBuddy : public UIElement
	{
	public:
		static constexpr Type TYPE = UIElement::Type::MOBILEADDBUDDY;
		static constexpr bool FOCUSED = true;
		static constexpr bool TOGGLED = false;

		UIMobileAddBuddy(const std::string& name, const std::string& group);

		void draw(float inter) const override;
		void update() override;

		Cursor::State send_cursor(bool clicked, Point<int16_t> cursorpos) override;
		void send_key(int32_t keycode, bool pressed, bool escape) override;
		bool is_in_range(Point<int16_t> cursorpos) const override;
		void remove_cursor() override;

		UIElement::Type get_type() const override;

	private:
		void next_group();
		void commit();
		void close();

		std::string group;
		TapGesture gesture;
		Textfield name_field;

		Text title;
		Text placeholder;
		Text group_label;
		Text ok_label;
		Text cancel_label;
	};
}
