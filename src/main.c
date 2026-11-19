/*
 * Heart of The Alien: Game loop and main
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <limits.h>
#include <assert.h>
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "parg.h"

#include "client.h"
#include "vm.h"
#include "rooms.h"
#include "debug.h"
#include "audio.h"
#include "common.h"
#include "files.h"
#include "decode.h"
#include "render.h"
#include "screen.h"
#include "sprites.h"
#include "game2bin.h"
#include "animation.h"
#include "keymap.h"
#include "platform.h"

static char *QUICKSAVE_FILENAME = "quicksave";
static char *RECORDED_KEYS_FILENAME = "recorded-keys";

typedef struct anm_file_s
{
	int track;
	const char *filename;
	int offset;
} anm_file_t;

static anm_file_t anm_files[] = 
{
	{31, "INTRO1.BIN", 0},
	{32, "INTRO2.BIN", 0},
	{33, "INTRO3.BIN", 0},
	{34, "INTRO4.BIN", 0},
	{35, "MAKE2MB.BIN", 0x109a},
	{36, "MID2.BIN", 0},
	{37, "END1.BIN", 0},
	{38, "END2.BIN", 0},
	{39, "END3.BIN", 0},
	{40, "END4.BIN", 0}
};

///////
extern int script_ptr;

int next_script;
int current_backdrop;
int current_room;

int speed_throttle = 0;

int debug_flag = 0;
int test_flag = 0;
int record_flag = 0;
int replay_flag = 0;
int fullscreen_flag = 0;
int fastest_flag = 0;
int check_keys_flag = 0;
int skip_password_flag = 1;
int integer_scale_flag = -1; /* -1 = use video.cfg, 0/1 = command line */
int last_level = 0; /* last playable room, for restarting after game over */

short task_pc[64];
short new_task_pc[64];
short enabled_tasks[64];
short new_enabled_tasks[64];

static int key_up, key_down, key_left, key_right, key_a, key_b, key_c, key_select;

/* An action can have several physical keys bound to it. These counters
 * keep the action pressed until every one of them is released; a plain
 * boolean would stop walking when the player lets go of just one of the
 * keys (for example A while Z is still held). */
static int held_up, held_down, held_left, held_right;
static int held_a, held_b, held_c, held_start, held_reset;
static int reset_requested = 0;

/* Gamepad state. The pad is polled once per frame for held buttons and
 * axes; button-down events also set the one-frame tap latches. */
#define JOY_DEADZONE 12000
static SDL_Gamepad *gamepad = NULL;
static int pad_up, pad_down, pad_left, pad_right;
static int pad_a, pad_b, pad_c, pad_select;

/* A tap shorter than one game frame produces a key-down and a key-up
 * event before the script runs. These latches keep that tap visible for
 * exactly one frame so a quick shot/jump/step is never lost. */
static int pressed_up, pressed_down, pressed_left, pressed_right;
static int pressed_a, pressed_b, pressed_c, pressed_start;

/** Keys that were effective for the last update_keys() call, used by
    the key recorder so recorded/replayed input matches the game. */
static unsigned char frame_key_byte = 0;

static Uint64 last_tick = 0, last_tick_fp = 0;

/** Optional alternate key bindings file (--keys). */
static const char *keymap_filename = "keys.txt";

/* 3 bytes header + 256 global variables + aux variables + task state +
 * sprite state */
#define QUICKSAVE_SIZE (3L + 256*2L + MAX_TASKS*32*2L + MAX_TASKS*4*2L + \
	MAX_SPRITES*16L + 3L)

#define RECORDED_KEYS_CACHE 4096
static int cached_keys_offset = 0;
static unsigned char cached_recorded_keys[RECORDED_KEYS_CACHE];

/** file descriptor where keys are written to, or read from */
FILE *record_fp = 0;

/** scratchpad used for unpacking code */
static unsigned char scratchpad[29184];


static int load_room(int index)
{
	char filename[32];
	unsigned char *ptr;

	if (index < 1 || index > 8)
	{
		fprintf(stderr, "invalid room number %d (valid rooms are 1-8)\n", index);
		return -1;
	}

	snprintf(filename, sizeof(filename), "ROOMS%d.BIN", index);

	LOG(("loading %s\n", filename));
	ptr = get_memory_ptr(0xf900);
	if (read_file(filename, ptr) < 0)
	{
		fprintf(stderr, "Could not read %s from the game data.\n", filename);
		panic("load_room failed");
		return -1;
	}

	script_ptr = get_long(0xf900);
	LOG(("script ptr %x\n", script_ptr));

	sound_flush_cache();

	return 0;
}

/** atexit() callback
*/
static void atexit_callback()
{
	if (gamepad != NULL)
	{
		SDL_CloseGamepad(gamepad);
		gamepad = NULL;
	}

	render_done();
	audio_done();
	SDL_Quit();

	if(cls.iso_prefix != NULL)
	{
		free(cls.iso_prefix);
	}
	if(cls.music_prefix != NULL)
	{
		free(cls.music_prefix);
	}
}

