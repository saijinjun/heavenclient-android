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

#include <cstdint>
#include <vector>

namespace jrc
{
    // The artwork for the on-screen touch controls: round buttons and white
    // icons, drawn once at startup from simple shapes so they stay sharp at
    // any screen size. Everything is white or grey with alpha; the controls
    // tint it when drawing.
    namespace TouchSprites
    {
        enum Sprite : uint8_t
        {
            DISC,       // Filled button face, lighter at the top and rim.
            RING,       // Thin outline around a button or the stick.
            ARROWS,     // Four small arrows around the stick.
            SWORD,      // Attack
            JUMP,       // Two chevrons pointing up
            PICKUP,     // Arrow into a tray
            BAG,        // Inventory
            SHIRT,      // Equipment
            BARS,       // Character stats
            STAR,       // Skills
            QUEST,      // Quest log: an exclamation mark
            GLOBE,      // World map
            PLUS,       // An empty quick slot
            DIGIT0,     // Digits 0-9 for item counts, in order
            DIGIT9 = DIGIT0 + 9,
            NUM_SPRITES
        };

        // Each sprite is a CELL x CELL square in a sheet COLUMNS cells wide.
        // The sheet has a one pixel border so no texture coordinate is 0,
        // which the shader reserves for solid colors.
        constexpr int32_t CELL = 256;
        constexpr int32_t COLUMNS = 8;
        constexpr int32_t ROWS = (NUM_SPRITES + COLUMNS - 1) / COLUMNS;
        constexpr int32_t BORDER = 1;
        constexpr int32_t WIDTH = COLUMNS * CELL + 2 * BORDER;
        constexpr int32_t HEIGHT = ROWS * CELL + 2 * BORDER;

        struct Cell
        {
            int16_t left;
            int16_t top;
            int16_t right;
            int16_t bottom;
        };

        Cell cell(Sprite sprite);

        // The whole sheet, WIDTH x HEIGHT RGBA pixels.
        std::vector<uint8_t> render();
    }
}
