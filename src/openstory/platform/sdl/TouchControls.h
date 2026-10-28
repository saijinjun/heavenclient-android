//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ms
{
	// On-screen controls for touch screens, laid out like a mobile game:
	// a floating analog stick and menu buttons on the left, and on the right
	// a large attack button with jump, pick up and six quick slots around it.
	//
	// Positions are in screen pixels with the origin at the top left. Sizes
	// scale with the screen height so the controls stay thumb-sized on any
	// phone, and on wide screens they sit in the black bars beside the game.
	// This class only tracks fingers and reports what to do and what to
	// draw; the window feeds it touch events and does the rest.
	//
	// The gear button opens a layout editor: drag any control to move it,
	// and use the toolbar at the top to resize the selected one, change how
	// see-through all of them are, or go back to the default layout.
	class TouchControls
	{
	public:
		static constexpr uint8_t NUM_SLOTS = 6;

		// What a control does. ACTION and MENU ids are KeyAction ids; SLOT
		// ids are quick slot numbers, which the window maps to the keys whose
		// skills or items should be used.
		enum class Output : uint8_t
		{
			ACTION,
			MENU,
			SLOT,
			// Opens the chat field, or sends it away when it is open.
			CHAT
		};

		struct Action
		{
			Output output;
			int32_t id;
			bool pressed;
		};

		// A sprite from TouchSprites, or the icon of a quick slot's skill or
		// item, centered on (x, y) and half wide by half high.
		struct Draw
		{
			enum Kind : uint8_t
			{
				SPRITE,
				SLOT_ICON
			};

			Kind kind;
			uint8_t sprite;
			uint8_t slot;
			float x;
			float y;
			float half;
			float red;
			float green;
			float blue;
			float alpha;
		};

		void set_screen(int32_t width, int32_t height);
		// The current time, for the ripple when a button is pressed.
		void set_time(uint32_t milliseconds);
		// Empty slots are drawn faintly and ignore touches.
		void set_slot_filled(uint8_t slot, bool filled);
		// What a filled slot shows besides its icon: how many of its item
		// are left (negative for none, e.g. a skill), and whether its skill
		// is cooling down.
		void set_slot_info(uint8_t slot, int32_t count, bool cooling);

		// Each returns true when the finger belongs to a control, so the
		// caller must not also treat it as a tap on the game.
		bool finger_down(int64_t finger, float x, float y, std::vector<Action>& actions);
		bool finger_move(int64_t finger, float x, float y, std::vector<Action>& actions);
		bool finger_up(int64_t finger, std::vector<Action>& actions);

		// Lets go of everything, e.g. when leaving the game screen.
		void release_all(std::vector<Action>& actions);

		std::vector<Draw> draws() const;

		// The quick slot under a point, filled or not; used to drop skills
		// and items dragged from their windows onto a slot.
		bool slot_at(float x, float y, uint8_t& slot) const;

		bool is_editing() const;
		// The layout as text for the settings file, and back. Unknown or
		// broken parts of the text are ignored.
		std::string get_layout() const;
		void set_layout(const std::string& text);
		// True once after the layout editor changed something, so the
		// window knows to save it.
		bool take_layout_changed();

	private:
		enum Control : uint8_t
		{
			STICK,
			ATTACK,
			JUMP,
			PICKUP,
			SLOT0,
			SLOT_LAST = SLOT0 + NUM_SLOTS - 1,
			MENU_INVENTORY,
			MENU_EQUIPS,
			MENU_STATS,
			MENU_SKILLS,
			MENU_QUESTS,
			MENU_MAP,
			JUMP_DOWN,
			JUMP_ATTACK,
			MENU_LAYOUT,
			MENU_CHAT,
			NUM_CONTROLS
		};

		// The layout editor's toolbar.
		enum Tool : uint8_t
		{
			TOOL_SMALLER,
			TOOL_BIGGER,
			TOOL_FADE,
			TOOL_RESET,
			TOOL_DONE,
			NUM_TOOLS
		};

		// How the player changed a control from its default place.
		struct Adjustment
		{
			float dx = 0.0f;
			float dy = 0.0f;
			float scale = 1.0f;
		};

		enum Direction : uint8_t
		{
			LEFT,
			RIGHT,
			UP,
			DOWN,
			NUM_DIRECTIONS
		};

		struct Circle
		{
			float x;
			float y;
			float radius;
		};

		float unit() const;
		Circle home_area(Control control) const;
		Circle area(Control control) const;
		Circle tool_area(Tool tool) const;
		bool in_stick_zone(float x, float y) const;
		bool control_at(float x, float y, Control& control) const;
		Action output(Control control, bool pressed) const;
		void place_stick(float x, float y);
		void aim_stick(float x, float y, std::vector<Action>& actions);
		void set_direction(Direction direction, bool pressed, std::vector<Action>& actions);
		void set_button(Control control, bool pressed, std::vector<Action>& actions);
		void send_held_keys(std::vector<Action>& actions);
		uint8_t fingers_on(Control control) const;
		void start_editing(int64_t finger, std::vector<Action>& actions);
		bool edit_down(int64_t finger, float x, float y);
		void use_tool(Tool tool);
		void move_control(Control control, float x, float y);

		float width = 0.0f;
		float height = 0.0f;
		std::unordered_map<int64_t, Control> fingers;
		std::array<bool, NUM_DIRECTIONS> directions = {};
		std::array<bool, NUM_CONTROLS> buttons = {};
		std::array<uint32_t, NUM_CONTROLS> pressed_at = {};
		uint32_t now = 0;
		std::array<bool, NUM_SLOTS> slots_filled = {};
		std::array<int32_t, NUM_SLOTS> slot_counts = { -1, -1, -1, -1, -1, -1 };
		std::array<bool, NUM_SLOTS> slots_cooling = {};
		// Where the stick sits while a thumb holds it, and the thumb itself.
		float stick_x = 0.0f;
		float stick_y = 0.0f;
		float thumb_x = 0.0f;
		float thumb_y = 0.0f;
		// The keys sent as held for the stick and buttons together, in the
		// order of KEYS in TouchControls.cpp.
		std::array<bool, 6> keys_held = {};

		std::array<Adjustment, NUM_CONTROLS> adjustments = {};
		float opacity = 1.0f;
		bool editing = false;
		bool layout_changed = false;
		// The control being edited, and the finger dragging it and where on
		// the control it grabbed.
		Control selected = NUM_CONTROLS;
		int64_t edit_finger = -1;
		float grab_x = 0.0f;
		float grab_y = 0.0f;
	};
}
