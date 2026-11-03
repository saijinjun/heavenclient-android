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
#include "TouchControls.h"

#include "IO/KeyAction.h"
#include "TouchSprites.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace ms
{
	namespace
	{
		// Fingers rarely land dead on a control, so each one accepts touches a
		// little outside the shape it draws.
		constexpr float HIT_SLACK = 1.2f;
		// How far from the stick center, as a share of its radius, a thumb
		// has to move before a direction counts.
		constexpr float DEAD_ZONE = 0.25f;
		// A direction only counts when it dominates the other axis by this
		// ratio, so a sideways drag does not also press up (portals) or down
		// (crouch), while a clear diagonal still presses both.
		constexpr float AXIS_RATIO = 0.5f;
		// Size of the stick's knob relative to its base.
		constexpr float KNOB = 0.42f;

		using namespace TouchSprites;

		// Where each control sits and how it looks. Centers are in layout
		// units from the left or right edge and from the top or bottom edge.
		struct Place
		{
			bool from_right;
			bool from_bottom;
			float x;
			float y;
			float radius;
			Sprite icon;
			float red;
			float green;
			float blue;
		};

		// Glassy dark face for everything that is not a main action.
		constexpr float DR = 0.12f;
		constexpr float DG = 0.14f;
		constexpr float DB = 0.20f;

		// One unit is a sixth of the screen height. On a 20:9 phone each
		// black bar is about 2.7 units wide, which everything below fits in.
		// When the game fills the screen instead, the same strips are free
		// at the bottom beside the centered status bar, and the menu sits
		// below the minimap.
		// In the order of TouchControls::Control.
		constexpr std::array<Place, 21> PLACES =
		{{
			{ false, true,  1.40f, 1.50f, 1.00f, NUM_SPRITES, DR, DG, DB },      // STICK, at rest
			{ true,  true,  1.15f, 1.20f, 0.72f, SWORD, 0.96f, 0.52f, 0.16f },   // ATTACK
			{ true,  true,  2.20f, 0.55f, 0.45f, JUMP, 0.26f, 0.60f, 0.96f },    // JUMP
			{ true,  true,  2.30f, 2.95f, 0.33f, PICKUP, 0.36f, 0.78f, 0.42f },  // PICKUP
			{ true,  true,  2.25f, 1.85f, 0.40f, NUM_SPRITES, DR, DG, DB },      // SLOT0
			{ true,  true,  1.62f, 2.55f, 0.40f, NUM_SPRITES, DR, DG, DB },      // SLOT1
			{ true,  true,  0.72f, 2.70f, 0.40f, NUM_SPRITES, DR, DG, DB },      // SLOT2
			{ true,  true,  2.10f, 3.65f, 0.36f, NUM_SPRITES, DR, DG, DB },      // SLOT3
			{ true,  true,  1.30f, 3.65f, 0.36f, NUM_SPRITES, DR, DG, DB },      // SLOT4
			{ true,  true,  0.50f, 3.65f, 0.36f, NUM_SPRITES, DR, DG, DB },      // SLOT5
			{ false, false, 0.50f, 1.85f, 0.30f, BAG, DR, DG, DB },              // MENU_INVENTORY
			{ false, false, 1.25f, 1.85f, 0.30f, SHIRT, DR, DG, DB },            // MENU_EQUIPS
			{ false, false, 2.00f, 1.85f, 0.30f, BARS, DR, DG, DB },             // MENU_STATS
			{ false, false, 0.50f, 2.60f, 0.30f, STAR, DR, DG, DB },             // MENU_SKILLS
			{ false, false, 1.25f, 2.60f, 0.30f, QUEST, DR, DG, DB },            // MENU_QUESTS
			{ false, false, 2.00f, 2.60f, 0.30f, GLOBE, DR, DG, DB },            // MENU_MAP
			{ true,  true,  0.36f, 0.40f, 0.30f, DROP_DOWN, 0.26f, 0.60f, 0.96f },  // JUMP_DOWN
			{ true,  true,  0.36f, 1.98f, 0.30f, JUMP_SLASH, 0.96f, 0.52f, 0.16f }, // JUMP_ATTACK
			{ true,  false, 0.45f, 0.45f, 0.26f, GEAR, DR, DG, DB },             // MENU_LAYOUT
			{ true,  false, 1.15f, 0.45f, 0.26f, CHAT, DR, DG, DB },             // MENU_CHAT
			{ true,  false, 1.85f, 0.45f, 0.26f, LIST, 0.80f, 0.56f, 0.18f },    // MENU_SOLO
		}};

		// Names for the settings file, in the order of TouchControls::Control.
		constexpr std::array<const char*, 21> NAMES =
		{
			"STICK", "ATTACK", "JUMP", "PICKUP",
			"SLOT0", "SLOT1", "SLOT2", "SLOT3", "SLOT4", "SLOT5",
			"INVENTORY", "EQUIPS", "STATS", "SKILLS", "QUESTS", "MAP",
			"JUMP_DOWN", "JUMP_ATTACK", "LAYOUT", "CHAT", "SOLO"
		};

		// The keys the stick and the action buttons hold down, in the order
		// they are pressed: down before jump makes a jump down, and jump
		// before attack makes a jump attack.
		constexpr std::array<int32_t, 6> KEYS =
		{
			KeyAction::LEFT, KeyAction::RIGHT, KeyAction::UP, KeyAction::DOWN,
			KeyAction::JUMP, KeyAction::ATTACK
		};

		// The layout editor's toolbar, left to right along the top middle.
		constexpr std::array<Sprite, 5> TOOL_ICONS = { MINUS, PLUS, FADE, RESET, CHECK };
		constexpr float TOOL_RADIUS = 0.32f;
		constexpr float TOOL_SPACING = 0.9f;

		// Limits for what the editor can change.
		constexpr float MIN_SCALE = 0.6f;
		constexpr float MAX_SCALE = 1.6f;
		constexpr float SCALE_STEP = 0.1f;
		constexpr std::array<float, 4> OPACITIES = { 1.0f, 0.75f, 0.5f, 0.3f };

		// The stick appears wherever the left thumb lands in this part of
		// the screen: left of STICK_ZONE_RIGHT and below STICK_ZONE_TOP.
		constexpr float STICK_ZONE_RIGHT = 2.9f;
		constexpr float STICK_ZONE_TOP = 3.0f;
	}

	void TouchControls::set_screen(int32_t screen_width, int32_t screen_height)
	{
		width = static_cast<float>(screen_width);
		height = static_cast<float>(screen_height);
	}

	void TouchControls::set_time(uint32_t milliseconds)
	{
		now = milliseconds;
	}

	void TouchControls::set_slot_filled(uint8_t slot, bool filled)
	{
		if (slot < NUM_SLOTS)
		{
			slots_filled[slot] = filled;
		}
	}

	void TouchControls::set_slot_info(uint8_t slot, int32_t count, bool cooling)
	{
		if (slot < NUM_SLOTS)
		{
			slot_counts[slot] = count;
			slots_cooling[slot] = cooling;
		}
	}

	float TouchControls::unit() const
	{
		return height / 6.0f;
	}

	TouchControls::Circle TouchControls::home_area(Control control) const
	{
		static_assert(PLACES.size() == NUM_CONTROLS, "one place per control");
		static_assert(NAMES.size() == NUM_CONTROLS, "one name per control");

		const Place& place = PLACES[control];
		float u = unit();

		return {
			place.from_right ? width - place.x * u : place.x * u,
			place.from_bottom ? height - place.y * u : place.y * u,
			place.radius * u
		};
	}

	TouchControls::Circle TouchControls::area(Control control) const
	{
		Circle home = home_area(control);
		const Adjustment& adjustment = adjustments[control];
		float u = unit();
		float radius = home.radius * adjustment.scale;

		// Moved controls stay whole on the screen, even on a smaller one
		// than they were placed on.
		return {
			std::clamp(home.x + adjustment.dx * u, radius, std::max(radius, width - radius)),
			std::clamp(home.y + adjustment.dy * u, radius, std::max(radius, height - radius)),
			radius
		};
	}

	TouchControls::Circle TouchControls::tool_area(Tool tool) const
	{
		float u = unit();
		float offset = static_cast<float>(tool) - (NUM_TOOLS - 1) / 2.0f;
		return { width / 2.0f + offset * TOOL_SPACING * u, 0.55f * u, TOOL_RADIUS * u };
	}

	bool TouchControls::in_stick_zone(float x, float y) const
	{
		// Near wherever the stick was moved to also works.
		Circle home = area(STICK);
		bool near_home = std::hypot(x - home.x, y - home.y) < home.radius * 1.3f;
		return near_home || (x < STICK_ZONE_RIGHT * unit() && y > STICK_ZONE_TOP * unit());
	}

	bool TouchControls::control_at(float x, float y, Control& control) const
	{
		// The closest button wins when the slack around two of them overlaps.
		float best = HIT_SLACK;

		for (uint8_t i = ATTACK; i < NUM_CONTROLS; i++)
		{
			Control candidate = static_cast<Control>(i);

			if (candidate >= SLOT0 && candidate <= SLOT_LAST && !slots_filled[candidate - SLOT0])
			{
				continue;
			}

			Circle circle = area(candidate);
			float distance = std::hypot(x - circle.x, y - circle.y) / circle.radius;

			if (distance <= best)
			{
				best = distance;
				control = candidate;
			}
		}

		if (best < HIT_SLACK)
		{
			return true;
		}

		if (in_stick_zone(x, y))
		{
			control = STICK;
			return true;
		}

		return false;
	}

	bool TouchControls::finger_down(int64_t finger, float x, float y, std::vector<Action>& actions)
	{
		if (editing)
		{
			return edit_down(finger, x, y);
		}

		Control control;

		if (!control_at(x, y, control))
		{
			return false;
		}

		if (control == MENU_LAYOUT)
		{
			start_editing(finger, actions);
			return true;
		}

		if (control == STICK && fingers_on(STICK) > 0)
		{
			// A second thumb in the stick area does not move the stick, but
			// it is still not a tap on the game.
			fingers[finger] = NUM_CONTROLS;
			return true;
		}

		fingers[finger] = control;

		if (control == STICK)
		{
			place_stick(x, y);
			aim_stick(x, y, actions);
		}
		else
		{
			set_button(control, true, actions);
		}

		return true;
	}

	bool TouchControls::finger_move(int64_t finger, float x, float y, std::vector<Action>& actions)
	{
		auto iter = fingers.find(finger);

		if (iter == fingers.end())
		{
			return false;
		}

		if (editing)
		{
			if (finger == edit_finger && selected < NUM_CONTROLS)
			{
				move_control(selected, x - grab_x, y - grab_y);
			}
			return true;
		}

		// The stick keeps following its thumb even past its edge. Buttons stay
		// held until the finger lifts, wherever it slides.
		if (iter->second == STICK)
		{
			aim_stick(x, y, actions);
		}

		return true;
	}

	bool TouchControls::finger_up(int64_t finger, std::vector<Action>& actions)
	{
		auto iter = fingers.find(finger);

		if (iter == fingers.end())
		{
			return false;
		}

		Control control = iter->second;
		fingers.erase(iter);

		if (finger == edit_finger)
		{
			edit_finger = -1;
		}

		if (control == NUM_CONTROLS)
		{
			return true;
		}

		// Another finger may still be holding the same control.
		if (fingers_on(control) == 0)
		{
			if (control == STICK)
			{
				for (uint8_t i = 0; i < NUM_DIRECTIONS; i++)
				{
					set_direction(static_cast<Direction>(i), false, actions);
				}
			}
			else
			{
				set_button(control, false, actions);
			}
		}

		return true;
	}

	void TouchControls::release_all(std::vector<Action>& actions)
	{
		while (!fingers.empty())
		{
			finger_up(fingers.begin()->first, actions);
		}
	}

	uint8_t TouchControls::fingers_on(Control control) const
	{
		return static_cast<uint8_t>(std::count_if(fingers.begin(), fingers.end(),
			[control](const auto& entry) { return entry.second == control; }));
	}

	void TouchControls::place_stick(float x, float y)
	{
		// Centered under the thumb, so touching it presses nothing yet.
		stick_x = x;
		stick_y = y;
		thumb_x = x;
		thumb_y = y;
	}

	void TouchControls::aim_stick(float x, float y, std::vector<Action>& actions)
	{
		float radius = area(STICK).radius;
		float dx = x - stick_x;
		float dy = y - stick_y;
		float dead = radius * DEAD_ZONE;

		thumb_x = x;
		thumb_y = y;

		bool horizontal = std::fabs(dx) > dead && std::fabs(dx) >= AXIS_RATIO * std::fabs(dy);
		bool vertical = std::fabs(dy) > dead && std::fabs(dy) >= AXIS_RATIO * std::fabs(dx);

		set_direction(LEFT, horizontal && dx < 0.0f, actions);
		set_direction(RIGHT, horizontal && dx > 0.0f, actions);
		set_direction(UP, vertical && dy < 0.0f, actions);
		set_direction(DOWN, vertical && dy > 0.0f, actions);
	}

	void TouchControls::set_direction(Direction direction, bool pressed, std::vector<Action>& actions)
	{
		if (directions[direction] == pressed)
		{
			return;
		}

		directions[direction] = pressed;
		send_held_keys(actions);
	}

	void TouchControls::send_held_keys(std::vector<Action>& actions)
	{
		// Several controls can hold the same key: the jump down button holds
		// down and jump, the jump attack button jump and attack.
		bool jump_down = buttons[JUMP_DOWN];
		bool jump_attack = buttons[JUMP_ATTACK];
		std::array<bool, KEYS.size()> wanted =
		{
			directions[LEFT],
			directions[RIGHT],
			directions[UP],
			directions[DOWN] || jump_down,
			buttons[JUMP] || jump_down || jump_attack,
			buttons[ATTACK] || jump_attack
		};

		for (size_t i = 0; i < KEYS.size(); i++)
		{
			if (keys_held[i] != wanted[i])
			{
				keys_held[i] = wanted[i];
				actions.push_back({ Output::ACTION, KEYS[i], wanted[i] });
			}
		}
	}

	TouchControls::Action TouchControls::output(Control control, bool pressed) const
	{
		switch (control)
		{
		case PICKUP:
			return { Output::ACTION, KeyAction::PICKUP, pressed };
		case MENU_INVENTORY:
			return { Output::MENU, KeyAction::ITEMS, pressed };
		case MENU_EQUIPS:
			return { Output::MENU, KeyAction::EQUIPMENT, pressed };
		case MENU_STATS:
			return { Output::MENU, KeyAction::STATS, pressed };
		case MENU_SKILLS:
			return { Output::MENU, KeyAction::SKILLS, pressed };
		case MENU_QUESTS:
			return { Output::MENU, KeyAction::QUESTLOG, pressed };
		case MENU_MAP:
			return { Output::MENU, KeyAction::WORLDMAP, pressed };
		case MENU_CHAT:
			return { Output::CHAT, 0, pressed };
		case MENU_SOLO:
			return { Output::SOLO_MENU, 0, pressed };
		default:
			return { Output::SLOT, control - SLOT0, pressed };
		}
	}

	void TouchControls::set_button(Control control, bool pressed, std::vector<Action>& actions)
	{
		if (buttons[control] == pressed)
		{
			return;
		}

		buttons[control] = pressed;
		if (pressed)
		{
			pressed_at[control] = now == 0 ? 1 : now;
		}

		if (control == ATTACK || control == JUMP || control == JUMP_DOWN || control == JUMP_ATTACK)
		{
			send_held_keys(actions);
			return;
		}

		Action action = output(control, pressed);

		// Menus toggle a window once per tap, so only the press is sent.
		if (action.output != Output::MENU || pressed)
		{
			actions.push_back(action);
		}
	}

	std::vector<TouchControls::Draw> TouchControls::draws() const
	{
		std::vector<Draw> result;

		if (width <= 0.0f || height <= 0.0f)
		{
			return result;
		}

		// Everything but the editor's toolbar is as see-through as the
		// player chose.
		float fade = opacity;
		auto sprite = [&result, &fade](Sprite s, float x, float y, float half, float r, float g, float b, float a) {
			result.push_back({ Draw::SPRITE, s, 0, x, y, half, r, g, b, a * fade });
		};

		// The stick: a faint base with arrows, and a knob under the thumb.
		// At rest it waits at its home position, dimmer.
		bool held = fingers_on(STICK) > 0;
		Circle home = area(STICK);
		// Near an edge the base is drawn moved in so all of it shows.
		float base_x = held ? std::clamp(stick_x, home.radius, std::max(home.radius, width - home.radius)) : home.x;
		float base_y = held ? std::clamp(stick_y, home.radius, std::max(home.radius, height - home.radius)) : home.y;
		float knob_x = base_x;
		float knob_y = base_y;

		if (held)
		{
			float dx = thumb_x - stick_x;
			float dy = thumb_y - stick_y;
			float reach = home.radius * (1.0f - KNOB);
			float distance = std::hypot(dx, dy);
			float scale = distance > reach ? reach / distance : 1.0f;
			knob_x += dx * scale;
			knob_y += dy * scale;
		}

		float stick_alpha = held ? 1.0f : 0.7f;
		sprite(DISC, base_x, base_y, home.radius, 0.08f, 0.10f, 0.14f, 0.35f * stick_alpha);
		sprite(RING, base_x, base_y, home.radius, 1.0f, 1.0f, 1.0f, 0.30f * stick_alpha);
		sprite(ARROWS, base_x, base_y, home.radius, 1.0f, 1.0f, 1.0f, 0.55f * stick_alpha);
		sprite(DISC, knob_x, knob_y, home.radius * KNOB, 0.86f, 0.90f, 1.0f, 0.65f * stick_alpha);

		// Buttons: a tinted glassy face, a thin white rim and a white icon.
		// A held button shrinks a little and lights up.
		for (uint8_t i = ATTACK; i < NUM_CONTROLS; i++)
		{
			Control control = static_cast<Control>(i);
			Circle circle = area(control);
			bool pressed = buttons[control];
			bool slot = control >= SLOT0 && control <= SLOT_LAST;
			bool empty = slot && !slots_filled[control - SLOT0];
			const Place& look = PLACES[control];

			float half = circle.radius * (pressed ? 0.93f : 1.0f);
			float face = empty ? 0.18f : (pressed ? 0.80f : 0.55f);
			float lift = pressed ? 0.25f : 0.0f;

			if (TouchSprites::has_art(ORB))
			{
				// The navy orb, lightly tinted with the button's color and
				// lit gold while held
				auto tint = [&](float c) { return std::min(1.0f, 0.7f + 0.3f * c + lift); };
				sprite(ORB, circle.x, circle.y, half, tint(look.red), tint(look.green), tint(look.blue),
					empty ? 0.45f : (pressed ? 1.0f : 0.85f));

				if (pressed)
					sprite(DISC, circle.x, circle.y, half * 0.8f, 1.0f, 0.82f, 0.25f, 0.25f);
			}
			else
			{
				sprite(DISC, circle.x, circle.y, half,
					std::min(1.0f, look.red + lift), std::min(1.0f, look.green + lift), std::min(1.0f, look.blue + lift), face);
				sprite(RING, circle.x, circle.y, half, 1.0f, 1.0f, 1.0f, empty ? 0.25f : 0.45f);
			}

			// A ring that grows and fades for a moment after each press.
			constexpr uint32_t RIPPLE_MS = 280;
			uint32_t age = now - pressed_at[control];
			if (pressed_at[control] != 0 && age < RIPPLE_MS)
			{
				float t = static_cast<float>(age) / RIPPLE_MS;
				sprite(RING, circle.x, circle.y, circle.radius * (1.0f + 0.35f * t),
					std::min(1.0f, look.red + 0.4f), std::min(1.0f, look.green + 0.4f), std::min(1.0f, look.blue + 0.4f),
					0.7f * (1.0f - t));
			}

			if (slot)
			{
				uint8_t index = static_cast<uint8_t>(control - SLOT0);

				if (empty)
				{
					// An empty slot hints that something can go there.
					sprite(PLUS, circle.x, circle.y, half * 0.45f, 1.0f, 1.0f, 1.0f, 0.35f);
					continue;
				}

				result.push_back({ Draw::SLOT_ICON, 0, index,
					circle.x, circle.y, half * 0.62f, 1.0f, 1.0f, 1.0f, (pressed ? 1.0f : 0.92f) * fade });

				if (slots_cooling[index])
				{
					sprite(DISC, circle.x, circle.y, half * 0.94f, 0.0f, 0.0f, 0.0f, 0.6f);
				}

				if (slot_counts[index] >= 0)
				{
					// The count in the bottom right, digits with a shadow.
					std::string digits = std::to_string(std::min(slot_counts[index], 9999));
					float size = half * 0.26f;
					float advance = size * 0.95f;
					float x = circle.x + half * 0.72f - advance * (digits.size() - 1);
					float y = circle.y + half * 0.62f;
					float shadow = std::max(1.0f, size * 0.12f);

					for (char c : digits)
					{
						auto glyph = static_cast<Sprite>(DIGIT0 + (c - '0'));
						sprite(glyph, x + shadow, y + shadow, size, 0.0f, 0.0f, 0.0f, 0.8f);
						sprite(glyph, x, y, size, 1.0f, 1.0f, 1.0f, 1.0f);
						x += advance;
					}
				}
			}
			else if (look.icon < NUM_SPRITES)
			{
				sprite(look.icon, circle.x, circle.y, half * 0.6f, 1.0f, 1.0f, 1.0f, 0.95f);
			}
		}

		if (editing)
		{
			fade = 1.0f;

			// The control being edited gets a gold outline.
			if (selected < NUM_CONTROLS)
			{
				Circle circle = area(selected);
				sprite(RING, circle.x, circle.y, circle.radius * 1.15f, 1.0f, 0.82f, 0.25f, 1.0f);
			}

			for (uint8_t i = 0; i < NUM_TOOLS; i++)
			{
				Tool tool = static_cast<Tool>(i);
				Circle circle = tool_area(tool);
				bool done = tool == TOOL_DONE;
				// Resizing needs a control picked first.
				bool usable = selected < NUM_CONTROLS || (tool != TOOL_SMALLER && tool != TOOL_BIGGER);
				float face = usable ? 0.8f : 0.35f;

				sprite(DISC, circle.x, circle.y, circle.radius,
					done ? 0.30f : DR, done ? 0.72f : DG, done ? 0.40f : DB, face);
				sprite(RING, circle.x, circle.y, circle.radius, 1.0f, 1.0f, 1.0f, 0.6f);
				sprite(TOOL_ICONS[i], circle.x, circle.y, circle.radius * 0.6f, 1.0f, 1.0f, 1.0f, usable ? 1.0f : 0.4f);
			}
		}

		return result;
	}

	void TouchControls::start_editing(int64_t finger, std::vector<Action>& actions)
	{
		// Let go of everything held, so the character stops while the
		// controls are rearranged. Fingers already down are ignored until
		// they lift.
		directions = {};
		for (uint8_t i = ATTACK; i < NUM_CONTROLS; i++)
		{
			set_button(static_cast<Control>(i), false, actions);
		}
		send_held_keys(actions);

		for (auto& entry : fingers)
		{
			entry.second = NUM_CONTROLS;
		}
		fingers[finger] = NUM_CONTROLS;

		editing = true;
		selected = NUM_CONTROLS;
		edit_finger = -1;
	}

	bool TouchControls::edit_down(int64_t finger, float x, float y)
	{
		// While editing, every touch belongs to the editor.
		fingers[finger] = NUM_CONTROLS;

		for (uint8_t i = 0; i < NUM_TOOLS; i++)
		{
			Circle circle = tool_area(static_cast<Tool>(i));
			if (std::hypot(x - circle.x, y - circle.y) <= circle.radius * HIT_SLACK)
			{
				use_tool(static_cast<Tool>(i));
				return true;
			}
		}

		// Any control can be picked up, empty slots and the stick included.
		float best = HIT_SLACK;
		Control found = NUM_CONTROLS;
		for (uint8_t i = 0; i < NUM_CONTROLS; i++)
		{
			Circle circle = area(static_cast<Control>(i));
			float distance = std::hypot(x - circle.x, y - circle.y) / circle.radius;
			if (distance <= best)
			{
				best = distance;
				found = static_cast<Control>(i);
			}
		}

		if (found < NUM_CONTROLS && best < HIT_SLACK)
		{
			Circle circle = area(found);
			selected = found;
			edit_finger = finger;
			grab_x = x - circle.x;
			grab_y = y - circle.y;
		}

		return true;
	}

	void TouchControls::use_tool(Tool tool)
	{
		switch (tool)
		{
		case TOOL_SMALLER:
		case TOOL_BIGGER:
			if (selected < NUM_CONTROLS)
			{
				Adjustment& adjustment = adjustments[selected];
				float step = tool == TOOL_BIGGER ? SCALE_STEP : -SCALE_STEP;
				adjustment.scale = std::clamp(adjustment.scale + step, MIN_SCALE, MAX_SCALE);
			}
			break;
		case TOOL_FADE:
		{
			// Steps from solid to faint, then back to solid.
			size_t next = 0;
			for (size_t i = 0; i < OPACITIES.size(); i++)
			{
				if (opacity >= OPACITIES[i] - 0.01f)
				{
					next = (i + 1) % OPACITIES.size();
					break;
				}
			}
			opacity = OPACITIES[next];
			break;
		}
		case TOOL_RESET:
			adjustments = {};
			opacity = 1.0f;
			break;
		case TOOL_DONE:
			editing = false;
			selected = NUM_CONTROLS;
			edit_finger = -1;
			break;
		default:
			break;
		}

		layout_changed = true;
	}

	void TouchControls::move_control(Control control, float x, float y)
	{
		Circle home = home_area(control);
		float u = unit();
		float radius = home.radius * adjustments[control].scale;

		x = std::clamp(x, radius, std::max(radius, width - radius));
		y = std::clamp(y, radius, std::max(radius, height - radius));
		adjustments[control].dx = (x - home.x) / u;
		adjustments[control].dy = (y - home.y) / u;
		layout_changed = true;
	}

	bool TouchControls::is_editing() const
	{
		return editing;
	}

	bool TouchControls::take_layout_changed()
	{
		bool changed = layout_changed;
		layout_changed = false;
		return changed;
	}

	std::string TouchControls::get_layout() const
	{
		// For example "opacity:0.75;ATTACK:-0.40,0.10,1.20", where the
		// numbers are how far the control moved right and down, in sixths
		// of the screen height, and its size. Only changes are written.
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "opacity:%.2f", opacity);
		std::string text = buffer;

		for (uint8_t i = 0; i < NUM_CONTROLS; i++)
		{
			const Adjustment& adjustment = adjustments[i];
			if (adjustment.dx == 0.0f && adjustment.dy == 0.0f && adjustment.scale == 1.0f)
			{
				continue;
			}

			std::snprintf(buffer, sizeof(buffer), ";%s:%.3f,%.3f,%.2f",
				NAMES[i], adjustment.dx, adjustment.dy, adjustment.scale);
			text += buffer;
		}

		return text;
	}

	void TouchControls::set_layout(const std::string& text)
	{
		adjustments = {};
		opacity = 1.0f;

		size_t start = 0;
		while (start < text.size())
		{
			size_t end = text.find(';', start);
			if (end == std::string::npos)
			{
				end = text.size();
			}

			std::string part = text.substr(start, end - start);
			start = end + 1;

			size_t colon = part.find(':');
			if (colon == std::string::npos)
			{
				continue;
			}

			std::string name = part.substr(0, colon);
			const char* values = part.c_str() + colon + 1;

			if (name == "opacity")
			{
				char* end = nullptr;
				float value = std::strtof(values, &end);
				if (end != values)
				{
					opacity = std::clamp(value, OPACITIES.back(), 1.0f);
				}
				continue;
			}

			auto found = std::find_if(NAMES.begin(), NAMES.end(),
				[&name](const char* candidate) { return name == candidate; });
			Adjustment adjustment;
			if (found == NAMES.end() ||
				std::sscanf(values, "%f,%f,%f", &adjustment.dx, &adjustment.dy, &adjustment.scale) != 3)
			{
				continue;
			}

			// Nothing is moved further than across the screen.
			adjustment.dx = std::clamp(adjustment.dx, -20.0f, 20.0f);
			adjustment.dy = std::clamp(adjustment.dy, -6.0f, 6.0f);
			adjustment.scale = std::clamp(adjustment.scale, MIN_SCALE, MAX_SCALE);
			adjustments[found - NAMES.begin()] = adjustment;
		}
	}

	bool TouchControls::slot_at(float x, float y, uint8_t& slot) const
	{
		for (uint8_t i = 0; i < NUM_SLOTS; i++)
		{
			Circle circle = area(static_cast<Control>(SLOT0 + i));

			if (std::hypot(x - circle.x, y - circle.y) <= circle.radius * HIT_SLACK)
			{
				slot = i;
				return true;
			}
		}

		return false;
	}
}
