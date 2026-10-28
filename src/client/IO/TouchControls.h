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
#include <unordered_map>
#include <vector>

namespace jrc
{
    // On-screen controls for touch screens: a d-pad in the bottom-left corner
    // and jump, attack and pick-up buttons in the bottom-right corner.
    //
    // Positions are in screen pixels with the origin at the top left. Sizes
    // scale with the screen height so the controls stay thumb-sized on any
    // phone, and on wide screens they sit in the black bars beside the game.
    // This class only tracks fingers and reports game actions; the window
    // feeds it touch events and draws the shapes it returns.
    class TouchControls
    {
    public:
        // A game action turning on or off, using KeyAction ids.
        struct Action
        {
            int32_t action;
            bool pressed;
        };

        struct Shape
        {
            int16_t left;
            int16_t top;
            int16_t right;
            int16_t bottom;
            float red;
            float green;
            float blue;
            float alpha;
        };

        void set_screen(int32_t width, int32_t height);

        // Each returns true when the finger belongs to a control, so the
        // caller must not also treat it as a tap on the game.
        bool finger_down(int64_t finger, float x, float y, std::vector<Action>& actions);
        bool finger_move(int64_t finger, float x, float y, std::vector<Action>& actions);
        bool finger_up(int64_t finger, std::vector<Action>& actions);

        // Lets go of everything, e.g. when leaving the game screen.
        void release_all(std::vector<Action>& actions);

        std::vector<Shape> shapes() const;

    private:
        enum Control : uint8_t
        {
            DPAD,
            JUMP,
            ATTACK,
            PICKUP,
            NUM_CONTROLS
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

            bool contains(float px, float py) const;
        };

        Circle area(Control control) const;
        bool control_at(float x, float y, Control& control) const;
        void aim_dpad(float x, float y, std::vector<Action>& actions);
        void set_direction(Direction direction, bool pressed, std::vector<Action>& actions);
        void set_button(Control control, bool pressed, std::vector<Action>& actions);
        uint8_t fingers_on(Control control) const;

        float width = 0.0f;
        float height = 0.0f;
        std::unordered_map<int64_t, Control> fingers;
        std::array<bool, NUM_DIRECTIONS> directions = {};
        std::array<bool, NUM_CONTROLS> buttons = {};
    };
}
