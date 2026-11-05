//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#include "DirectionScene.h"

#include "../../Audio/Audio.h"
#include "../../Constants.h"
#include "../../Net/Packets/GameplayPackets.h"

#include <algorithm>

#ifdef USE_NX
#include <nlnx/nx.hpp>
#endif

namespace ms
{
	namespace
	{
		// "Effect/Direction1.img/aranTutorial/Scene0" -> node in Effect.nx
		nl::node effect_node(const std::string& path)
		{
			const std::string prefix = "Effect/";

			if (path.compare(0, prefix.size(), prefix) == 0)
				return nl::nx::effect.resolve(path.substr(prefix.size()));

			return nl::nx::effect.resolve(path);
		}
	}

	void DirectionScene::play(const std::string& path)
	{
		clear();

		nl::node scene = effect_node(path);

		if (!scene)
			return;

		for (nl::node step : scene)
		{
			int32_t type = step["type"].get_integer();
			int32_t start = step["start"].get_integer();

			switch (type)
			{
			case 0:
			{
				nl::node visual = effect_node(step["visual"].get_string());

				if (!visual)
					break;

				Picture picture;
				picture.animation = Animation(visual);
				picture.x = static_cast<int16_t>(step["x"].get_integer());
				picture.y = static_cast<int16_t>(step["y"].get_integer());
				picture.z = step["z"].get_integer();
				picture.start = start;
				picture.done = false;
				pictures.push_back(std::move(picture));
				break;
			}
			case 2:
			{
				int32_t field = step["field"].get_integer();

				if (field > 0)
					cues.push_back({ Cue::Kind::FIELD, start, "", field, false });

				break;
			}
			case 5:
			{
				std::string sound = step["sound"].get_string();

				// Music lives in Sound.wz ("Sound/Bgm19.img/CrystalCave"),
				// short effects in Effect.wz ("Effect/Direction1.img/sound/...")
				if (sound.compare(0, 6, "Sound/") == 0)
					cues.push_back({ Cue::Kind::MUSIC, start, sound.substr(6), 0, false });
				else if (!sound.empty())
					cues.push_back({ Cue::Kind::SOUND, start, sound, 0, false });

				break;
			}
			default:
				break;
			}
		}

		std::stable_sort(pictures.begin(), pictures.end(),
			[](const Picture& a, const Picture& b) { return a.z < b.z; });
	}

	void DirectionScene::fire(Cue& cue) const
	{
		cue.fired = true;

		switch (cue.kind)
		{
		case Cue::Kind::FIELD:
			// What the game client does at the end of a scene: ask for the next
			// map. The server only allows this from the cutscene maps.
			ChangeMapPacket(false, cue.field, "", false).dispatch();
			break;
		case Cue::Kind::MUSIC:
			Music(cue.path).play();
			break;
		case Cue::Kind::SOUND:
			if (nl::node sound = effect_node(cue.path))
				Sound(sound).play();
			break;
		}
	}

	void DirectionScene::update()
	{
		if (!active())
			return;

		elapsed += Constants::TIMESTEP;

		for (Picture& picture : pictures)
		{
			if (picture.done || elapsed < picture.start)
				continue;

			if (picture.animation.update())
				picture.done = true;
		}

		for (Cue& cue : cues)
		{
			if (!cue.fired && elapsed >= cue.start)
				fire(cue);
		}
	}

	void DirectionScene::draw(float alpha) const
	{
		if (pictures.empty())
			return;

		// Scene coordinates are relative to the middle of the screen
		int16_t cx = Constants::Constants::get().get_viewwidth() / 2;
		int16_t cy = Constants::Constants::get().get_viewheight() / 2;

		for (const Picture& picture : pictures)
		{
			if (picture.done || elapsed < picture.start)
				continue;

			picture.animation.draw(DrawArgument(Point<int16_t>(cx + picture.x, cy + picture.y)), alpha);
		}
	}

	void DirectionScene::clear()
	{
		pictures.clear();
		cues.clear();
		elapsed = 0;
	}

	bool DirectionScene::active() const
	{
		if (std::any_of(cues.begin(), cues.end(), [](const Cue& c) { return !c.fired; }))
			return true;

		return std::any_of(pictures.begin(), pictures.end(), [](const Picture& p) { return !p.done; });
	}
}