static int initialize()
{
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "failed to initialize SDL video: %s\n", SDL_GetError());
		return -1;
	}

	atexit(atexit_callback);

	/* Audio and gamepad are optional. Headless machines and remote
	 * sessions often have no audio device; the game must still run. */
	if (cls.nosound == 0)
	{
		if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		{
			fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
			cls.nosound = 1;
		}
	}

	if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
	{
		fprintf(stderr, "gamepad support disabled: %s\n", SDL_GetError());
	}

	keymap_init(keymap_filename);

	if (cls.nosound == 0)
	{
		if (audio_init() < 0)
		{
			fprintf(stderr, "audio disabled: %s\n", SDL_GetError());
			cls.nosound = 1;
		}
	}

	if (render_init() < 0)
	{
		panic("failed to initialize renderer module");
		return -1;
	}

	if (game2bin_init() < 0)
	{
		fprintf(stderr, "Could not read GAME2.BIN from the game data.\n");
		fprintf(stderr, "Check that the ISO (or loose game files) is present and that --iso points to the right prefix.\n");
		panic("can't read GAME2.BIN file");
		return -1;
	}

	screen_init();
	
	vm_reset();
	set_variable(227, 1);

	if (render_create_surface() < 0)
	{
		panic("failed to create video surface");
		return -1;
	}

	return 0;
}

/** Loads a screen from room file
    @param room    suffix for room%d.bin file
    @param index   screen number
    @returns zero on success, negative on error
*/
int load_room_screen(int room, int index)
{
	int i;
	unsigned char *pixels;

	if (index <= 0)
	{
		LOG(("invalid screen index %d\n", index));
		return -1;
	}

	LOG(("loading room screen %d from room file %d\n", index - 1, room));

	if (unpack_room(scratchpad, index - 1) < 0)
	{
		LOG(("failed to unpack room screen %d\n", index - 1));
		return -1;
	}

	/* convert 4bpp -> 8bpp */
	pixels = (unsigned char *)get_screen_ptr(0);
	for (i=0; i<304*192/2; i++)
	{
		pixels[i*2+0] = scratchpad[i] >> 4;
		pixels[i*2+1] = scratchpad[i] & 0xf;
	}

	current_backdrop = index;
	return 0;
}

/** Rewinds (clears) the recorded-keys cache
*/
void rewind_recorded_keys()
{
	cached_keys_offset = 0;
}

/** Writes down all the recored keys, and rewinds
*/
void flush_recorded_keys()
{
	if (record_fp == NULL)
	{
		cached_keys_offset = 0;
		return;
	}

	fwrite(cached_recorded_keys, 1, cached_keys_offset, record_fp);
	cached_keys_offset = 0;
}

/** Reads keystate from record file

    Will read information sufficient for one rendered frame; if the
    record file ended already, all keys will be considered 'released'
*/
void read_keys_from_record()
{
	int c;

	if (record_fp == NULL)
	{
		c = 0;
	}
	else
	{
		c = fgetc(record_fp);
		if (c == EOF)
		{
			c = 0;
			LOG(("ERROR: record file ended!\n"));
		}
	}

	key_up = (c >> 7) & 1;
	key_down = (c >> 6) & 1;
	key_left = (c >> 5) & 1;
	key_right = (c >> 4) & 1;
	key_a = (c >> 3) & 1;
	key_b = (c >> 2) & 1;
	key_c = (c >> 1) & 1;
	key_select = (c >> 0) & 1;
}

/** Adds a single key to the record file

    Adds a key to the cache array of recorded keys; if the array 
    is full, it will force flushing
*/
void add_keys_to_record()
{
	if (record_fp == NULL)
	{
		return;
	}

	/* frame_key_byte was built by update_keys(), so it contains the
	 * effective state including a one-frame tap. */
	cached_recorded_keys[cached_keys_offset++] = frame_key_byte;
 	if (cached_keys_offset == sizeof(cached_recorded_keys))
	{
 		/* if the player never quicksaves or quickloads */
		flush_recorded_keys();
	}
}

/** Translates gamepad presses and updates variables

    Entry at b6c4
*/
void update_keys()
{
	short flags;
	int up, down, left, right, a, b, c, select;

	if (replay_flag)
	{
		/* replay drives the keys directly; physical latches must not
		 * leak into the recorded sequence */
		up = key_up;
		down = key_down;
		left = key_left;
		right = key_right;
		a = key_a;
		b = key_b;
		c = key_c;
		select = key_select;
		pressed_up = pressed_down = pressed_left = pressed_right = 0;
		pressed_a = pressed_b = pressed_c = pressed_start = 0;
	}
	else
	{
		up = key_up || pressed_up || pad_up;
		down = key_down || pressed_down || pad_down;
		left = key_left || pressed_left || pad_left;
		right = key_right || pressed_right || pad_right;
		a = key_a || pressed_a || pad_a;
		b = key_b || pressed_b || pad_b;
		c = key_c || pressed_c || pad_c;
		select = key_select || pressed_start || pad_select;
	}

	toggle_aux(0);           /* me and my paranoia */
	set_variable(253, 0);
	set_variable(252, 0);
	set_variable(229, 0);
	set_variable(251, 0);

	flags = 0;

	if (right)
	{
		set_variable(252, 1);
		flags |= 1;
	}
	else if (left)
	{
		set_variable(252, -1);
		flags |= 2;
	}
	else if (down)
	{
		set_variable(251, 1);
		set_variable(229, 1);
		flags |= 4;
	}

	if (up)
	{
		set_variable(229, -1);
		flags |= 8;
	}

	if (c)
	{
		set_variable(251, -1);
		flags |= 8;
		/* some if  here! */
	}

	set_variable(253, flags);

	/* b748 */
	set_variable(250, 0);
	set_variable(254, get_variable(253));

	if (b)
	{
		set_variable(254, (unsigned short)(get_variable(254) | 0x40));
	}
	else if (a)
	{
		set_variable(250, 1);
		set_variable(254, (unsigned short)(get_variable(254) | 0x80));
	}

	/* The game-over screen uses variable 226 as "start pressed":
	 * leaving it set opens the password entry screen. */
	set_variable(226, select ? 1 : 0);

	frame_key_byte = (unsigned char)((up << 7) | (down << 6) |
		(left << 5) | (right << 4) | (a << 3) | (b << 2) |
		(c << 1) | select);

	if (!replay_flag)
	{
		/* consume one-frame taps; held keys stay pressed next frame */
		key_up = held_up > 0;
		key_down = held_down > 0;
		key_left = held_left > 0;
		key_right = held_right > 0;
		key_a = held_a > 0;
		key_b = held_b > 0;
		key_c = held_c > 0;
		key_select = held_start > 0;
		pressed_up = pressed_down = pressed_left = pressed_right = 0;
		pressed_a = pressed_b = pressed_c = pressed_start = 0;
	}
}

