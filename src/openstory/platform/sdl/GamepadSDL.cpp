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
//	SDL port of src/IO/Gamepad.cpp: the same button mapping, read through
//	SDL_GameController instead of GLFW's gamepad API.
#include "IO/Gamepad.h"

#include "IO/UI.h"

#include <SDL.h>

namespace ms
{
	Gamepad::Gamepad()
	{
		connected = false;
		joystick_id = GLFW_JOYSTICK_1;
		last_pressed = GP_NONE;

		for (int i = 0; i < 15; i++)
			prev_state[i] = false;

		prev_axis_left = false;
		prev_axis_right = false;
		prev_axis_up = false;
		prev_axis_down = false;

		reset_defaults();
	}

	void Gamepad::reset_defaults()
	{
		button_map.clear();

		// D-pad for movement
		button_map[GP_DPAD_LEFT]     = KeyAction::Id::LEFT;
		button_map[GP_DPAD_RIGHT]    = KeyAction::Id::RIGHT;
		button_map[GP_DPAD_UP]       = KeyAction::Id::UP;
		button_map[GP_DPAD_DOWN]     = KeyAction::Id::DOWN;

		// Face buttons
		button_map[GP_A]             = KeyAction::Id::JUMP;
		button_map[GP_B]             = KeyAction::Id::ATTACK;
		button_map[GP_X]             = KeyAction::Id::PICKUP;
		button_map[GP_Y]             = KeyAction::Id::ITEMS;

		// Bumpers/triggers as hotkeys (mapped to skills/items slots)
		button_map[GP_LEFT_BUMPER]   = KeyAction::Id::SKILLS;
		button_map[GP_RIGHT_BUMPER]  = KeyAction::Id::STATS;

		// System
		button_map[GP_START]         = KeyAction::Id::MAINMENU;
		button_map[GP_BACK]          = KeyAction::Id::ESCAPE;

		// Thumbsticks
		button_map[GP_LEFT_THUMB]    = KeyAction::Id::MINIMAP;
		button_map[GP_RIGHT_THUMB]   = KeyAction::Id::SIT;
	}

	void Gamepad::poll()
	{
		static SDL_GameController* controller = nullptr;

		if (controller && !SDL_GameControllerGetAttached(controller))
		{
			SDL_GameControllerClose(controller);
			controller = nullptr;
		}

		if (!controller)
		{
			for (int i = 0; i < SDL_NumJoysticks() && !controller; i++)
				if (SDL_IsGameController(i))
					controller = SDL_GameControllerOpen(i);
		}

		if (!controller)
		{
			if (connected)
			{
				connected = false;
				name = "";
			}

			return;
		}

		if (!connected)
		{
			connected = true;
			const char* jname = SDL_GameControllerName(controller);
			name = jname ? jname : "Gamepad";
		}

		// SDL's buttons in GLFW's gamepad order, so the mappings carry over.
		static const SDL_GameControllerButton ORDER[GLFW_GAMEPAD_BUTTON_LAST + 1] = {
			SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B,
			SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
			SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
			SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
			SDL_CONTROLLER_BUTTON_GUIDE,
			SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
			SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
			SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT
		};

		struct
		{
			unsigned char buttons[GLFW_GAMEPAD_BUTTON_LAST + 1];
			float axes[2];
		} state;

		for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++)
			state.buttons[i] = SDL_GameControllerGetButton(controller, ORDER[i]) ? GLFW_PRESS : GLFW_RELEASE;

		state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
		state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;

