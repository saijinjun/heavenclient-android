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

#include "KeyAction.h"

#include <algorithm>
#include <cmath>

namespace jrc
{
    namespace
    {
        // Fingers rarely land dead on a control, so each one accepts touches a
        // little outside the shape it draws.
        constexpr float HIT_SLACK = 1.25f;
        // How far from the d-pad center, as a share of its radius, a thumb
        // has to move before a direction counts.
        constexpr float DEAD_ZONE = 0.25f;
        // A direction only counts when it dominates the other axis by this
        // ratio, so a sideways drag does not also press up (portals) or down
        // (crouch), while a clear diagonal still presses both.
        constexpr float AXIS_RATIO = 0.5f;

        TouchControls::Shape square(float x, float y, float half, float r, float g, float b, float a)
        {
            return {
                static_cast<int16_t>(x - half), static_cast<int16_t>(y - half),
                static_cast<int16_t>(x + half), static_cast<int16_t>(y + half),
                r, g, b, a
            };
        }
    }

    bool TouchControls::Circle::contains(float px, float py) const
    {
        float dx = px - x;
        float dy = py - y;
        float reach = radius * HIT_SLACK;

        return dx * dx + dy * dy <= reach * reach;
    }

    void TouchControls::set_screen(int32_t screen_width, int32_t screen_height)
    {
        width = static_cast<float>(screen_width);
        height = static_cast<float>(screen_height);
    }

    TouchControls::Circle TouchControls::area(Control control) const
    {
        // One unit is a sixth of the screen height: about a thumb's width on
        // a phone held sideways.
        float unit = height / 6.0f;

        switch (control)
        {
        case DPAD:
            return { 1.6f * unit, height - 1.6f * unit, 1.35f * unit };
        case ATTACK:
            return { width - 1.5f * unit, height - 1.5f * unit, 0.8f * unit };
        case JUMP:
            return { width - 3.3f * unit, height - 1.0f * unit, 0.7f * unit };
        case PICKUP:
            return { width - 1.5f * unit, height - 3.3f * unit, 0.6f * unit };
        default:
            return { 0.0f, 0.0f, 0.0f };
        }
    }

    bool TouchControls::control_at(float x, float y, Control& control) const
    {
        for (uint8_t i = 0; i < NUM_CONTROLS; i++)
        {
            Control candidate = static_cast<Control>(i);

            if (area(candidate).contains(x, y))
            {
                control = candidate;
                return true;
            }
        }

        return false;
    }

    bool TouchControls::finger_down(int64_t finger, float x, float y, std::vector<Action>& actions)
    {
        Control control;

        if (!control_at(x, y, control))
        {
            return false;
        }

        fingers[finger] = control;

        if (control == DPAD)
        {
            aim_dpad(x, y, actions);
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

        // The d-pad keeps following its thumb even past its edge. Buttons stay
        // held until the finger lifts, wherever it slides.
        if (iter->second == DPAD)
        {
            aim_dpad(x, y, actions);
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

        // Another finger may still be holding the same control.
        if (fingers_on(control) == 0)
        {
            if (control == DPAD)
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

    void TouchControls::aim_dpad(float x, float y, std::vector<Action>& actions)
    {
        Circle dpad = area(DPAD);
        float dx = x - dpad.x;
        float dy = y - dpad.y;
        float dead = dpad.radius * DEAD_ZONE;

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

        static constexpr std::array<int32_t, NUM_DIRECTIONS> ACTIONS =
        {
            KeyAction::LEFT, KeyAction::RIGHT, KeyAction::UP, KeyAction::DOWN
        };

        actions.push_back({ ACTIONS[direction], pressed });
    }

    void TouchControls::set_button(Control control, bool pressed, std::vector<Action>& actions)
    {
        if (buttons[control] == pressed)
        {
            return;
        }

        buttons[control] = pressed;

        switch (control)
        {
        case JUMP:
            actions.push_back({ KeyAction::JUMP, pressed });
            break;
        case ATTACK:
            actions.push_back({ KeyAction::ATTACK, pressed });
            break;
        case PICKUP:
            actions.push_back({ KeyAction::PICKUP, pressed });
            break;
        default:
            break;
        }
    }

    std::vector<TouchControls::Shape> TouchControls::shapes() const
    {
        std::vector<Shape> result;

        if (width <= 0.0f || height <= 0.0f)
        {
            return result;
        }

        // D-pad: a plus sign whose arms light up while pressed.
        Circle dpad = area(DPAD);
        float arm = dpad.radius / 3.0f;
        float idle = 0.22f;
        float lit = 0.55f;

        result.push_back(square(dpad.x, dpad.y, arm, 1.0f, 1.0f, 1.0f, idle));
        result.push_back({
            static_cast<int16_t>(dpad.x - dpad.radius), static_cast<int16_t>(dpad.y - arm),
            static_cast<int16_t>(dpad.x - arm), static_cast<int16_t>(dpad.y + arm),
            1.0f, 1.0f, 1.0f, directions[LEFT] ? lit : idle
        });
        result.push_back({
            static_cast<int16_t>(dpad.x + arm), static_cast<int16_t>(dpad.y - arm),
            static_cast<int16_t>(dpad.x + dpad.radius), static_cast<int16_t>(dpad.y + arm),
            1.0f, 1.0f, 1.0f, directions[RIGHT] ? lit : idle
        });
        result.push_back({
            static_cast<int16_t>(dpad.x - arm), static_cast<int16_t>(dpad.y - dpad.radius),
            static_cast<int16_t>(dpad.x + arm), static_cast<int16_t>(dpad.y - arm),
            1.0f, 1.0f, 1.0f, directions[UP] ? lit : idle
        });
        result.push_back({
            static_cast<int16_t>(dpad.x - arm), static_cast<int16_t>(dpad.y + arm),
            static_cast<int16_t>(dpad.x + arm), static_cast<int16_t>(dpad.y + dpad.radius),
            1.0f, 1.0f, 1.0f, directions[DOWN] ? lit : idle
        });

        // Buttons: a colored square with a lighter face, brighter while held.
        // Attack is red, jump green, pick up yellow.
        struct Style
        {
            Control control;
            float r;
            float g;
            float b;
        };

        static constexpr std::array<Style, 3> STYLES =
        {{
            { ATTACK, 0.90f, 0.25f, 0.25f },
            { JUMP,   0.25f, 0.80f, 0.35f },
            { PICKUP, 0.95f, 0.80f, 0.20f }
        }};

        for (const Style& style : STYLES)
        {
            Circle button = area(style.control);
            float alpha = buttons[style.control] ? 0.70f : 0.35f;

            result.push_back(square(button.x, button.y, button.radius, style.r, style.g, style.b, alpha));
            result.push_back(square(button.x, button.y, button.radius * 0.7f, 1.0f, 1.0f, 1.0f, alpha * 0.4f));
        }

        return result;
    }
}