/** Loads a quicksave file
*/
void quickload()
{
	int i, j;
	int palette_used;
	long file_size;
	FILE *fp;

	if (current_room < 1 || current_room > 8)
	{
		/* still in the intro or a test screen */
		return;
	}

	/* the recorded key stream should not lose the frames before the load */
	if (record_flag)
	{
		flush_recorded_keys();
	}

	fp = hota_fopen(QUICKSAVE_FILENAME, "rb");
	if (fp == NULL)
	{
		perror("failed to load 'quicksave' file\n");
		return;
	}

	if (fseek(fp, 0, SEEK_END) != 0)
	{
		perror("failed to read 'quicksave' file\n");
		fclose(fp);
		return;
	}

	file_size = ftell(fp);
	if (fseek(fp, 0, SEEK_SET) != 0 || file_size != QUICKSAVE_SIZE)
	{
		fprintf(stderr, "invalid 'quicksave' file (size %ld, expected %ld)\n",
			file_size, (long)QUICKSAVE_SIZE);
		fclose(fp);
		return;
	}

	current_room = fgetc(fp);
	current_backdrop = fgetc(fp);
	palette_used = fgetc(fp);

	if (current_room < 1 || current_room > 8 || current_backdrop < 1)
	{
		fprintf(stderr, "invalid 'quicksave' file (room %d, backdrop %d)\n",
			current_room, current_backdrop);
		fclose(fp);
		return;
	}

	last_level = current_room;

	/* Drop every old buffer, then put the saved backdrop back. */
	screen_clear_all();

	if (load_room(current_room) < 0 ||
		load_room_screen(0, current_backdrop) < 0)
	{
		fprintf(stderr, "failed to load room %d from 'quicksave' file\n",
			current_room);
		fclose(fp);
		return;
	}

	set_palette(palette_used);

	/* must be ran out of thread loop, so no active thread */
	toggle_aux(0);
	for (i=0; i<256; i++)
	{
		set_variable(i, fgetw(fp));
	}

	toggle_aux(1);
	for (j=0; j<MAX_TASKS; j++)
	{
		set_aux_bank(j);
		for (i=0; i<32; i++)	
		{
			set_variable(i, fgetw(fp));
		}
	}

	/* when frame ends, aux is always zero anyway */
	toggle_aux(0);

	for (i=0; i<MAX_TASKS; i++)
	{
		task_pc[i] = fgetw(fp);
		new_task_pc[i] = fgetw(fp);
		enabled_tasks[i] = fgetw(fp);
		new_enabled_tasks[i] = fgetw(fp);
	}

	quickload_sprites(fp);

	fclose(fp);
}

/** Creates a quicksave file

    Quicksave file includes all that is required so later on a user can
    use the quickload and be provided with the exact same game state. 
*/    
void quicksave()
{
	int i, j;
	FILE *fp;

	if (current_room < 1 || current_room > 8 || current_backdrop < 1)
	{
		/* still in the intro, a test screen, or before the first
		 * screen of a room has been loaded */
		return;
	}

	if (record_flag)
	{
		flush_recorded_keys();
	}

	fp = hota_fopen(QUICKSAVE_FILENAME, "wb");
	if (fp == NULL)
	{
		perror("failed to create 'quicksave' file\n");
		return;
	}

	fputc(current_room, fp);
	fputc(current_backdrop, fp);
	fputc(get_current_palette(), fp);

	/* must be ran out of thread loop, so no active thread */
	toggle_aux(0);
	for (i=0; i<256; i++)
	{
		fputw(get_variable(i), fp);
	}

	toggle_aux(1);
	for (j=0; j<MAX_TASKS; j++)
	{
		set_aux_bank(j);
		for (i=0; i<32; i++)	
		{
			fputw(get_variable(i), fp);
		}
	}

	toggle_aux(0);

	for (i=0; i<MAX_TASKS; i++)
	{
		fputw(task_pc[i], fp);
		fputw(new_task_pc[i], fp);
		fputw(enabled_tasks[i], fp);
		fputw(new_enabled_tasks[i], fp);
	}

	quicksave_sprites(fp);

	if (ferror(fp))
	{
		perror("failed to write 'quicksave' file\n");
	}

	fclose(fp);
}

/** Releases every game key.

    Called when the window loses focus: the key-up events for keys that
    were held at that moment are delivered to another window, so without
    this the character would keep walking after alt-tabbing back.
*/
static void release_all_keys()
{
	key_up = key_down = key_left = key_right = 0;
	key_a = key_b = key_c = key_select = 0;
	held_up = held_down = held_left = held_right = 0;
	held_a = held_b = held_c = held_start = held_reset = 0;
	pressed_up = pressed_down = pressed_left = pressed_right = 0;
	pressed_a = pressed_b = pressed_c = pressed_start = 0;
	pad_up = pad_down = pad_left = pad_right = 0;
	pad_a = pad_b = pad_c = pad_select = 0;
	reset_requested = 0;
	speed_throttle = 0;
}

