/*
 * Heart of The Alien: SFX handling
 * Copyright (c) 2004-2005 Gil Megidish
 * Copyright (c) 2016-2026 carstene1ns
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "audio.h"
#include "common.h"
#include "vm.h"
#include "client.h"
#include "files.h"

static MIX_Mixer *mixer;
static MIX_Track *music;
static MIX_Track *sounds[4];
static int mixer_initialized = 0;

/** Stops all 4 channels
*/
static void stop_all_channels()
{
	for(int i = 0; i < 4; i++)
	{
		if (sounds[i] != NULL)
		{
			MIX_StopTrack(sounds[i], 0);
		}
	}
}

/** Start playing a sample
    @param index     sample identifier
    @param volume    volume [0 .. 0xff]
    @param channel   mixing channel [0 .. 3]

    Samples on sega-cd are 8000 hz, 8bit signed. each sample is
    converted to native format before being mixed.
*/
void play_sample(int index, int volume, int channel)
{
	static SDL_AudioSpec spec = {
		.format = SDL_AUDIO_S8,
		.channels = 1,
		.freq = 8000
	};

	if (cls.nosound)
	{
		/* day off! */
		return;
	}

	if (index == 0)
	{
		/* stop all sounds */
		stop_all_channels();
		return;
	}

	if (channel < 0 || channel >= 4)
	{
		LOG(("play_sample: invalid channel %d\n", channel));
		return;
	}

	if (mixer == NULL || sounds[channel] == NULL)
	{
		return;
	}

	index = index - 1;
	LOG(("playing sample %d, volume %d, channel %d\n", index, volume, channel));

	int sample_ptr = get_long(0xf90c);
	int ptr = get_long(sample_ptr + index * 4);
	int length = get_long(ptr);

	if (length <= 0)
	{
		LOG(("play_sample: invalid sample length %d\n", length));
		return;
	}

	if (volume < 0) volume = 0;
	if (volume > 255) volume = 255;

	/* length plus some unknown flags */
	ptr = ptr + 8;
	LOG(("sample data starts at 0x%x, length is %d\n", ptr, length));

	/* convert from 8000 mono 8bit, to 44100 stereo 16bit */
	char *buffer = SDL_malloc(length);
	if (buffer == NULL)
	{
		fprintf(stderr, "failed to allocate %d bytes for sample\n", length);
		return;
	}

	char *current_sample = buffer;
	for (int p=0; p<length; p++)
	{
		unsigned char u;
		signed char s;

		u = get_byte(ptr++);
		if (u > 0x80)
		{
			s = 0 - (u & 0x7f);
		}
		else
		{
			s = (signed char)u;
		}

		*current_sample++ = s;
	}

	MIX_Audio *a = MIX_LoadRawAudioNoCopy(mixer, buffer, length, &spec, true);
	if (a == NULL)
	{
		LOG(("play_sample: failed to load sample %d: %s\n", index, SDL_GetError()));
		SDL_free(buffer);
		return;
	}

	MIX_SetTrackAudio(sounds[channel], a);
	MIX_SetTrackGain(sounds[channel], volume/255.0f);
	MIX_PlayTrack(sounds[channel], 0);
	MIX_DestroyAudio(a);
}

/** Initialize audio module
    @returns zero on success, negative value on error
*/
int audio_init()
{
	if (!MIX_Init())
	{
		fprintf(stderr, "failed to initialize SDL_mixer: %s\n", SDL_GetError());
		return -1;
	}

	mixer_initialized = 1;
	mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
	if (mixer == NULL)
	{
		fprintf(stderr, "failed to create audio mixer: %s\n", SDL_GetError());
		MIX_Quit();
		mixer_initialized = 0;
		return -1;
	}

	music = MIX_CreateTrack(mixer);
	for(int i = 0; i < 4; i++)
	{
		sounds[i] = MIX_CreateTrack(mixer);
	}

	if (music == NULL)
	{
		fprintf(stderr, "failed to create music track: %s\n", SDL_GetError());
		return -1;
	}

	for (int i = 0; i < 4; i++)
	{
		if (sounds[i] == NULL)
		{
			fprintf(stderr, "failed to create sound track %d: %s\n", i, SDL_GetError());
			return -1;
		}
	}

	return 0;
}

