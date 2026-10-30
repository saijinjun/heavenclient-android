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
#include "TouchSprites.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <string>

#include <SDL.h>
#include <stb_image.h>

namespace ms
{
	namespace TouchSprites
	{
		namespace
		{
			// Shapes are signed distance functions over a cell spanning -1 to
			// 1 on both axes, y pointing down: negative inside, positive
			// outside. Turning distance into coverage gives smooth edges.
			struct Vec
			{
				float x;
				float y;
			};

			Vec operator-(Vec a, Vec b) { return { a.x - b.x, a.y - b.y }; }
			Vec operator*(Vec a, float s) { return { a.x * s, a.y * s }; }
			float dot(Vec a, Vec b) { return a.x * b.x + a.y * b.y; }
			float length(Vec a) { return std::sqrt(dot(a, a)); }

			float circle(Vec p, Vec center, float radius)
			{
				return length(p - center) - radius;
			}

			float ring(Vec p, Vec center, float radius, float thickness)
			{
				return std::fabs(length(p - center) - radius) - thickness;
			}

			// A line from a to b with round ends.
			float capsule(Vec p, Vec a, Vec b, float radius)
			{
				Vec pa = p - a;
				Vec ba = b - a;
				float h = std::clamp(dot(pa, ba) / dot(ba, ba), 0.0f, 1.0f);
				return length(pa - ba * h) - radius;
			}

			// A connected line through the points.
			float polyline(Vec p, std::initializer_list<Vec> points, float radius)
			{
				float d = 1e9f;
				const Vec* prev = nullptr;
				for (const Vec& point : points)
				{
					if (prev)
					{
						d = std::min(d, capsule(p, *prev, point, radius));
					}
					prev = &point;
				}
				return d;
			}

			float rounded_box(Vec p, Vec center, Vec half, float radius)
			{
				Vec q = { std::fabs(p.x - center.x) - half.x + radius,
						  std::fabs(p.y - center.y) - half.y + radius };
				Vec outside = { std::max(q.x, 0.0f), std::max(q.y, 0.0f) };
				return length(outside) + std::min(std::max(q.x, q.y), 0.0f) - radius;
			}

			template <size_t N>
			float polygon(Vec p, const std::array<Vec, N>& v)
			{
				float d = dot(p - v[0], p - v[0]);
				float sign = 1.0f;
				for (size_t i = 0, j = N - 1; i < N; j = i, i++)
				{
					Vec e = v[j] - v[i];
					Vec w = p - v[i];
					Vec b = w - e * std::clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
					d = std::min(d, dot(b, b));
					bool c1 = p.y >= v[i].y;
					bool c2 = p.y < v[j].y;
					bool c3 = e.x * w.y > e.y * w.x;
					if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
					{
						sign = -sign;
					}
				}
				return sign * std::sqrt(d);
			}

			float triangle(Vec p, Vec a, Vec b, Vec c)
			{
				return polygon<3>(p, { a, b, c });
			}

			float star(Vec p)
			{
				std::array<Vec, 10> points;
				for (size_t i = 0; i < points.size(); i++)
				{
					float angle = -1.5707963f + static_cast<float>(i) * 0.6283185f;
					float radius = i % 2 == 0 ? 0.72f : 0.32f;
					points[i] = { radius * std::cos(angle), 0.06f + radius * std::sin(angle) };
				}
				return polygon(p, points);
			}

			// Rounded seven-segment digits, readable at small sizes.
			float digit(int32_t value, Vec p)
			{
				// Segments a-g: top, top right, bottom right, bottom,
				// bottom left, top left, middle.
				static constexpr std::array<uint8_t, 10> SEGMENTS =
				{
					0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
				};
				const Vec tl = { -0.32f, -0.62f };
				const Vec tr = { 0.32f, -0.62f };
				const Vec ml = { -0.32f, 0.0f };
				const Vec mr = { 0.32f, 0.0f };
				const Vec bl = { -0.32f, 0.62f };
				const Vec br = { 0.32f, 0.62f };
				const std::array<std::array<Vec, 2>, 7> lines =
				{{
					{ tl, tr }, { tr, mr }, { mr, br }, { bl, br }, { ml, bl }, { tl, ml }, { ml, mr }
				}};

				float d = 1e9f;
				for (size_t i = 0; i < lines.size(); i++)
				{
					if (SEGMENTS[value] & (1 << i))
					{
						d = std::min(d, capsule(p, lines[i][0], lines[i][1], 0.12f));
					}
				}
				return d;
			}