/** Opens the first connected gamepad, if any. */
static void gamepad_open_first(void)
{
	int count = 0;
	SDL_JoystickID *ids;

	if (gamepad != NULL)
	{
		return;
	}

	ids = SDL_GetGamepads(&count);
	if (ids == NULL)
	{
		return;
	}

	if (count > 0)
	{
		gamepad = SDL_OpenGamepad(ids[0]);
		if (gamepad != NULL)
		{
			LOG(("opened gamepad %d\n", (int)ids[0]));
		}
	}

	SDL_free(ids);
}

/** Refreshes the held state of the gamepad. */
static void poll_gamepad(void)
{
	int axis_x = 0;
	int axis_y = 0;

	if (gamepad == NULL)
	{
		pad_up = pad_down = pad_left = pad_right = 0;
		pad_a = pad_b = pad_c = pad_select = 0;
		return;
	}

	axis_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
	axis_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);

	pad_left = axis_x < -JOY_DEADZONE ||
		SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
	pad_right = axis_x > JOY_DEADZONE ||
		SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
	pad_up = axis_y < -JOY_DEADZONE ||
		SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
	pad_down = axis_y > JOY_DEADZONE ||
		SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);

	pad_a = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
	pad_b = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST);
	pad_c = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST);
	pad_select = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
}

/** Processes SDL events

    This processes windows messages, keyboard pressed, joystick moves,
    and general SDL events
*/
void check_events()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
	        switch (event.type) 
		{
			case SDL_EVENT_GAMEPAD_ADDED:
			if (gamepad == NULL)
			{
				gamepad = SDL_OpenGamepad(event.gdevice.which);
				if (gamepad != NULL)
				{
					LOG(("opened gamepad %d\n", (int)event.gdevice.which));
				}
			}
			break;

			case SDL_EVENT_GAMEPAD_REMOVED:
			if (gamepad != NULL &&
				SDL_GetGamepadID(gamepad) == event.gdevice.which)
			{
				SDL_CloseGamepad(gamepad);
				gamepad = NULL;
				poll_gamepad();
				gamepad_open_first();
			}
			break;

			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			if (gamepad != NULL &&
				SDL_GetGamepadID(gamepad) == event.gbutton.which)
			{
				switch (event.gbutton.button)
				{
					case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
					pressed_right = 1;
					break;

					case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
					pressed_left = 1;
					break;

					case SDL_GAMEPAD_BUTTON_DPAD_UP:
					pressed_up = 1;
					break;

					case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
					pressed_down = 1;
					break;

					case SDL_GAMEPAD_BUTTON_SOUTH:
					pressed_a = 1;
					break;

					case SDL_GAMEPAD_BUTTON_WEST:
					pressed_b = 1;
					break;

					case SDL_GAMEPAD_BUTTON_EAST:
					pressed_c = 1;
					break;

					case SDL_GAMEPAD_BUTTON_START:
					pressed_start = 1;
					break;

					case SDL_GAMEPAD_BUTTON_NORTH:
					toggle_filter();
					break;

					case SDL_GAMEPAD_BUTTON_BACK:
					cls.quit = 1;
					break;

					case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
					quicksave();
					break;

					case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
					quickload();
					break;

					default:
					break;
				}
			}
			break;

			case SDL_EVENT_GAMEPAD_BUTTON_UP:
			/* held state is refreshed by poll_gamepad() */
			break;

			case SDL_EVENT_KEY_UP:
			/* Ignore modifiers here: the player may release Ctrl or
			 * Alt before releasing the bound key. */
			if (keymap_matches_key(KM_RIGHT, event.key.key))
			{
				if (held_right > 0) held_right--;
				key_right = held_right > 0;
			}
			if (keymap_matches_key(KM_LEFT, event.key.key))
			{
				if (held_left > 0) held_left--;
				key_left = held_left > 0;
			}
			if (keymap_matches_key(KM_UP, event.key.key))
			{
				if (held_up > 0) held_up--;
				key_up = held_up > 0;
			}
			if (keymap_matches_key(KM_DOWN, event.key.key))
			{
				if (held_down > 0) held_down--;
				key_down = held_down > 0;
			}
			if (keymap_matches_key(KM_A, event.key.key))
			{
				if (held_a > 0) held_a--;
				key_a = held_a > 0;
			}
			if (keymap_matches_key(KM_B, event.key.key))
			{
				if (held_b > 0) held_b--;
				key_b = held_b > 0;
			}
			if (keymap_matches_key(KM_C, event.key.key))
			{
				if (held_c > 0) held_c--;
				key_c = held_c > 0;
			}
			if (keymap_matches_key(KM_START, event.key.key))
			{
				if (held_start > 0) held_start--;
				key_select = held_start > 0;
			}
			if (keymap_matches_key(KM_RESET, event.key.key))
			{
				if (held_reset > 0) held_reset--;
			}
			if (keymap_matches_key(KM_SPEED, event.key.key))
			{
				speed_throttle = 0;
			}
			break;

		case SDL_EVENT_KEY_DOWN:
			/* Ignore auto-repeat: movement keys keep their state, and
			 * action keys (save/load/screenshot/...) must trigger once. */
			if (event.key.repeat)
			{
				break;
			}

			#ifdef ENABLE_DEBUG
			/* enable/disable sprites */
			switch (event.key.key)
			{
				case SDLK_1:
				case SDLK_2:
				case SDLK_3:
				case SDLK_4:
				case SDLK_5:
				case SDLK_6:
				case SDLK_7:
				case SDLK_8:
				case SDLK_9:
				{
					int tmp = event.key.key - SDLK_1 + 1;

					if (event.key.mod & SDL_KMOD_SHIFT)
					{
						tmp = tmp + 10;
					}

					sprites[tmp].u1 ^= 0x80;
				}
				break;

				case SDLK_G:
				debug_flag ^= 1;
				break;

				default:
				break;
			}
			#endif

			if (keymap_matches(KM_RIGHT, event.key.key, event.key.mod))
			{
				held_right++;
				key_right = 1;
				pressed_right = 1;
			}
			if (keymap_matches(KM_LEFT, event.key.key, event.key.mod))
			{
				held_left++;
				key_left = 1;
				pressed_left = 1;
			}
			if (keymap_matches(KM_UP, event.key.key, event.key.mod))
			{
				held_up++;
				key_up = 1;
				pressed_up = 1;
			}
			if (keymap_matches(KM_DOWN, event.key.key, event.key.mod))
			{
				held_down++;
				key_down = 1;
				pressed_down = 1;
			}
			if (keymap_matches(KM_A, event.key.key, event.key.mod))
			{
				held_a++;
				key_a = 1;
				pressed_a = 1;
				if (held_reset > 0)
				{
					reset_requested = 1;
				}
			}
			if (keymap_matches(KM_B, event.key.key, event.key.mod))
			{
				held_b++;
				key_b = 1;
				pressed_b = 1;
			}
			if (keymap_matches(KM_C, event.key.key, event.key.mod))
			{
				held_c++;
				key_c = 1;
				pressed_c = 1;
			}
			/* Alt+Enter is fullscreen, not the start button. */
			if (keymap_matches(KM_START, event.key.key, event.key.mod) &&
				!(event.key.key == SDLK_RETURN &&
					(event.key.mod & SDL_KMOD_ALT)))
			{
				held_start++;
				key_select = 1;
				pressed_start = 1;
			}
			if (keymap_matches(KM_RESET, event.key.key, event.key.mod))
			{
				held_reset++;
				if (held_a > 0)
				{
					reset_requested = 1;
				}
			}
			if (keymap_matches(KM_MUTE, event.key.key, event.key.mod))
			{
				cls.muted = !cls.muted;
				audio_apply_volumes();
			}
			if (keymap_matches(KM_QUICKSAVE, event.key.key, event.key.mod))
			{
				quicksave();
			}
			if (keymap_matches(KM_QUICKLOAD, event.key.key, event.key.mod))
			{
				quickload();
			}
			if (keymap_matches(KM_SCREENSHOT, event.key.key, event.key.mod))
			{
				screenshot();
			}
			if (keymap_matches(KM_FULLSCREEN, event.key.key, event.key.mod))
			{
				toggle_fullscreen();
			}
			if (keymap_matches(KM_SPEED, event.key.key, event.key.mod))
			{
				speed_throttle = 1;
			}
			if (keymap_matches(KM_UPSCALE, event.key.key, event.key.mod))
			{
				toggle_upscale();
			}
			if (keymap_matches(KM_SCALER, event.key.key, event.key.mod))
			{
				toggle_scaler();
			}
			if (keymap_matches(KM_FILTER, event.key.key, event.key.mod))
			{
				toggle_filter();
			}
			if (keymap_matches(KM_INTEGER, event.key.key, event.key.mod))
			{
				toggle_integer_scale();
			}
			if (keymap_matches(KM_QUIT, event.key.key, event.key.mod))
			{
				cls.quit = 1;
			}
			break;

		case SDL_EVENT_QUIT:
			cls.quit = 1;
			break;

		case SDL_EVENT_WINDOW_FOCUS_LOST:
		case SDL_EVENT_WINDOW_MINIMIZED:
			/* key-up events go to the other window, so forget any
			 * key that is still held, and pause the game */
			release_all_keys();
			cls.paused = 1;
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_RESTORED:
			if (cls.paused)
			{
				/* resume without trying to replay the paused time */
				cls.paused = 0;
				last_tick = SDL_GetTicks();
				last_tick_fp = 0;
			}
			break;

		default:
			break;
		}
	}

	poll_gamepad();
}