/** Purges all cached sound effects from memory

    Releases memory taken by sfx previously played by play_sample()
*/
void sound_flush_cache()
{
}

/** callback when game script has been unloaded */
void audio_done()
{
	if (mixer_initialized == 0)
	{
		return;
	}

	sound_flush_cache();

	for(int i = 0; i < 4; i++)
	{
		if (sounds[i] != NULL)
		{
			MIX_DestroyTrack(sounds[i]);
			sounds[i] = NULL;
		}
	}

	if (music != NULL)
	{
		MIX_StopTrack(music, 0);
		MIX_DestroyTrack(music);
		music = NULL;
	}

	if (mixer != NULL)
	{
		MIX_DestroyMixer(mixer);
		mixer = NULL;
	}

	MIX_Quit();
	mixer_initialized = 0;
}

/** Stops music */
void stop_music()
{
	if (cls.nosound == 0 && music != NULL)
	{
		MIX_StopTrack(music, 100);
	}
}

/** Builds a path to a music file, falling back to the directory layout used
    by the distributed game data (prefix/prefix NN.ext).
    @returns zero if a readable file was found
*/
static int find_music_file(char *filename, size_t filename_size,
	const char *prefix, int track, const char *extension)
{
	int len;

	len = snprintf(filename, filename_size, "%s %02d.%s",
		prefix, track + 1, extension);
	if (len < 0 || (size_t)len >= filename_size)
	{
		return -1;
	}

	if (access(filename, R_OK) == 0)
	{
		return 0;
	}

	if (strchr(prefix, '/') == NULL && strchr(prefix, '\\') == NULL)
	{
		len = snprintf(filename, filename_size, "%s/%s %02d.%s",
			prefix, prefix, track + 1, extension);
		if (len > 0 && (size_t)len < filename_size &&
			access(filename, R_OK) == 0)
		{
			return 0;
		}
	}

	return -1;
}

/** Plays audio track
    @param track   track to play
    @param loop    non-zero to loop forever, zero to play once
*/
void play_music_track(int track, int loop)
{
	if (cls.nosound == 0)
	{
		char filename[512];

		if (mixer == NULL || music == NULL)
		{
			return;
		}

		stop_music();

		const char *ptr;
		if(cls.music_prefix != NULL)
		{
			ptr = cls.music_prefix;
		}
		else if(cls.iso_prefix != NULL)
		{
			ptr = cls.iso_prefix;
		}
		else
		{
			ptr = DEFAULT_ISO_PREFIX;
		}

		#define NUM_EXT 5
		static const char* extensions[NUM_EXT] = { "wav", "mp3", "ogg", "opus", "flac" };
		int found = 0;

		/* search for valid music files */
		for (int i = 0; i < NUM_EXT; i++)
		{
			if (find_music_file(filename, sizeof(filename), ptr, track, extensions[i]) == 0)
			{
				found = 1;
				break;
			}
		}
		#undef NUM_EXT

		if (!found)
		{
			LOG(("music %d not found!\n", track + 1));
			return;
		}

		LOG(("playing music %s\n", filename));

		MIX_Audio *a = MIX_LoadAudio(mixer, filename, false);
		if (a == NULL)
		{
			fprintf(stderr, "failed to load music %s: %s\n", filename, SDL_GetError());
			return;
		}

		MIX_SetTrackAudio(music, a);
		MIX_SetTrackLoops(music, loop ? -1 : 0);
		MIX_PlayTrack(music, 0);
		MIX_DestroyAudio(a);
	}
}

/** Callback after a frame has been rendered */
void music_update()
{
}
