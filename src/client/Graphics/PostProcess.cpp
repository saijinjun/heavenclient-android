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
#include "PostProcess.h"

#ifdef MS_PLATFORM_SDL
#include "../Console.h"

#include <string>

namespace jrc
{
    namespace
    {
        const char* VERTEX =
            "attribute vec2 pos;\n"
            "varying vec2 uv;\n"
            "void main() {\n"
            "    uv = pos * 0.5 + 0.5;\n"
            "    gl_Position = vec4(pos, 0.0, 1.0);\n"
            "}\n";

        const char* COPY =
            "precision highp float;\n"
            "varying vec2 uv;\n"
            "uniform sampler2D src;\n"
            "void main() { gl_FragColor = texture2D(src, uv); }\n";

        // Separable Gaussian, nine taps folded into five linear samples.
        const char* BLUR =
            "precision highp float;\n"
            "varying vec2 uv;\n"
            "uniform sampler2D src;\n"
            "uniform vec2 dir;\n"
            "void main() {\n"
            "    vec4 c = texture2D(src, uv) * 0.2270270270;\n"
            "    c += texture2D(src, uv + dir * 1.3846153846) * 0.3162162162;\n"
            "    c += texture2D(src, uv - dir * 1.3846153846) * 0.3162162162;\n"
            "    c += texture2D(src, uv + dir * 3.2307692308) * 0.0702702703;\n"
            "    c += texture2D(src, uv - dir * 3.2307692308) * 0.0702702703;\n"
            "    gl_FragColor = c;\n"
            "}\n";

        // Keeps only the bright parts, at half resolution, for the glow.
        const char* BRIGHT =
            "precision highp float;\n"
            "varying vec2 uv;\n"
            "uniform sampler2D src;\n"
            "void main() {\n"
            "    vec3 c = texture2D(src, uv).rgb;\n"
            "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
            "    gl_FragColor = vec4(c * smoothstep(0.72, 1.0, l), 1.0);\n"
            "}\n";

        // The blurred far background, a little washed out like distant air.
        const char* BACKGROUND =
            "precision highp float;\n"
            "varying vec2 uv;\n"
            "uniform sampler2D src;\n"
            "void main() {\n"
            "    vec3 c = texture2D(src, uv).rgb;\n"
            "    float l = dot(c, vec3(0.299, 0.587, 0.114));\n"
            "    c = mix(c, vec3(l), 0.15);\n"
            "    c = mix(c, vec3(0.86, 0.91, 1.0), 0.07);\n"
            "    gl_FragColor = vec4(c, 1.0);\n"
            "}\n";