void rest(int fps)
{
	if (fastest_flag == 0)
	{
		/* only set reference */
		if (fps <= 0)
		{
			last_tick = SDL_GetTicks();
			last_tick_fp = 0;
			return;
		}

		if (speed_throttle == 1)
		{
			/* 10 times faster */
			fps = fps*10;
		}

		Uint64 diff = ((1000ULL << 16) / (Uint64)fps) + last_tick_fp;
		last_tick_fp = diff & 0xffff;
		diff = diff >> 16;
		Uint64 current_tick = SDL_GetTicks();

		/* After a long stall (alt-tab, debugger, slow room load) do
		 * not replay a burst of frames to catch up: that would skip
		 * over the player's input. Resume from the current time. */
		if (current_tick - last_tick > 1000)
		{
			last_tick = current_tick;
			last_tick_fp = 0;
		}

		while (current_tick - last_tick < diff)
		{
			SDL_Delay(1);
			current_tick = SDL_GetTicks();
		}
		last_tick += diff;
	}
}

/** Initializes all tasks to stopped state
*/
void init_tasks()
{
	int i;

	for (i=0; i<MAX_TASKS; i++)
	{
		task_pc[i] = -1;
		new_task_pc[i] = -1;

		enabled_tasks[i] = 0;
		new_enabled_tasks[i] = 0;
	}

	toggle_aux(0);
	task_pc[0] = 0;
}

/** Plays several animations at once
    @param anm        pointer to an array of anm_file_t entries
    @param n          number of elements in array
    @param skippable  skip to next animation if key pressed (otherwise breaks)
    @returns zero on completion of all videos, 1 if skipped, negative on error

    Note that cls.quit might be true; in that case, return value of one will 
    be returned
*/
int play_anm(anm_file_t *anm, int n, int skippable)
{
	int seq;
	int ret;

	ret = 0;
	for (seq = 0; seq < n; seq++)
	{
		if (cls.quit == 0)
		{
			int ok;

			play_music_track(anm[seq].track, 0);
			ok = play_animation(anm[seq].filename, anm[seq].offset);
			if (ok < 0)
			{
				ret = ok;
				break;
			}

			if (ok == 1 && skippable == 0)
			{
				ret = 1;
				break;
			}
		}
	}

	stop_music();
	return ret;
}