			float shape(Sprite sprite, Vec p)
			{
				if (sprite >= DIGIT0 && sprite <= DIGIT9)
				{
					return digit(sprite - DIGIT0, p);
				}

				switch (sprite)
				{
				case DISC:
					return circle(p, { 0.0f, 0.0f }, 0.94f);
				case RING:
					return ring(p, { 0.0f, 0.0f }, 0.92f, 0.04f);
				case ARROWS:
					return std::min({
						triangle(p, { 0.0f, -0.86f }, { -0.11f, -0.72f }, { 0.11f, -0.72f }),
						triangle(p, { 0.0f, 0.86f }, { 0.11f, 0.72f }, { -0.11f, 0.72f }),
						triangle(p, { -0.86f, 0.0f }, { -0.72f, 0.11f }, { -0.72f, -0.11f }),
						triangle(p, { 0.86f, 0.0f }, { 0.72f, -0.11f }, { 0.72f, 0.11f })
					});
				case SWORD:
					return std::min({
						// Blade with a pointed tip, then guard, grip and pommel.
						capsule(p, { -0.22f, 0.22f }, { 0.42f, -0.42f }, 0.10f),
						triangle(p, { 0.62f, -0.62f }, { 0.33f, -0.47f }, { 0.47f, -0.33f }),
						capsule(p, { -0.44f, 0.04f }, { -0.04f, 0.44f }, 0.075f),
						capsule(p, { -0.26f, 0.26f }, { -0.50f, 0.50f }, 0.07f),
						circle(p, { -0.56f, 0.56f }, 0.11f)
					});
				case JUMP:
					return std::min(
						polyline(p, { { -0.44f, 0.02f }, { 0.0f, -0.42f }, { 0.44f, 0.02f } }, 0.1f),
						polyline(p, { { -0.44f, 0.44f }, { 0.0f, 0.0f }, { 0.44f, 0.44f } }, 0.1f));
				case PICKUP:
					return std::min({
						capsule(p, { 0.0f, -0.6f }, { 0.0f, 0.12f }, 0.09f),
						polyline(p, { { -0.3f, -0.14f }, { 0.0f, 0.16f }, { 0.3f, -0.14f } }, 0.09f),
						polyline(p, { { -0.56f, 0.12f }, { -0.56f, 0.52f }, { 0.56f, 0.52f }, { 0.56f, 0.12f } }, 0.08f)
					});
				case BAG:
				{
					float body = rounded_box(p, { 0.0f, 0.16f }, { 0.52f, 0.44f }, 0.16f);
					float handle = ring(p, { 0.0f, -0.3f }, 0.24f, 0.07f);
					float flap = capsule(p, { -0.6f, 0.02f }, { 0.6f, 0.02f }, 0.045f);
					float clasp = rounded_box(p, { 0.0f, 0.08f }, { 0.1f, 0.12f }, 0.04f);
					return std::min(std::min(std::max(body, -flap), handle), clasp);
				}
				case SHIRT:
				{
					float body = polygon<12>(p, { {
						{ -0.24f, -0.6f }, { -0.1f, -0.52f }, { 0.1f, -0.52f }, { 0.24f, -0.6f },
						{ 0.7f, -0.3f }, { 0.52f, 0.02f }, { 0.38f, -0.06f }, { 0.38f, 0.64f },
						{ -0.38f, 0.64f }, { -0.38f, -0.06f }, { -0.52f, 0.02f }, { -0.7f, -0.3f }
					} });
					return std::max(body, -circle(p, { 0.0f, -0.62f }, 0.16f));
				}
				case BARS:
					return std::min({
						rounded_box(p, { -0.42f, 0.3f }, { 0.13f, 0.32f }, 0.05f),
						rounded_box(p, { 0.0f, 0.1f }, { 0.13f, 0.52f }, 0.05f),
						rounded_box(p, { 0.42f, -0.06f }, { 0.13f, 0.68f }, 0.05f)
					});
				case STAR:
					return star(p);
				case QUEST:
					return std::min(
						capsule(p, { 0.0f, -0.58f }, { 0.0f, 0.14f }, 0.13f),
						circle(p, { 0.0f, 0.5f }, 0.14f));
				case GLOBE:
				{
					float outline = ring(p, { 0.0f, 0.0f }, 0.62f, 0.065f);
					// Squashing x turns a ring into a meridian ellipse; the
					// distance is only approximate, which is fine at this size.
					float meridian = ring({ p.x * 2.2f, p.y }, { 0.0f, 0.0f }, 0.62f, 0.13f) / 2.2f;
					float equator = capsule(p, { -0.62f, 0.0f }, { 0.62f, 0.0f }, 0.055f);
					return std::min({ outline, std::max(meridian, circle(p, { 0.0f, 0.0f }, 0.62f)), equator });
				}
				case PLUS:
					return std::min(
						capsule(p, { -0.42f, 0.0f }, { 0.42f, 0.0f }, 0.07f),
						capsule(p, { 0.0f, -0.42f }, { 0.0f, 0.42f }, 0.07f));
				case DROP_DOWN:
					return std::min({
						capsule(p, { 0.0f, -0.6f }, { 0.0f, 0.3f }, 0.09f),
						polyline(p, { { -0.3f, 0.06f }, { 0.0f, 0.36f }, { 0.3f, 0.06f } }, 0.09f),
						// The platform, with a gap where the arrow goes through.
						std::max(capsule(p, { -0.62f, -0.14f }, { 0.62f, -0.14f }, 0.07f),
							-rounded_box(p, { 0.0f, -0.14f }, { 0.2f, 0.2f }, 0.02f))
					});
				case JUMP_SLASH:
				{
					// A small sword under a jump chevron.
					constexpr float SCALE = 0.74f;
					Vec sword = (p - Vec{ 0.04f, 0.22f }) * (1.0f / SCALE);
					return std::min(
						polyline(p, { { -0.4f, -0.36f }, { 0.0f, -0.74f }, { 0.4f, -0.36f } }, 0.11f),
						shape(SWORD, sword) * SCALE);
				}
				case GEAR:
				{
					// Eight teeth around a ring.
					float angle = std::atan2(p.y, p.x);
					float teeth = std::cos(angle * 8.0f) > 0.2f ? 0.66f : 0.5f;
					float body = length(p) - teeth;
					return std::max(body, -circle(p, { 0.0f, 0.0f }, 0.22f));
				}
				case MINUS:
					return capsule(p, { -0.42f, 0.0f }, { 0.42f, 0.0f }, 0.08f);
				case FADE:
					return std::min(
						ring(p, { 0.0f, 0.0f }, 0.52f, 0.06f),
						std::max(circle(p, { 0.0f, 0.0f }, 0.52f), -p.x));
				case RESET:
				{
					// Three quarters of a circle with an arrow head.
					float arc = ring(p, { 0.0f, 0.0f }, 0.46f, 0.08f);
					// Cut out the top right quarter, leaving a square end.
					float open = std::max(arc, std::min(p.x, -p.y));
					return std::min(open,
						triangle(p, { 0.0f, -0.72f }, { 0.0f, -0.2f }, { 0.32f, -0.46f }));
				}
				case CHECK:
					return polyline(p, { { -0.46f, 0.02f }, { -0.14f, 0.34f }, { 0.48f, -0.32f } }, 0.1f);
				case CHAT:
				{
					float bubble = std::min(
						rounded_box(p, { 0.0f, -0.1f }, { 0.62f, 0.4f }, 0.2f),
						triangle(p, { -0.36f, 0.2f }, { -0.04f, 0.2f }, { -0.44f, 0.58f }));
					float dots = std::min({
						circle(p, { -0.3f, -0.1f }, 0.09f),
						circle(p, { 0.0f, -0.1f }, 0.09f),
						circle(p, { 0.3f, -0.1f }, 0.09f) });
					return std::max(bubble, -dots);
				}
				case LIST:
				{
					float rows = 1.0f;

					for (float y : { -0.36f, 0.0f, 0.36f })
					{
						rows = std::min({ rows,
							circle(p, { -0.42f, y }, 0.09f),
							capsule(p, { -0.18f, y }, { 0.48f, y }, 0.075f) });
					}

					return rows;
				}
				default:
					return 1.0f;
				}
			}

