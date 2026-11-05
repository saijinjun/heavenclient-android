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
#include "OtherChar.h"

#include "../Constants.h"

#include <algorithm>
#include <cmath>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// Above this much queued movement the playback speeds up so a character
		// that fell behind (bunched packets, a hitch) catches up instead of
		// lagging further. A real client sends one packet about every 500 ms
		// holding about 500 ms of movement, so normal play stays below it.
		constexpr double CATCHUP_AFTER_MS = 700.0;
		constexpr double MAX_CATCHUP_RATE = 4.0;
		// So far behind that playing it back is pointless: jump to the end.
		constexpr double SNAP_BACKLOG_MS = 2500.0;
		// A new packet that starts this far from where the last one ended is a
		// resync (lag recovery, a missed packet), not a walk: snap to it.
		constexpr double RESYNC_DISTANCE = 250.0;
	}

	OtherChar::OtherChar(int32_t id, const CharLook& lk, uint8_t lvl, int16_t jb, const std::string& nm, int8_t st, Point<int16_t> pos) : Char(id, lk, nm)
	{
		level = lvl;
		job = jb;
		set_position(pos);

		lastmove.xpos = pos.x();
		lastmove.ypos = pos.y();
		lastmove.newstate = st;

		seg_x = tail_x = pos.x();
		seg_y = tail_y = pos.y();
		seg_elapsed = 0.0;

		attackspeed = 6;
		attacking = false;

		apply_nametag_style(jb == 900 || jb == 910);
		refresh_ring_effect();
	}

	int8_t OtherChar::update(const Physics& physics)
	{
		// Play the received movement path back step by step, each step over
		// its own duration, the way the original client does. Every packet
		// carries several steps (walk, jump, fall, land...); using only the
		// last one made characters dash to the end of each packet and then
		// walk on the spot until the next one, and lost every jump arc.
		double queued = backlog();

		if (queued > SNAP_BACKLOG_MS)
		{
			snap_to(tail_x, tail_y);
			queued = 0.0;
		}

		double rate = 1.0;

		if (queued > CATCHUP_AFTER_MS)
			rate = std::min(MAX_CATCHUP_RATE, queued / CATCHUP_AFTER_MS);

		double budget = Constants::TIMESTEP * rate;
		double cur_x = phobj.crnt_x();
		double cur_y = phobj.crnt_y();
		double next_x = cur_x;
		double next_y = cur_y;

		while (!path.empty() && budget > 0.0)
		{
			const Waypoint& step = path.front();
			lastmove.newstate = step.state;

			double left = step.duration - seg_elapsed;

			if (left <= budget)
			{
				budget -= std::max(left, 0.0);

				if (step.haspos)
				{
					next_x = step.x;
					next_y = step.y;
				}

				seg_x = next_x;
				seg_y = next_y;
				seg_elapsed = 0.0;
				path.pop_front();
			}
			else
			{
				seg_elapsed += budget;
				budget = 0.0;

				if (step.haspos)
				{
					double t = seg_elapsed / step.duration;
					next_x = seg_x + (step.x - seg_x) * t;
					next_y = seg_y + (step.y - seg_y) * t;
				}
			}
		}

		if (!attacking)
			set_state(lastmove.newstate);

		phobj.hspeed = next_x - cur_x;
		phobj.vspeed = next_y - cur_y;
		phobj.move();

		// Movement comes from packets, so nothing else tells them which layer
		// of the map they're on. Without this they all draw on the lowest one,
		// behind the tiles and objects of every layer above it.
		physics.get_fht().update_layer(phobj);

		bool aniend = Char::update(physics, get_stancespeed());

		if (aniend && attacking)
			attacking = false;

		return get_layer();
	}

	void OtherChar::snap_to(double x, double y)
	{
		path.clear();
		phobj.set_x(x);
		phobj.set_y(y);
		phobj.hspeed = 0.0;
		phobj.vspeed = 0.0;
		seg_x = tail_x = x;
		seg_y = tail_y = y;
		seg_elapsed = 0.0;
	}

	double OtherChar::backlog() const
	{
		double total = -seg_elapsed;

		for (const Waypoint& step : path)
			total += step.duration;

		return std::max(total, 0.0);
	}

	void OtherChar::send_movement(const std::vector<Movement>& newmoves)
	{
		if (newmoves.empty())
			return;

		if (path.empty())
		{
			// Nothing queued: the next step starts where the character stands.
			seg_x = tail_x = phobj.crnt_x();
			seg_y = tail_y = phobj.crnt_y();
			seg_elapsed = 0.0;
		}

		// A packet that starts far from where the last one ended means we are
		// out of sync (lag recovery, a dropped packet): snap rather than slide
		// the character across the map, otherwise a foreign player lingers
		// where they used to be and appears to attack empty space.
		for (const Movement& m : newmoves)
		{
			if (m.type == Movement::NONE || m.type == Movement::RELATIVE)
				continue;

			double dx = m.xpos - tail_x;
			double dy = m.ypos - tail_y;

			if (m.type != Movement::TELEPORT && (dx * dx + dy * dy) > RESYNC_DISTANCE * RESYNC_DISTANCE)
				snap_to(m.xpos, m.ypos);

			break;
		}

		bool warped = false;

		for (const Movement& m : newmoves)
		{
			switch (m.type)
			{
			case Movement::NONE:
				// Equip change and similar: no position, no time.
				break;
			case Movement::TELEPORT:
				// A teleport is stated outright by the movement command, so it
				// does not have to be inferred from distance.
				snap_to(m.xpos, m.ypos);
				lastmove.newstate = m.newstate;
				warped = true;
				break;
			case Movement::RELATIVE:
				// Jump/knockback start: the two shorts are a velocity, not a
				// position. Show the stance; the following absolute steps
				// carry the actual arc.
				path.push_back({ tail_x, tail_y, false, m.newstate, static_cast<double>(std::max<int16_t>(m.duration, 0)) });
				break;
			default:
				path.push_back({ static_cast<double>(m.xpos), static_cast<double>(m.ypos), true, m.newstate, static_cast<double>(std::max<int16_t>(m.duration, 0)) });
				tail_x = m.xpos;
				tail_y = m.ypos;
				break;
			}
		}

		// The teleport skills carry no authored use-effect, so the standard
		// BasicEff puff is substituted — same as the local player's own warp.
		if (warped)
		{
			static Animation tp_effect(nl::nx::effect["BasicEff.img"]["Teleport"]);
			show_attack_effect(tp_effect, 0);
		}

		lastmove.xpos = static_cast<int16_t>(tail_x);
		lastmove.ypos = static_cast<int16_t>(tail_y);
	}

	void OtherChar::update_skill(int32_t skillid, uint8_t skilllevel)
	{
		skilllevels[skillid] = skilllevel;
	}

	void OtherChar::update_speed(uint8_t as)
	{
		attackspeed = as;
	}

	void OtherChar::update_look(const LookEntry& newlook)
	{
		look = newlook;

		uint8_t laststate = lastmove.newstate;
		set_state(laststate);

		// A newly-equipped effect ring should start (or stop) its aura.
		refresh_ring_effect();
	}

	int8_t OtherChar::get_integer_attackspeed() const
	{
		return attackspeed;
	}

	uint16_t OtherChar::get_level() const
	{
		return level;
	}

	int16_t OtherChar::get_job() const
	{
		return job;
	}

	int32_t OtherChar::get_skilllevel(int32_t skillid) const
	{
		auto iter = skilllevels.find(skillid);

		if (iter == skilllevels.end())
			return 0;

		return iter->second;
	}
}