/** Plays the introduction to the game

    Introduction is split into 4 files, since sega-cd was limited with 
    512 KB of ram, and video is loaded into memory before it can be
    played. also, each such sequence comes with it's own audio track
*/
static void play_intro()
{
	play_anm(anm_files, 4, 0);
}

/** Main game loop

    This is where all the magic happens!
*/
static void run()
{
	cls.quit = 0;
	init_tasks();

	if (next_script == 0)
	{
		/* if no room specified, then follow the original flow:
		 * first play intro, then jump to code entry script.
		 */
		play_intro();
		/* The code entry screen is a level selector. By default jump
		 * straight into the first level instead. */
		next_script = skip_password_flag ? 1 : 7;
	}

	rest(0);

	while (cls.quit == 0)
	{
		int i;

		if (next_script != 0)
		{
			if (next_script >= 1 && next_script <= 8)
			{
				/* remember the level so a game over can restart it */
				last_level = next_script;
			}

			current_room = next_script;
			current_backdrop = 0;
			reset_sprite_list();
			init_tasks();

			/* Do not leave the last frame of the previous room on
			 * screen while the new room is read and unpacked. The
			 * password/continue screen in particular used to stay
			 * visible for a moment when the first level started. */
			screen_clear_all();
			render_blank();

			LOG(("loading room %d\n", current_room));
			load_room(current_room);
			next_script = 0;
		}

		check_events();

		if (cls.paused)
		{
			/* keep the event loop responsive while the window is
			 * in the background */
			SDL_Delay(16);
			continue;
		}

		if (reset_requested)
		{
			reset_requested = 0;
			if (current_room >= 1 && current_room <= 8)
			{
				/* reload the current room, as if the level was
				 * restarted from the continue screen */
				next_script = current_room;
			}
		}

		if (replay_flag)
		{
			read_keys_from_record();
		}

		update_keys();

		if (record_flag)
		{
			add_keys_to_record();
		}
	
		LOG(("*new frame*\n"));

		for (i=0; i<MAX_TASKS; i++)
		{
			int d0;

			/* 70d0 */
			enabled_tasks[i] = new_enabled_tasks[i];

			d0 = new_task_pc[i];
			if (d0 == -1)
			{
				continue;
			}
			
			if (d0 == -2)
			{
				d0 = -1;
			}		

			task_pc[i] = d0;
			new_task_pc[i] = -1;
		}

		for (i=0; i<MAX_TASKS; i++)
		{
			int pc = task_pc[i];
			if (pc != INVALID_PC && enabled_tasks[i] == 0)
			{
				toggle_aux(0);
				set_aux_bank(i);
				LOG(("task %d starts at 0x%x\n", i, pc));
				task_pc[i] = decode(i, pc);
				LOG(("task %d ended at 0x%x\n", i, pc));
			}

			if (next_script != 0)
			{
				/* script has been changed */
				break;
			}
		}

		music_update();

		if (next_script != 0)
		{
			/* The script asked for another room. Blank the display
			 * right away instead of leaving this room's last frame
			 * (often the password/continue screen) up for another
			 * frame, then load the room at the top of the loop. */
			render_blank();
			continue;
		}

		rest(12);
	}
}

/** Runs the animation-test, enabled with --animation-test
*/
static void animation_test()
{
	int files = sizeof(anm_files) / sizeof(anm_files[0]);
	play_anm(anm_files, files, 1);
}

/** Runs the sprite-test 

    Interactively show the sprites in this 'room' file. Use the --room
    parameter to view sprites in other rooms
*/
void sprite_test()
{
	int redraw;

	load_room(next_script);

	sprites[0].index = 0;
	sprites[0].frame = 0;
	sprites[0].x = 10;
	sprites[0].y = 10;

	cls.quit = 0;

	redraw = 1;
	set_palette(0x11);
	rest(0);

	while (cls.quit == 0)
	{
		int a4;

		a4 = get_long(0xf904) + (sprites[0].index << 2);
		a4 = get_long(a4);

		if (redraw)
		{
			int selected_screen;
			void *background;

			selected_screen = get_selected_screen();
			background = get_screen_ptr(selected_screen);
			memset(background, 0xff, 304*192);

			render_sprite(0);
			render(background);
			redraw = 0;
			print_sprite(0);
		}

		check_events();
		if (cls.paused)
		{
			SDL_Delay(16);
			continue;
		}

		rest(12);
		update_keys();

		if (get_variable(252) == 1)
		{
			int i = (sprites[0].frame & 0x7f) + 1;
			sprites[0].frame = (sprites[0].frame & 0x80) | i;
			if (sprites[0].frame > get_byte(a4))
			{
				sprites[0].frame &= 0x80;
			}

			redraw = 1;
		}

		if (get_variable(252) == -1)
		{
			if (sprites[0].frame > 0)
			{
				int i = (sprites[0].frame & 0x7f) - 1;
				redraw = 1;
				sprites[0].frame = (sprites[0].frame & 0x80) | i;
			}
		}

		if (get_variable(229) == 1)
		{
			redraw = 1;
			if (sprites[0].index < 255)
			{
				sprites[0].index++;
			}
			sprites[0].frame &= 0x80;
		}

		if (get_variable(229) == -1)
		{
			if (sprites[0].index > 0)
			{
				redraw = 1;
				sprites[0].index--;
				sprites[0].frame &= 0x80;
			}
		}

		if (get_variable(250))
		{
			/* flip */
			sprites[0].frame ^= 0x80;
		}

		set_variable(229, 0);
		set_variable(252, 0);
	}
}