			// Grey level for a covered pixel. The disc gets a soft vertical
			// gradient and a brighter rim so buttons read as raised.
			float shade(Sprite sprite, Vec p)
			{
				if (sprite != DISC)
				{
					return 1.0f;
				}

				float rim = length(p) > 0.84f ? 1.0f : 0.0f;
				float gradient = 0.9f - 0.2f * (p.y + 1.0f) * 0.5f;
				return std::max(rim, gradient);
			}

			float opacity(Sprite sprite, Vec p)
			{
				if (sprite != DISC)
				{
					return 1.0f;
				}

				return length(p) > 0.84f ? 1.0f : 0.78f;
			}
		}

		Cell cell(Sprite sprite)
		{
			int32_t column = sprite % COLUMNS;
			int32_t row = sprite / COLUMNS;
			int32_t left = BORDER + column * CELL;
			int32_t top = BORDER + row * CELL;

			return {
				static_cast<int16_t>(left), static_cast<int16_t>(top),
				static_cast<int16_t>(left + CELL), static_cast<int16_t>(top + CELL)
			};
		}

		// Copies a CELL x CELL PNG from the APK assets into the sheet.
		void load_icon(std::vector<uint8_t>& pixels, Cell area, const std::string& path)
		{
			SDL_RWops* file = SDL_RWFromFile(path.c_str(), "rb");

			if (!file)
				return;

			std::vector<uint8_t> bytes(static_cast<size_t>(std::max<Sint64>(SDL_RWsize(file), 0)));
			bool read = !bytes.empty() && SDL_RWread(file, bytes.data(), 1, bytes.size()) == bytes.size();
			SDL_RWclose(file);

			if (!read)
				return;

			int width = 0;
			int height = 0;
			int channels = 0;
			stbi_uc* data = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);