		{
			// Process buttons
			for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++)
			{
				bool pressed = state.buttons[i] == GLFW_PRESS;
				bool was_pressed = prev_state[i];

				if (pressed && !was_pressed)
				{
					last_pressed = i;

					auto it = button_map.find(static_cast<Button>(i));

					if (it != button_map.end())
					{
						KeyAction::Id action = it->second;

						// For movement keys, simulate GLFW key codes directly
						if (action == KeyAction::Id::LEFT)
							UI::get().send_key(GLFW_KEY_LEFT, true);
						else if (action == KeyAction::Id::RIGHT)
							UI::get().send_key(GLFW_KEY_RIGHT, true);
						else if (action == KeyAction::Id::UP)
							UI::get().send_key(GLFW_KEY_UP, true);
						else if (action == KeyAction::Id::DOWN)
							UI::get().send_key(GLFW_KEY_DOWN, true);
						else if (action == KeyAction::Id::JUMP)
							UI::get().send_key(GLFW_KEY_LEFT_ALT, true);
						else if (action == KeyAction::Id::ATTACK)
							UI::get().send_key(GLFW_KEY_LEFT_CONTROL, true);
						else if (action == KeyAction::Id::ESCAPE)
							UI::get().send_key(GLFW_KEY_ESCAPE, true);
						else
						{
							// Find the keyboard key that maps to this action and simulate it
							auto& kb = UI::get().get_keyboard();
							auto maplekeys = kb.get_maplekeys();

							for (auto& pair : maplekeys)
							{
								if (pair.second.action == action)
								{
									UI::get().send_key(pair.first, true);
									break;
								}
							}
						}
					}
				}
				else if (!pressed && was_pressed)
				{
					auto it = button_map.find(static_cast<Button>(i));

					if (it != button_map.end())
					{
						KeyAction::Id action = it->second;

						if (action == KeyAction::Id::LEFT)
							UI::get().send_key(GLFW_KEY_LEFT, false);
						else if (action == KeyAction::Id::RIGHT)
							UI::get().send_key(GLFW_KEY_RIGHT, false);
						else if (action == KeyAction::Id::UP)
							UI::get().send_key(GLFW_KEY_UP, false);
						else if (action == KeyAction::Id::DOWN)
							UI::get().send_key(GLFW_KEY_DOWN, false);
						else if (action == KeyAction::Id::JUMP)
							UI::get().send_key(GLFW_KEY_LEFT_ALT, false);
						else if (action == KeyAction::Id::ATTACK)
							UI::get().send_key(GLFW_KEY_LEFT_CONTROL, false);
						else if (action == KeyAction::Id::ESCAPE)
							UI::get().send_key(GLFW_KEY_ESCAPE, false);
						else
						{
							auto& kb = UI::get().get_keyboard();
							auto maplekeys = kb.get_maplekeys();

							for (auto& pair : maplekeys)
							{
								if (pair.second.action == action)
								{
									UI::get().send_key(pair.first, false);
									break;
								}
							}
						}
					}
				}

				prev_state[i] = pressed;
			}

			// Left stick as D-pad
			bool axis_left  = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] < -AXIS_THRESHOLD;
			bool axis_right = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] > AXIS_THRESHOLD;
			bool axis_up    = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] < -AXIS_THRESHOLD;
			bool axis_down  = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y] > AXIS_THRESHOLD;

			if (axis_left != prev_axis_left)
				UI::get().send_key(GLFW_KEY_LEFT, axis_left);

			if (axis_right != prev_axis_right)
				UI::get().send_key(GLFW_KEY_RIGHT, axis_right);

			if (axis_up != prev_axis_up)
				UI::get().send_key(GLFW_KEY_UP, axis_up);

			if (axis_down != prev_axis_down)
				UI::get().send_key(GLFW_KEY_DOWN, axis_down);

			prev_axis_left = axis_left;
			prev_axis_right = axis_right;
			prev_axis_up = axis_up;
			prev_axis_down = axis_down;
		}
	}

	bool Gamepad::is_connected() const
	{
		return connected;
	}

	std::string Gamepad::get_name() const
	{
		return name;
	}

	void Gamepad::set_mapping(Button btn, KeyAction::Id action)
	{
		button_map[btn] = action;
	}

	KeyAction::Id Gamepad::get_mapping(Button btn) const
	{
		auto it = button_map.find(btn);

		if (it != button_map.end())
			return it->second;

		return KeyAction::Id::LENGTH;
	}

	std::map<Gamepad::Button, KeyAction::Id>& Gamepad::get_mappings()
	{
		return button_map;
	}

	int32_t Gamepad::get_last_pressed() const
	{
		return last_pressed;
	}

	void Gamepad::clear_last_pressed()
	{
		last_pressed = GP_NONE;
	}

	std::string Gamepad::get_button_name(int32_t btn)
	{
		switch (btn)
		{
		case GP_A:             return "A";
		case GP_B:             return "B";
		case GP_X:             return "X";
		case GP_Y:             return "Y";
		case GP_LEFT_BUMPER:   return "LB";
		case GP_RIGHT_BUMPER:  return "RB";
		case GP_BACK:          return "Back";
		case GP_START:         return "Start";
		case GP_LEFT_THUMB:    return "LS";
		case GP_RIGHT_THUMB:   return "RS";
		case GP_DPAD_UP:       return "D-Up";
		case GP_DPAD_RIGHT:    return "D-Right";
		case GP_DPAD_DOWN:     return "D-Down";
		case GP_DPAD_LEFT:     return "D-Left";
		default:               return "Btn " + std::to_string(btn);
		}
	}
}