static void help()
{
	printf("Heart of The Alien Redux %s\n", HOTA_VERSION);
	puts("USAGE:");
	puts("\t--debug        turn on debugging output");
	puts("\t--scale x      window scale factor <x> (0 = auto)");
	puts("\t--upscale n    internal upscaling 1-4 (0 = auto)");
	puts("\t--smooth       use Scale2x/3x smoothing (default: sharp)");
	puts("\t--filter       use bilinear filtering");
	puts("\t--hd           HD rendering via OpenGL (default)");
	puts("\t--no-hd        disable HD, use the SDL renderer");
	puts("\t--sharp        HD: nearest shader instead of Omniscale");
	puts("\t--hd-smooth n  HD: Omniscale diagonal smoothing 0-100 (default: 30)");
	puts("\t--integer-scale    pixel-perfect integer scaling (black borders)");
	puts("\t--no-integer-scale fractional/letterbox scaling (default)");
	puts("\t--fullscreen   start in fullscreen");
	puts("\t--no-sound     disable music and sound effects");
	puts("\t--music-volume n  music volume 0-100 (default: 100)");
	puts("\t--sfx-volume n    sound effects volume 0-100 (default: 100)");
	puts("\t--skip-password   skip code entry; restart the level after game over (default)");
	puts("\t--no-skip-password show the original code entry screen");
	puts("\t--room n       start from room <n>");
	puts("\t--iso prefix   use ISO image <prefix.iso>");
	puts("\t--music prefix use music <prefix.[wav/mp3/ogg/opus/flac]>");
	puts("\t--keys file    read key bindings from <file> (default: keys.txt)");
	puts("\t--check-keys   validate the key bindings file and exit");
	puts("\t--sprite-test  run sprite test (use with room)");
	puts("\t--intro-test   play all animations");
	puts("\t--fastest      speed throttle");
	puts("\t--record       record keys");
	puts("\t--replay       replay keys");
	puts("\t--help         this help");
}

static struct parg_option options[] =
{
	{"debug", PARG_NOARG, &debug_flag, 1},
	{"room", PARG_REQARG, NULL, 'r'},
	{"iso", PARG_OPTARG, NULL, 'i'},
	{"music", PARG_REQARG, NULL, 'm'},
	{"keys", PARG_REQARG, NULL, 'k'},
	{"check-keys", PARG_NOARG, &check_keys_flag, 1},
	{"skip-password", PARG_NOARG, &skip_password_flag, 1},
	{"no-skip-password", PARG_NOARG, &skip_password_flag, 0},
	{"sprite-test", PARG_NOARG, &test_flag, 1},
	{"intro-test", PARG_NOARG, &test_flag, 2},
	{"help", PARG_NOARG, NULL, 'h'},
	{"no-sound", PARG_NOARG, NULL, 'n'},
	{"music-volume", PARG_REQARG, NULL, 'M'},
	{"sfx-volume", PARG_REQARG, NULL, 'V'},
	{"fullscreen", PARG_NOARG, &fullscreen_flag, 1},
	{"record", PARG_NOARG, &record_flag, 1},
	{"replay", PARG_NOARG, &replay_flag, 1},
	{"scale", PARG_REQARG, NULL, 's'},
	{"upscale", PARG_REQARG, NULL, 'u'},
	{"smooth", PARG_NOARG, &cls.smooth, 1},
	{"filter", PARG_NOARG, NULL, 'f'},
	{"hd", PARG_NOARG, &cls.hd, 1},
	{"no-hd", PARG_NOARG, &cls.hd, 0},
	{"sharp", PARG_NOARG, &cls.hd_sharp, 1},
	{"hd-smooth", PARG_REQARG, NULL, 'S'},
	{"integer-scale", PARG_NOARG, &integer_scale_flag, 1},
	{"no-integer-scale", PARG_NOARG, &integer_scale_flag, 0},
	{"fastest", PARG_NOARG, &fastest_flag, 1},
	{0, 0, 0, 0}
};

/** Duplicates an ISO/music prefix, dropping a trailing ".iso" so
    "--iso game.iso" and "--iso game" mean the same thing.
*/
static char *dup_prefix(const char *text)
{
	char *copy;
	size_t len;

	if (text == NULL)
	{
		return NULL;
	}

	copy = strdup(text);
	if (copy == NULL)
	{
		return NULL;
	}

	len = strlen(copy);
	if (len > 4 && SDL_strcasecmp(copy + len - 4, ".iso") == 0)
	{
		copy[len - 4] = '\0';
	}

	return copy;
}

