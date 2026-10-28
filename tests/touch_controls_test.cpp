#include <doctest/doctest.h>
#include "client/IO/KeyAction.h"
#include "client/IO/TouchControls.h"
#include "client/IO/TouchSprites.h"

#include <algorithm>
#include <vector>

namespace
{
    using Controls = jrc::TouchControls;
    using Actions = std::vector<Controls::Action>;
    using Output = Controls::Output;

    // A 20:9 phone held sideways (2800 x 1260). One layout unit is a sixth
    // of the height. The 4:3 game view in the middle leaves a black bar of
    // 560 pixels on each side.
    constexpr float WIDTH = 2800.0f;
    constexpr float HEIGHT = 1260.0f;
    constexpr float UNIT = HEIGHT / 6.0f;
    constexpr float BAR = (WIDTH - HEIGHT * 4.0f / 3.0f) / 2.0f;

    // Control centers, matching PLACES in TouchControls.cpp.
    constexpr float STICK_X = 1.4f * UNIT;
    constexpr float STICK_Y = HEIGHT - 1.5f * UNIT;
    constexpr float ATTACK_X = WIDTH - 1.15f * UNIT;
    constexpr float ATTACK_Y = HEIGHT - 1.2f * UNIT;
    constexpr float JUMP_X = WIDTH - 2.2f * UNIT;
    constexpr float JUMP_Y = HEIGHT - 0.55f * UNIT;
    constexpr float SLOT0_X = WIDTH - 2.25f * UNIT;
    constexpr float SLOT0_Y = HEIGHT - 1.85f * UNIT;
    constexpr float INVENTORY_X = 0.5f * UNIT;
    constexpr float INVENTORY_Y = 1.85f * UNIT;

    Controls phone()
    {
        Controls controls;
        controls.set_screen(static_cast<int32_t>(WIDTH), static_cast<int32_t>(HEIGHT));
        return controls;
    }

    bool has(const Actions& actions, Output output, int32_t id, bool pressed)
    {
        return std::any_of(actions.begin(), actions.end(), [&](const auto& a) {
            return a.output == output && a.id == id && a.pressed == pressed;
        });
    }

    bool has(const Actions& actions, int32_t action, bool pressed)
    {
        return has(actions, Output::ACTION, action, pressed);
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

TEST_CASE("The stick appears under the thumb and steers as it moves")
{
    auto controls = phone();
    Actions actions;

    // Anywhere in the lower left works, not just the drawn stick.
    float x = STICK_X + 0.7f * UNIT;
    float y = STICK_Y + 0.6f * UNIT;
    CHECK(controls.finger_down(1, x, y, actions));
    CHECK(actions.empty());

    CHECK(controls.finger_move(1, x + 0.5f * UNIT, y, actions));
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::RIGHT, true));

    // Sliding across to the left swaps directions without lifting.
    actions.clear();
    controls.finger_move(1, x - 0.5f * UNIT, y, actions);
    CHECK(has(actions, jrc::KeyAction::RIGHT, false));
    CHECK(has(actions, jrc::KeyAction::LEFT, true));

    // Dragging far past the stick keeps steering.
    actions.clear();
    controls.finger_move(1, x - 4 * UNIT, y, actions);
    CHECK(actions.empty());

    actions.clear();
    CHECK(controls.finger_up(1, actions));
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::LEFT, false));
}

TEST_CASE("Touching the stick right at the screen corner presses nothing")
{
    auto controls = phone();
    Actions actions;

    CHECK(controls.finger_down(1, 5.0f, HEIGHT - 5.0f, actions));
    CHECK(actions.empty());
}

TEST_CASE("A mostly sideways push does not press up or down")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, STICK_X, STICK_Y, actions);
    controls.finger_move(1, STICK_X + UNIT, STICK_Y - 0.3f * UNIT, actions);
    CHECK(has(actions, jrc::KeyAction::RIGHT, true));
    CHECK_FALSE(has(actions, jrc::KeyAction::UP, true));

    // A clear diagonal presses both, e.g. down and left.
    actions.clear();
    controls.finger_move(1, STICK_X - UNIT, STICK_Y + UNIT, actions);
    CHECK(has(actions, jrc::KeyAction::LEFT, true));
    CHECK(has(actions, jrc::KeyAction::DOWN, true));
}

