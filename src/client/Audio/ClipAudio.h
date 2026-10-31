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

// Clip-based audio backend: each sound is registered once under an id and then
// played by that id. The WASM build implements this in JavaScript inside
// Audio.cpp; SDL builds implement it natively in ClipAudioSDL.cpp.
namespace jrc
{
    // Returns nonzero on success.
    int clip_audio_init();
    void clip_audio_close();
    // Volumes range from 0.0 to 1.0. Return nonzero on success.
    int clip_audio_set_sfx_volume(double volume);
    int clip_audio_set_bgm_volume(double volume);
    // Registers encoded audio (MP3 or PCM WAV). The data must stay valid for
    // the program's lifetime, which holds for memory-mapped NX files.
    int clip_audio_register_clip(int id, const void* data, int length);
    void clip_audio_play_sound(int id);
    // Loops the clip as background music, replacing the current track.
    void clip_audio_play_music(int id);
}