/** Parses a complete integer command line value.
    @returns zero on success
*/
static int parse_int(const char *text, int *value)
{
	char *end;
	long parsed;

	if (text == NULL || *text == '\0')
	{
		return -1;
	}

	parsed = strtol(text, &end, 10);
	if (*end != '\0' || parsed < INT_MIN || parsed > INT_MAX)
	{
		return -1;
	}

	*value = (int)parsed;
	return 0;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
	/* The C runtime gives ANSI argv on Windows. Re-read the command
	 * line so UTF-8 paths (accented folders, for example) survive. */
	{
		int utf8_argc = 0;
		char **utf8_argv = hota_utf8_argv(&utf8_argc);

		if (utf8_argv != NULL)
		{
			argc = utf8_argc;
			argv = utf8_argv;
		}
	}
#endif

	next_script = 0;

	// set default options
	cls.scale = 0;
	cls.upscale = 0;
	cls.smooth = 0;
	cls.filtered = 0;
	cls.hd = 1;
	cls.hd_sharp = 0;
	cls.hd_smooth = 30;
	cls.music_volume = 100;
	cls.sfx_volume = 100;
	cls.muted = 0;
	cls.integer_scale = 0;
	cls.fullscreen = 0;
	cls.speed_throttle = 0;
	cls.paused = 0;
	cls.nosound = 0;
	cls.use_iso = false;
	cls.iso_prefix = NULL;
	cls.music_prefix = NULL;

	// parse CLI options
	struct parg_state ps;
	parg_init(&ps);
	int c;
	while ((c = parg_getopt_long(&ps, argc, argv, "hdr:ns:fi::m:k:S:M:V:", options, NULL)) != -1)
	{
		switch(c)
		{
			case 'r':
			if (parse_int(ps.optarg, &next_script) < 0)
			{
				fprintf(stderr, "invalid room number '%s'\n", ps.optarg);
				return 1;
			}
			break;

			case 'h':
			help();
			return 0;

			case 'd':
			debug_flag = 1;
			break;

			case 's':
			if (parse_int(ps.optarg, &cls.scale) < 0)
			{
				fprintf(stderr, "invalid scale factor '%s' (valid: 0-16, 0 = auto)\n", ps.optarg);
				return 1;
			}
			if (cls.scale < 0 || cls.scale > 16)
			{
				fprintf(stderr, "invalid scale factor %d (valid: 0-16, 0 = auto)\n", cls.scale);
				return 1;
			}
			break;

			case 'u':
			if (parse_int(ps.optarg, &cls.upscale) < 0)
			{
				fprintf(stderr, "invalid upscale factor '%s' (valid: 0-4, 0 = auto)\n", ps.optarg);
				return 1;
			}
			if (cls.upscale < 0 || cls.upscale > 4)
			{
				fprintf(stderr, "invalid upscale factor %d (valid: 0-4, 0 = auto)\n", cls.upscale);
				return 1;
			}
			break;

			case 'f':
			cls.filtered = 1;
			break;

			case 'S':
			if (parse_int(ps.optarg, &cls.hd_smooth) < 0)
			{
				fprintf(stderr, "invalid HD smoothing '%s' (valid: 0-100)\n", ps.optarg);
				return 1;
			}
			if (cls.hd_smooth < 0 || cls.hd_smooth > 100)
			{
				fprintf(stderr, "invalid HD smoothing %d (valid: 0-100)\n", cls.hd_smooth);
				return 1;
			}
			break;

			case 'n':
			cls.nosound = 1;
			break;

			case 'M':
			if (parse_int(ps.optarg, &cls.music_volume) < 0 ||
				cls.music_volume < 0 || cls.music_volume > 100)
			{
				fprintf(stderr, "invalid music volume '%s' (valid: 0-100)\n", ps.optarg);
				return 1;
			}
			break;

			case 'V':
			if (parse_int(ps.optarg, &cls.sfx_volume) < 0 ||
				cls.sfx_volume < 0 || cls.sfx_volume > 100)
			{
				fprintf(stderr, "invalid sound volume '%s' (valid: 0-100)\n", ps.optarg);
				return 1;
			}
			break;

			case 'i':
			cls.use_iso = true;
			/* manually check for optional argument as next arg */
			if (ps.optarg == NULL && ps.optind < argc && argv[ps.optind][0] != '-')
			{
				ps.optarg = argv[ps.optind++];
			}
			if (ps.optarg != NULL)
			{
				free(cls.iso_prefix);
				cls.iso_prefix = dup_prefix(ps.optarg);
			}
			break;

			case 'm':
			free(cls.music_prefix);
			cls.music_prefix = dup_prefix(ps.optarg);
			break;

			case 'k':
			keymap_filename = ps.optarg;
			break;

			case '?':
			panic("invalid argument");
			return 1;
		}
	}

	if (replay_flag && record_flag)
	{
		fprintf(stderr, "cant specify both replay and record\n");
		return 1;
	}

	if (next_script < 0 || next_script > 8)
	{
		fprintf(stderr, "invalid room number %d (valid rooms are 1-8)\n", next_script);
		return 1;
	}

	if (check_keys_flag)
	{
		/* validate the bindings file without starting the game */
		int inited = SDL_Init(0);

		keymap_init(keymap_filename);
		if (inited)
		{
			SDL_Quit();
		}
		return 0;
	}

	if (replay_flag)
	{
		record_fp = hota_fopen(RECORDED_KEYS_FILENAME, "rb");
		if (record_fp == NULL)
		{
			perror("failed to open 'recorded-keys' file for replay");
			return 1;
		}
	}
	else if (record_flag)
	{
		record_fp = hota_fopen(RECORDED_KEYS_FILENAME, "wb");
		if (record_fp == NULL)
		{
			perror("failed to create 'recorded-keys' file");
			return 1;
		}
	}

	if (initialize() < 0)
	{
		if (record_fp != NULL)
		{
			fclose(record_fp);
			record_fp = NULL;
		}
		return 1;
	}

	gamepad_open_first();

	if (test_flag == 1 && next_script == 0)
	{
		/* sprite test needs a room; use the first one by default */
		next_script = 1;
	}

	switch(test_flag)
	{
		case 0:
		run();
		break;

		case 1:
		sprite_test();
		break;

		case 2:
		animation_test();
		break;

		default:
		fprintf(stderr, "unknown test_flag %d\n", test_flag);
		return 1;
	}

	if (record_fp != NULL)
	{
		flush_recorded_keys();
		fclose(record_fp);
	}

	return 0;
}
