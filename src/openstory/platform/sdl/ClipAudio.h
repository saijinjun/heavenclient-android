//////////////////////////////////////////////////////////////////////////////////
//	This program is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Affero General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//////////////////////////////////////////////////////////////////////////////////
#pragma once

// Clip-based audio for the SDL port: each sound is registered once under an
// id and then played by that id. Replaces BASS (src/Audio/Audio.cpp).
namespace ms
{
	// Returns nonzero on success.
	int clip_audio_init();
	void clip_audio_close();
	// Volumes range from 0.0 to 1.0. Return nonzero on success.
	int clip_audio_set_sfx_volume(double volume);
	int clip_audio_set_bgm_volume(double volume);
	// Registers encoded audio (MP3 or PCM WAV). The bytes are copied.
	int clip_audio_register_clip(int id, const void* data, int length);
	void clip_audio_play_sound(int id);
	// volume 0..1, pan -1 (left) .. 1 (right).
	void clip_audio_play_sound(int id, float volume, float pan);
	// Plays the clip as background music, replacing the current track.
	void clip_audio_play_music(int id);
	void clip_audio_play_music(int id, bool loop);
}