			if (!data)
				return;

			if (width == CELL && height == CELL)
			{
				for (int32_t y = 0; y < CELL; y++)
				{
					size_t row = (static_cast<size_t>(area.top + y) * WIDTH + area.left) * 4;
					std::copy_n(data + static_cast<size_t>(y) * CELL * 4, CELL * 4, pixels.begin() + row);
				}
			}

			stbi_image_free(data);
		}

		std::vector<uint8_t> render()
		{
			std::vector<uint8_t> pixels(static_cast<size_t>(WIDTH) * HEIGHT * 4, 0);
			constexpr float HALF = CELL / 2.0f;

			for (uint8_t s = 0; s < NUM_SPRITES; s++)
			{
				Sprite sprite = static_cast<Sprite>(s);
				Cell area = cell(sprite);

				for (int32_t y = 0; y < CELL; y++)
				{
					for (int32_t x = 0; x < CELL; x++)
					{
						Vec p = { (x + 0.5f) / HALF - 1.0f, (y + 0.5f) / HALF - 1.0f };
						// One pixel of soft edge.
						float coverage = std::clamp(0.5f - shape(sprite, p) * HALF, 0.0f, 1.0f);
						if (coverage <= 0.0f)
						{
							continue;
						}

						auto grey = static_cast<uint8_t>(std::lround(255.0f * shade(sprite, p)));
						auto alpha = static_cast<uint8_t>(std::lround(255.0f * coverage * opacity(sprite, p)));
						size_t index = (static_cast<size_t>(area.top + y) * WIDTH + area.left + x) * 4;
						pixels[index + 0] = grey;
						pixels[index + 1] = grey;
						pixels[index + 2] = grey;
						pixels[index + 3] = alpha;
					}
				}
			}

			// Icons drawn by hand, bundled as 256x256 white PNGs, replace
			// the generated ones where present.
			const std::pair<Sprite, const char*> art[] = {
				{ SWORD, "attack" }, { JUMP, "jump" }, { DROP_DOWN, "jump_down" },
				{ JUMP_SLASH, "jump_attack" }, { PICKUP, "pickup" }, { BAG, "inventory" },
				{ SHIRT, "equip" }, { BARS, "stats" }, { STAR, "skills" },
				{ QUEST, "quests" }, { GLOBE, "map" }, { GEAR, "settings" },
				{ CHAT, "chat" }, { LIST, "solo" }
			};

			for (const auto& [sprite, name] : art)
				load_icon(pixels, cell(sprite), std::string("touch/icon_") + name + ".png");

			return pixels;
		}
	}
}