TEST_CASE("Moving, attacking and jumping with several fingers at once")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, STICK_X, STICK_Y, actions);
    controls.finger_move(1, STICK_X + UNIT, STICK_Y, actions);
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

TEST_CASE("A second thumb in the stick area does not move the stick")
{
    auto controls = phone();
    Actions actions;

    controls.finger_down(1, STICK_X, STICK_Y, actions);
    CHECK(controls.finger_down(2, STICK_X + UNIT, STICK_Y, actions));
    CHECK(controls.finger_move(2, STICK_X + 2 * UNIT, STICK_Y, actions));
    CHECK(actions.empty());
    CHECK(controls.finger_up(2, actions));
    CHECK(actions.empty());
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

TEST_CASE("A menu button opens its window once per tap")
{
    auto controls = phone();
    Actions actions;

    CHECK(controls.finger_down(1, INVENTORY_X, INVENTORY_Y, actions));
    CHECK(controls.finger_up(1, actions));
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, Output::MENU, jrc::KeyAction::INVENTORY, true));
}

TEST_CASE("Quick slots work once something is on them")
{
    auto controls = phone();
    Actions actions;

    // Empty: the touch is not taken, and nothing is sent.
    CHECK_FALSE(controls.finger_down(1, SLOT0_X, SLOT0_Y, actions));
    CHECK(actions.empty());

    controls.set_slot_filled(0, true);
    CHECK(controls.finger_down(2, SLOT0_X, SLOT0_Y, actions));
    CHECK(has(actions, Output::SLOT, 0, true));
    controls.finger_up(2, actions);
    CHECK(has(actions, Output::SLOT, 0, false));

    // Dropping onto a slot finds it whether it is filled or not.
    uint8_t slot = 99;
    CHECK(controls.slot_at(SLOT0_X, SLOT0_Y, slot));
    CHECK(slot == 0);
    CHECK_FALSE(controls.slot_at(WIDTH / 2, HEIGHT / 2, slot));
}

TEST_CASE("On a wide phone every control sits in the black bars")
{
    jrc::TouchControls unsized;
    CHECK(unsized.draws().empty());

    auto controls = phone();
    for (uint8_t slot = 0; slot < Controls::NUM_SLOTS; slot++)
    {
        controls.set_slot_filled(slot, true);
    }

    for (const auto& draw : controls.draws())
    {
        float left = draw.x - draw.half;
        float right = draw.x + draw.half;
        CHECK(left >= 0.0f);
        CHECK(right <= WIDTH);
        CHECK(draw.y - draw.half >= 0.0f);
        CHECK(draw.y + draw.half <= HEIGHT);
        CHECK((right <= BAR || left >= WIDTH - BAR));
    }
}

TEST_CASE("Every touch control sprite has artwork")
{
    std::vector<uint8_t> sheet = jrc::TouchSprites::render();
    REQUIRE(sheet.size() == static_cast<size_t>(jrc::TouchSprites::WIDTH) * jrc::TouchSprites::HEIGHT * 4);

    for (uint8_t s = 0; s < jrc::TouchSprites::NUM_SPRITES; s++)
    {
        auto cell = jrc::TouchSprites::cell(static_cast<jrc::TouchSprites::Sprite>(s));
        CHECK(cell.top >= 1);
        CHECK(cell.right <= jrc::TouchSprites::WIDTH);

        size_t covered = 0;
        for (int32_t y = cell.top; y < cell.bottom; y++)
        {
            for (int32_t x = cell.left; x < cell.right; x++)
            {
                covered += sheet[(static_cast<size_t>(y) * jrc::TouchSprites::WIDTH + x) * 4 + 3] > 0;
            }
        }
        CHECK(covered > 500);
    }
}