        // Upscaling follows the idea of xBR: at each texel corner, compare
        // weighted color differences across the two diagonals. When the edge
        // runs along the corner, the corner is cut with a smooth diagonal
        // instead of a stair step.
        const char* COMPOSITE =
            "precision highp float;\n"
            "varying vec2 uv;\n"
            "uniform sampler2D scene;\n"
            "uniform sampler2D glowmap;\n"
            "uniform vec2 texsize;\n"
            "uniform float outscale;\n"
            "uniform float upscale;\n"
            "uniform float glow;\n"
            "uniform float grade;\n"
            "const vec3 LUMA = vec3(0.299, 0.587, 0.114);\n"
            "vec3 T(vec2 p) { return texture2D(scene, (p + 0.5) / texsize).rgb; }\n"
            "float d(vec3 a, vec3 b) { return dot(abs(a - b), LUMA); }\n"
            "vec3 smooth_edges(vec2 pos) {\n"
            "    vec2 base = floor(pos + 0.5);\n"
            "    vec2 f = pos - base;\n"
            "    vec2 s = vec2(f.x >= 0.0 ? 1.0 : -1.0, f.y >= 0.0 ? 1.0 : -1.0);\n"
            "    vec3 E = T(base);\n"
            "    vec3 B = T(base - vec2(0.0, s.y));\n"
            "    vec3 D = T(base - vec2(s.x, 0.0));\n"
            "    vec3 F = T(base + vec2(s.x, 0.0));\n"
            "    vec3 H = T(base + vec2(0.0, s.y));\n"
            "    vec3 I = T(base + s);\n"
            "    vec3 C = T(base + vec2(s.x, -s.y));\n"
            "    vec3 G = T(base + vec2(-s.x, s.y));\n"
            "    vec3 F4 = T(base + vec2(2.0 * s.x, 0.0));\n"
            "    vec3 H5 = T(base + vec2(0.0, 2.0 * s.y));\n"
            "    vec3 I4 = T(base + vec2(2.0 * s.x, s.y));\n"
            "    vec3 I5 = T(base + vec2(s.x, 2.0 * s.y));\n"
            "    float across = d(E, C) + d(E, G) + d(I, F4) + d(I, H5) + 4.0 * d(H, F);\n"
            "    float along = d(H, D) + d(H, I5) + d(F, I4) + d(F, B) + 4.0 * d(E, I);\n"
            "    if (across >= along) return E;\n"
            "    vec3 P = d(E, F) <= d(E, H) ? F : H;\n"
            "    float aa = 0.5 / outscale;\n"
            "    float w = smoothstep(0.5 - aa, 0.5 + aa, abs(f.x) + abs(f.y));\n"
            "    return mix(E, P, w);\n"
            "}\n"
            "void main() {\n"
            "    vec2 pos = uv * texsize - 0.5;\n"
            "    vec3 c = upscale > 0.5 ? smooth_edges(pos) : T(floor(pos + 0.5));\n"
            "    if (glow > 0.5) {\n"
            "        c += texture2D(glowmap, uv).rgb * 0.4;\n"
            "    }\n"
            "    if (grade > 0.5) {\n"
            "        float l = dot(c, LUMA);\n"
            "        c = mix(vec3(l), c, 1.12);\n"
            "        c = (c - 0.5) * 1.06 + 0.5;\n"
            "        c *= vec3(1.03, 1.0, 0.96);\n"
            "        c += (1.0 - l) * vec3(-0.01, 0.0, 0.02);\n"
            "        vec2 v = (uv - 0.5) * vec2(1.0, 1.25);\n"
            "        c *= 1.0 - 0.45 * dot(v, v);\n"
            "    }\n"
            "    gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
            "}\n";

        GLuint compile(GLenum type, const char* source)
        {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &source, nullptr);
            glCompileShader(shader);

            GLint ok = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (!ok)
            {
                char log[512] = {};
                glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
                Console::get().print("[HD] Shader failed: " + std::string(log));
                glDeleteShader(shader);
                return 0;
            }

            return shader;
        }

        GLuint link(const char* fragment)
        {
            GLuint vs = compile(GL_VERTEX_SHADER, VERTEX);
            GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
            if (!vs || !fs)
            {
                return 0;
            }

            GLuint program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glBindAttribLocation(program, 0, "pos");
            glLinkProgram(program);
            glDeleteShader(vs);
            glDeleteShader(fs);

            GLint ok = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &ok);
            if (!ok)
            {
                glDeleteProgram(program);
                return 0;
            }

