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

#ifdef MS_PLATFORM_SDL
#include "GLHeaders.h"

#include <cstdint>

namespace jrc
{
    // The "HD-2D" look for the game world on phones and desktop SDL builds.
    // The world is drawn at game resolution into an offscreen image, then
    // shown on screen through these optional effects:
    //
    // - upscale: a pixel-art upscaler that smooths diagonal edges instead of
    //   showing stair steps when the image is scaled up;
    // - glow: bright parts of the picture bleed soft light around them;
    // - depth: the far background layers are blurred and slightly hazy, so
    //   the playfield stands out like a tilt-shift photo;
    // - grade: a gentle color grade (saturation, contrast, warmth) and a
    //   vignette.
    //
    // The UI is drawn afterwards, straight to the screen, so it stays sharp.
    class PostProcess
    {
    public:
        struct Options
        {
            bool upscale = true;
            bool glow = true;
            bool depth = true;
            bool grade = true;

            bool any() const { return upscale || glow || depth || grade; }
        };

        bool init();
        bool ready() const { return programs_ready; }
        void set_options(const Options& options);
        const Options& get_options() const { return options; }

        // Game resolution; (re)creates the offscreen images when it changes.
        void resize(int32_t width, int32_t height);

        // With depth: draw the background layers after begin_background(),
        // then call begin_world(true), which blurs them into the world image
        // and leaves it bound for the rest of the world. Otherwise call
        // begin_world(false) and draw everything after it.
        void begin_background();
        void begin_world(bool blurred_background);

        // Shows the world image in the given rectangle of the target
        // framebuffer (0 = the screen), through the enabled effects.
        void present(GLuint target, GLint x, GLint y, GLsizei width, GLsizei height);

    private:
        struct Target
        {
            GLuint framebuffer = 0;
            GLuint texture = 0;
            int32_t width = 0;
            int32_t height = 0;
        };

        void make_target(Target& target, int32_t width, int32_t height);
        void free_target(Target& target);
        void bind(const Target& target);
        void draw_fullscreen(GLuint program);
        void blur(const Target& source, Target& middle, Target& out, float spread);

        Options options;
        bool programs_ready = false;

        GLuint quad_vbo = 0;
        GLuint copy_program = 0;
        GLuint blur_program = 0;
        GLuint bright_program = 0;
        GLuint background_program = 0;
        GLuint composite_program = 0;

        Target background;
        Target world;
        Target half_a;
        Target half_b;
        int32_t width = 0;
        int32_t height = 0;
    };
}
#endif