TEST_CASE("Slots show a plus when empty, and counts and cooldowns when filled")
{
    using Sprite = jrc::TouchSprites::Sprite;
    auto count_sprite = [](const Controls& controls, Sprite sprite) {
        auto draws = controls.draws();
        return std::count_if(draws.begin(), draws.end(), [&](const auto& d) {
            return d.kind == Controls::Draw::SPRITE && d.sprite == sprite;
        });
    };

    auto controls = phone();
    CHECK(count_sprite(controls, jrc::TouchSprites::PLUS) == Controls::NUM_SLOTS);

    // A potion with 25 left: two digits, each drawn with a shadow.
    controls.set_slot_filled(0, true);
    controls.set_slot_info(0, 25, false);
    CHECK(count_sprite(controls, jrc::TouchSprites::PLUS) == Controls::NUM_SLOTS - 1);
    CHECK(count_sprite(controls, static_cast<Sprite>(jrc::TouchSprites::DIGIT0 + 2)) == 2);
    CHECK(count_sprite(controls, static_cast<Sprite>(jrc::TouchSprites::DIGIT0 + 5)) == 2);

    // A skill cooling down gets a dark cover over its icon.
    size_t discs = count_sprite(controls, jrc::TouchSprites::DISC);
    controls.set_slot_info(0, -1, true);
    CHECK(count_sprite(controls, jrc::TouchSprites::DISC) == discs + 1);
}

TEST_CASE("The jump down button holds down, then jump")
{
    auto controls = phone();
    Actions actions;

    float x = WIDTH - 0.36f * UNIT;
    float y = HEIGHT - 0.40f * UNIT;
    CHECK(controls.finger_down(1, x, y, actions));
    REQUIRE(actions.size() == 2);
    CHECK(actions[0].id == jrc::KeyAction::DOWN);
    CHECK(actions[1].id == jrc::KeyAction::JUMP);

    actions.clear();
    controls.finger_up(1, actions);
    CHECK(has(actions, jrc::KeyAction::DOWN, false));
    CHECK(has(actions, jrc::KeyAction::JUMP, false));
}

TEST_CASE("The jump attack button jumps first, and shares keys with the other buttons")
{
    auto controls = phone();
    Actions actions;

    float x = WIDTH - 0.36f * UNIT;
    float y = HEIGHT - 1.98f * UNIT;
    controls.finger_down(1, x, y, actions);
    REQUIRE(actions.size() == 2);
    CHECK(actions[0].id == jrc::KeyAction::JUMP);
    CHECK(actions[1].id == jrc::KeyAction::ATTACK);

    // Attack is still held by its own button after the combo lifts.
    actions.clear();
    controls.finger_down(2, ATTACK_X, ATTACK_Y, actions);
    CHECK(actions.empty());
    controls.finger_up(1, actions);
    REQUIRE(actions.size() == 1);
    CHECK(has(actions, jrc::KeyAction::JUMP, false));
}

TEST_CASE("The layout editor moves and resizes controls and remembers it")
{
    auto controls = phone();
    Actions actions;

    // Holding attack, then opening the editor, lets go of attack.
    controls.finger_down(1, ATTACK_X, ATTACK_Y, actions);
    actions.clear();
    CHECK(controls.finger_down(2, WIDTH - 0.45f * UNIT, 0.45f * UNIT, actions));
    CHECK(controls.is_editing());
    CHECK(has(actions, jrc::KeyAction::ATTACK, false));
    controls.finger_up(1, actions);
    controls.finger_up(2, actions);

    // Drag attack one unit left, then make it bigger with the + tool.
    actions.clear();
    CHECK(controls.finger_down(3, ATTACK_X, ATTACK_Y, actions));
    CHECK(controls.finger_move(3, ATTACK_X - UNIT, ATTACK_Y, actions));
    controls.finger_up(3, actions);
    float tool_y = 0.55f * UNIT;
    controls.finger_down(4, WIDTH / 2 - 0.9f * UNIT, tool_y, actions);
    controls.finger_up(4, actions);
    CHECK(actions.empty());
    CHECK(controls.take_layout_changed());
    CHECK_FALSE(controls.take_layout_changed());

    // Done: touches work again, at the new place.
    controls.finger_down(5, WIDTH / 2 + 2.0f * 0.9f * UNIT, tool_y, actions);
    controls.finger_up(5, actions);
    CHECK_FALSE(controls.is_editing());
    controls.finger_down(6, ATTACK_X - UNIT, ATTACK_Y, actions);
    CHECK(has(actions, jrc::KeyAction::ATTACK, true));
    controls.finger_up(6, actions);

    // The saved text brings the same layout back.
    std::string layout = controls.get_layout();
    INFO(layout);
    CHECK(layout.find("ATTACK:-1.000,0.000,1.10") != std::string::npos);
    auto restored = phone();
    restored.set_layout(layout);
    CHECK(restored.get_layout() == layout);
    restored.set_layout("nonsense;ATTACK:x;opacity:");
    CHECK(restored.get_layout() == "opacity:1.00");
}