            return program;
        }
    }

    bool PostProcess::init()
    {
        copy_program = link(COPY);
        blur_program = link(BLUR);
        bright_program = link(BRIGHT);
        background_program = link(BACKGROUND);
        composite_program = link(COMPOSITE);
        programs_ready = copy_program && blur_program && bright_program &&
            background_program && composite_program;

        static const GLfloat QUAD[] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
        glGenBuffers(1, &quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD), QUAD, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        if (!programs_ready)
        {
            Console::get().print("[HD] Effects are unavailable on this device.");
        }

        return programs_ready;
    }

    void PostProcess::set_options(const Options& in_options)
    {
        options = in_options;
    }

    void PostProcess::make_target(Target& target, int32_t target_width, int32_t target_height)
    {
        free_target(target);

        glGenTextures(1, &target.texture);
        glBindTexture(GL_TEXTURE_2D, target.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_width, target_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glGenFramebuffers(1, &target.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        target.width = target_width;
        target.height = target_height;
    }

    void PostProcess::free_target(Target& target)
    {
        if (target.framebuffer)
        {
            glDeleteFramebuffers(1, &target.framebuffer);
        }
        if (target.texture)
        {
            glDeleteTextures(1, &target.texture);
        }
        target = {};
    }

    void PostProcess::resize(int32_t new_width, int32_t new_height)
    {
        if (new_width == width && new_height == height)
        {
            return;
        }

        width = new_width;
        height = new_height;
        int32_t half_w = (width + 1) / 2;
        int32_t half_h = (height + 1) / 2;

        make_target(background, width, height);
        make_target(world, width, height);
        make_target(half_a, half_w, half_h);
        make_target(half_b, half_w, half_h);
    }

    void PostProcess::bind(const Target& target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
        glViewport(0, 0, target.width, target.height);
    }

    void PostProcess::draw_fullscreen(GLuint program)
    {
        glUseProgram(program);
        glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glDisableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void PostProcess::blur(const Target& source, Target& middle, Target& out, float spread)
    {
        glUseProgram(blur_program);
        glUniform1i(glGetUniformLocation(blur_program, "src"), 0);
        GLint dir = glGetUniformLocation(blur_program, "dir");

        bind(middle);
        glBindTexture(GL_TEXTURE_2D, source.texture);
        glUniform2f(dir, spread / middle.width, 0.0f);
        draw_fullscreen(blur_program);

        bind(out);
        glBindTexture(GL_TEXTURE_2D, middle.texture);
        glUseProgram(blur_program);
        glUniform2f(dir, 0.0f, spread / out.height);
        draw_fullscreen(blur_program);
    }

    void PostProcess::begin_background()
    {
        bind(background);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void PostProcess::begin_world(bool blurred_background)
    {
        glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0);

        if (blurred_background)
        {
            // Half resolution, blurred twice: soft but cheap.
            bind(half_a);
            glBindTexture(GL_TEXTURE_2D, background.texture);
            glUseProgram(copy_program);
            glUniform1i(glGetUniformLocation(copy_program, "src"), 0);
            draw_fullscreen(copy_program);
            blur(half_a, half_b, half_a, 1.0f);
            blur(half_a, half_b, half_a, 1.6f);

            bind(world);
            glBindTexture(GL_TEXTURE_2D, half_a.texture);
            glUseProgram(background_program);
            glUniform1i(glGetUniformLocation(background_program, "src"), 0);
            draw_fullscreen(background_program);
        }
        else
        {
            bind(world);
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        glEnable(GL_BLEND);
    }

    void PostProcess::present(GLuint target, GLint x, GLint y, GLsizei out_width, GLsizei out_height)
    {
        glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0);

        if (options.glow)
        {
            bind(half_a);
            glBindTexture(GL_TEXTURE_2D, world.texture);
            glUseProgram(bright_program);
            glUniform1i(glGetUniformLocation(bright_program, "src"), 0);
            draw_fullscreen(bright_program);
            blur(half_a, half_b, half_a, 1.0f);
            blur(half_a, half_b, half_a, 2.0f);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, target);
        glViewport(x, y, out_width, out_height);

        glUseProgram(composite_program);
        glUniform1i(glGetUniformLocation(composite_program, "scene"), 0);
        glUniform1i(glGetUniformLocation(composite_program, "glowmap"), 1);
        glUniform2f(glGetUniformLocation(composite_program, "texsize"),
            static_cast<GLfloat>(width), static_cast<GLfloat>(height));
        glUniform1f(glGetUniformLocation(composite_program, "outscale"),
            static_cast<GLfloat>(out_height) / static_cast<GLfloat>(height));
        glUniform1f(glGetUniformLocation(composite_program, "upscale"), options.upscale ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(composite_program, "glow"), options.glow ? 1.0f : 0.0f);
        glUniform1f(glGetUniformLocation(composite_program, "grade"), options.grade ? 1.0f : 0.0f);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, half_a.texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, world.texture);
        draw_fullscreen(composite_program);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_BLEND);
    }
}
#endif
