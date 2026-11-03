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
#include "ClipAudio.h"
// Native mixer behind the clip audio API for SDL builds (desktop and Android).
#ifdef MS_PLATFORM_SDL

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#include "minimp3/minimp3.h"
#include "minimp3/minimp3_ex.h"

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace jrc
{
    namespace
    {
        constexpr int OUTPUT_RATE = 44100;
        constexpr int OUTPUT_CHANNELS = 2;
        constexpr size_t MAX_VOICES = 16;

        // The game's audio comes from .nx files that are read in pieces and
        // cached, so a pointer into them only stays valid for a moment.
        // Clips keep their own copy of the encoded bytes.
        struct Clip
        {
            std::vector<uint8_t> bytes;
            bool decoded = false;
            // Interleaved stereo at OUTPUT_RATE. Empty if decoding failed.
            std::vector<int16_t> pcm;
        };

        struct Voice
        {
            const Clip* clip;
            size_t position;
        };

        // Streams one MP3 from memory, decoding inside the audio callback.
        struct MusicTrack
        {
            // The encoded MP3, which the decoder reads while playing.
            std::vector<uint8_t> source;
            mp3dec_ex_t decoder;
            SDL_AudioStream* stream = nullptr;
            // Set when the track is WAV: it is decoded up front and looped.
            const Clip* clip = nullptr;
            size_t position = 0;

            ~MusicTrack()
            {
                if (stream)
                {
                    SDL_FreeAudioStream(stream);
                    mp3dec_ex_close(&decoder);
                }
            }
        };

        SDL_AudioDeviceID device = 0;
        std::unordered_map<int, Clip> clips;
        std::vector<Voice> voices;
        std::unique_ptr<MusicTrack> music;
        float sfx_volume = 1.0f;
        float bgm_volume = 1.0f;

        uint16_t read_u16(const uint8_t* bytes)
        {
            return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
        }

        uint32_t read_u32(const uint8_t* bytes)
        {
            return static_cast<uint32_t>(bytes[0]) |
                (static_cast<uint32_t>(bytes[1]) << 8) |
                (static_cast<uint32_t>(bytes[2]) << 16) |
                (static_cast<uint32_t>(bytes[3]) << 24);
        }

        bool is_wav(const uint8_t* data, size_t length)
        {
            return length >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0;
        }

        // Converts samples into the output format with an SDL audio stream.
        std::vector<int16_t> convert(SDL_AudioFormat format, int channels, int rate, const void* samples, size_t bytes)
        {
            std::vector<int16_t> result;
            SDL_AudioStream* stream = SDL_NewAudioStream(format, channels, rate, AUDIO_S16SYS, OUTPUT_CHANNELS, OUTPUT_RATE);

            if (!stream)
            {
                return result;
            }

            if (SDL_AudioStreamPut(stream, samples, static_cast<int>(bytes)) == 0 && SDL_AudioStreamFlush(stream) == 0)
            {
                result.resize(SDL_AudioStreamAvailable(stream) / sizeof(int16_t));
                int got = SDL_AudioStreamGet(stream, result.data(), static_cast<int>(result.size() * sizeof(int16_t)));
                result.resize(got > 0 ? got / sizeof(int16_t) : 0);
            }

            SDL_FreeAudioStream(stream);
            return result;
        }

        // Decodes a PCM WAV file with 8 or 16 bit samples.
        std::vector<int16_t> decode_wav(const uint8_t* data, size_t length)
        {
            int channels = 0;
            int rate = 0;
            int bits = 0;
            size_t offset = 12;

            while (offset + 8 <= length)
            {
                const uint8_t* chunk = data + offset;
                size_t size = read_u32(chunk + 4);
                const uint8_t* body = chunk + 8;
                size_t available = std::min(size, length - offset - 8);

                if (std::memcmp(chunk, "fmt ", 4) == 0 && available >= 16)
                {
                    if (read_u16(body) != 1)
                    {
                        return {};
                    }

                    channels = read_u16(body + 2);
                    rate = static_cast<int>(read_u32(body + 4));
                    bits = read_u16(body + 14);
                }
                else if (std::memcmp(chunk, "data", 4) == 0 && channels > 0 && rate > 0)
                {
                    SDL_AudioFormat format = bits == 8 ? AUDIO_U8 : AUDIO_S16LSB;

                    if (bits != 8 && bits != 16)
                    {
                        return {};
                    }

                    return convert(format, channels, rate, body, available);
                }

                offset += 8 + size + (size & 1);
            }

            return {};
        }

        std::vector<int16_t> decode_mp3(const uint8_t* data, size_t length)
        {
            mp3dec_t decoder;
            mp3dec_file_info_t info;
            std::memset(&info, 0, sizeof(info));

            std::vector<int16_t> result;

            if (mp3dec_load_buf(&decoder, data, length, &info, nullptr, nullptr) == 0 && info.samples > 0)
            {
                result = convert(AUDIO_S16SYS, info.channels, info.hz, info.buffer, info.samples * sizeof(mp3d_sample_t));
            }

            std::free(info.buffer);
            return result;
        }

        // Clips are decoded on first use, so loading a map stays fast.
        const Clip* decoded_clip(int id)
        {
            auto iter = clips.find(id);

            if (iter == clips.end())
            {
                return nullptr;
            }

            Clip& clip = iter->second;

            if (!clip.decoded)
            {
                clip.pcm = is_wav(clip.bytes.data(), clip.bytes.size()) ?
                    decode_wav(clip.bytes.data(), clip.bytes.size()) :
                    decode_mp3(clip.bytes.data(), clip.bytes.size());
                clip.decoded = true;
                // Only the decoded samples are needed from now on.
                clip.bytes = {};
            }

            return clip.pcm.empty() ? nullptr : &clip;
        }

        // Fills 'out' with up to 'count' music samples. Runs on the audio thread.
        size_t read_music(int16_t* out, size_t count)
        {
            MusicTrack& track = *music;

            if (track.clip)
            {
                const std::vector<int16_t>& pcm = track.clip->pcm;
                size_t done = 0;

                while (done < count)
                {
                    size_t chunk = std::min(count - done, pcm.size() - track.position);
                    std::memcpy(out + done, pcm.data() + track.position, chunk * sizeof(int16_t));
                    done += chunk;
                    track.position = (track.position + chunk) % pcm.size();
                }

                return done;
            }

            int wanted = static_cast<int>(count * sizeof(int16_t));
            bool rewound = false;

            while (SDL_AudioStreamAvailable(track.stream) < wanted)
            {
                mp3d_sample_t decoded[MINIMP3_MAX_SAMPLES_PER_FRAME];
                size_t samples = mp3dec_ex_read(&track.decoder, decoded, MINIMP3_MAX_SAMPLES_PER_FRAME);

                if (samples == 0)
                {
                    // End of the track: loop, unless it produced nothing at all.
                    if (rewound || mp3dec_ex_seek(&track.decoder, 0) != 0)
                    {
                        break;
                    }

                    rewound = true;
                    continue;
                }

                rewound = false;
                SDL_AudioStreamPut(track.stream, decoded, static_cast<int>(samples * sizeof(mp3d_sample_t)));
            }

            int got = SDL_AudioStreamGet(track.stream, out, wanted);
            return got > 0 ? static_cast<size_t>(got) / sizeof(int16_t) : 0;
        }

        void mix(void*, Uint8* stream, int length)
        {
            size_t count = static_cast<size_t>(length) / sizeof(int16_t);
            std::vector<int32_t> mixed(count, 0);

            for (auto voice = voices.begin(); voice != voices.end();)
            {
                const std::vector<int16_t>& pcm = voice->clip->pcm;
                size_t chunk = std::min(count, pcm.size() - voice->position);

                for (size_t i = 0; i < chunk; ++i)
                {
                    mixed[i] += static_cast<int32_t>(pcm[voice->position + i] * sfx_volume);
                }

                voice->position += chunk;
                voice = voice->position >= pcm.size() ? voices.erase(voice) : voice + 1;
            }

            if (music)
            {
                std::vector<int16_t> samples(count, 0);
                size_t got = read_music(samples.data(), count);

                for (size_t i = 0; i < got; ++i)
                {
                    mixed[i] += static_cast<int32_t>(samples[i] * bgm_volume);
                }
            }

            int16_t* out = reinterpret_cast<int16_t*>(stream);

            for (size_t i = 0; i < count; ++i)
            {
                out[i] = static_cast<int16_t>(std::clamp<int32_t>(mixed[i], INT16_MIN, INT16_MAX));
            }
        }

        // Holds the audio device lock for state shared with the mixer.
        struct MixerLock
        {
            MixerLock() { SDL_LockAudioDevice(device); }
            ~MixerLock() { SDL_UnlockAudioDevice(device); }
        };
    }

    int clip_audio_init()
    {
        if (device)
        {
            return 1;
        }

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            return 0;
        }

        SDL_AudioSpec wanted;
        SDL_zero(wanted);
        wanted.freq = OUTPUT_RATE;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = OUTPUT_CHANNELS;
        wanted.samples = 1024;
        wanted.callback = mix;

        // No allowed changes: SDL converts to whatever the hardware wants.
        device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);

        if (!device)
        {
            return 0;
        }

        SDL_PauseAudioDevice(device, 0);
        return 1;
    }

    void clip_audio_close()
    {
        if (!device)
        {
            return;
        }

        SDL_CloseAudioDevice(device);
        device = 0;
        voices.clear();
        music.reset();
        clips.clear();
    }

    int clip_audio_set_sfx_volume(double volume)
    {
        if (!device)
        {
            return 0;
        }

        MixerLock lock;
        sfx_volume = static_cast<float>(std::clamp(volume, 0.0, 1.0));
        return 1;
    }

    int clip_audio_set_bgm_volume(double volume)
    {
        if (!device)
        {
            return 0;
        }

        MixerLock lock;
        bgm_volume = static_cast<float>(std::clamp(volume, 0.0, 1.0));
        return 1;
    }

    int clip_audio_register_clip(int id, const void* data, int length)
    {
        if (!device || !data || length <= 0)
        {
            return 0;
        }

        // The mixer only reads decoded clips through voices, never the map.
        if (!clips.count(id))
        {
            Clip clip;
            auto bytes = static_cast<const uint8_t*>(data);
            clip.bytes.assign(bytes, bytes + length);
            clips.emplace(id, std::move(clip));
        }

        return 1;
    }

    void clip_audio_play_sound(int id)
    {
        if (!device)
        {
            return;
        }

        const Clip* clip = decoded_clip(id);

        if (!clip)
        {
            return;
        }

        MixerLock lock;

        if (voices.size() >= MAX_VOICES)
        {
            voices.erase(voices.begin());
        }

        voices.push_back({ clip, 0 });
    }

    void clip_audio_play_music(int id)
    {
        if (!device)
        {
            return;
        }

        auto iter = clips.find(id);

        if (iter == clips.end())
        {
            return;
        }

        auto track = std::make_unique<MusicTrack>();
        Clip& source = iter->second;

        if (source.decoded || is_wav(source.bytes.data(), source.bytes.size()))
        {
            track->clip = decoded_clip(id);

            if (!track->clip)
            {
                return;
            }
        }
        else
        {
            // The track takes the bytes, so a long song is not kept twice and
            // songs from maps left behind do not pile up.
            track->source = std::move(source.bytes);
            clips.erase(iter);

            if (mp3dec_ex_open_buf(&track->decoder, track->source.data(), track->source.size(), MP3D_SEEK_TO_BYTE | MP3D_DO_NOT_SCAN) != 0)
            {
                return;
            }

            track->stream = SDL_NewAudioStream(
                AUDIO_S16SYS, std::max(track->decoder.info.channels, 1), std::max(track->decoder.info.hz, 1),
                AUDIO_S16SYS, OUTPUT_CHANNELS, OUTPUT_RATE
            );

            if (!track->stream)
            {
                mp3dec_ex_close(&track->decoder);
                return;
            }
        }

        std::unique_ptr<MusicTrack> previous;

        {
            MixerLock lock;
            previous = std::move(music);
            music = std::move(track);
        }
    }
}
#endif
