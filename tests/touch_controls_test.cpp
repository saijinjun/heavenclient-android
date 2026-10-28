#include <doctest/doctest.h>
#include "client/IO/KeyAction.h"
#include "client/IO/TouchControls.h"

#include <algorithm>
#include <vector>

namespace
{
    using Actions = std::vector<jrc::TouchControls::Action>;

    // A 20:9 phone held sideways. One layout unit is a sixth of the height.
    constexpr float WIDTH = 2400.0f;
    constexpr float HEIGHT = 1080.0f;
    constexpr float UNIT = HEIGHT / 6.0f;

    // Control centers, matching TouchControls::area.
    constexpr float DPAD_X = 1.6f * UNIT;
    constexpr float DPAD_Y = HEIGHT - 1.6f * UNIT;
    constexpr float ATTACK_X = WIDTH - 1.5f * UNIT;
    constexpr float ATTACK_Y = HEIGHT - 1.5f * UNIT;
    constexpr float JUMP_X = WIDTH - 3.3f * UNIT;
    constexpr float JUMP_Y = HEIGHT - 1.0f * UNIT;

    jrc::TouchControls phone()
    {
        jrc::TouchControls controls;
        controls.set_screen(static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT));
        return controls;
    }

    bool has(const Actions& actions, int32_t action, bool pressed)
    {
        return std::any_of(actions.begin(), actions.end(), [&](const auto& a) {
            return a.action == action && a.pressed == pressed;
        });
    }
}

TEST_CASE("A touch on the game area is left for the cursor")
{
    auto controls = phone();
    Actions actions;

    CHECK_FALSE(controls.finger_down(1, WIDTH / 2, HEIGHT / 2, actions));
    CHECK_FALSE(controls.finger_move(1, WIDTH / 2 + 10, HEIGHT / 2, actions));
    CHECK_FALSE(controls.finger_up(1, actions));
    CHECK(actions.empty());
}

TEST_CASE("The d-pad presses and releases directions as the thumb moves")
{
    auto controls = phone();
    Actions actions;

    // Dead center presses nothing but still owns the finger.
    CHECK(controls.finger_down(1, DPAD_X, DPAD_Y, actions));
    CHECK(actions.empty());

    CHECK(controls.finger_move(1, DPAD_X + UNIT, DPAD_Y, actions));
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::RIGHT, true));

    // Sliding across to the left swaps directions without lifting.
    actions.clear();
    controls.finger_move(1, DPAD_X - UNIT, DPAD_Y, actions);
    CHECK(has(actions, jrc::KeyAction::RIGHT, false));
    CHECK(has(actions, jrc::KeyAction::LEFT, true));

    // Dragging past the edge keeps steering.
    actions.clear();
    controls.finger_move(1, DPAD_X - 4 * UNIT, DPAD_Y, actions);
    CHECK(actions.empty());

    actions.clear();
    CHECK(controls.finger_up(1, actions));
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::LEFT, false));
}

TEST_CASE("A mostly sideways drag does not press up or down")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, DPAD_X, DPAD_Y, actions);
    controls.finger_move(1, DPAD_X + UNIT, DPAD_Y - 0.3f * UNIT, actions);
    CHECK(has(actions, jrc::KeyAction::RIGHT, true));
    CHECK_FALSE(has(actions, jrc::KeyAction::UP, true));

    // A clear diagonal presses both, e.g. down and left.
    actions.clear();
    controls.finger_move(1, DPAD_X - UNIT, DPAD_Y + UNIT, actions);
    CHECK(has(actions, jrc::KeyAction::LEFT, true));
    CHECK(has(actions, jrc::KeyAction::DOWN, true));
}

TEST_CASE("Moving and attacking with two thumbs at once")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, DPAD_X + UNIT, DPAD_Y, actions);
    CHECK(controls.finger_down(2, ATTACK_X, ATTACK_Y, actions));
    CHECK(controls.finger_down(3, JUMP_X, JUMP_Y, actions));
    CHECK(has(actions, jrc::KeyAction::RIGHT, true));
    CHECK(has(actions, jrc::KeyAction::ATTACK, true));
    CHECK(has(actions, jrc::KeyAction::JUMP, true));

    // Letting go of attack leaves movement and jump held.
    actions.clear();
    controls.finger_up(2, actions);
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::ATTACK, false));

    actions.clear();
    controls.release_all(actions);
    CHECK(has(actions, jrc::KeyAction::RIGHT, false));
    CHECK(has(actions, jrc::KeyAction::JUMP, false));
    CHECK_FALSE(controls.finger_up(1, actions));
}

TEST_CASE("A button stays held while any finger is on it")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, ATTACK_X, ATTACK_Y, actions);
    controls.finger_down(2, ATTACK_X + 5, ATTACK_Y, actions);
    REQUIRE(actions.size() == 1);

    actions.clear();
    controls.finger_up(1, actions);
    CHECK(actions.empty());

    controls.finger_up(2, actions);
    CHECK(has(actions, jrc::KeyAction::ATTACK, false));
}

TEST_CASE("Controls fit on screen and are drawn only once sized")
{
    jrc::TouchControls unsized;
    CHECK(unsized.shapes().empty());

    for (auto shape : phone().shapes())
    {
        CHECK(shape.left >= 0);
        CHECK(shape.top >= 0);
        CHECK(shape.right <= WIDTH);
        CHECK(shape.bottom <= HEIGHT);
        CHECK(shape.left < shape.right);
        CHECK(shape.top < shape.bottom);
    }
}
