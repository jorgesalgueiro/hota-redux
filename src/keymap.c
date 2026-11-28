/*
 * Heart of The Alien Redux: user configurable key bindings
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
#include <ctype.h>

#include <SDL3/SDL.h>

#include "keymap.h"
#include "platform.h"

#define KM_MAX_BINDINGS 4
#define KM_LINE_SIZE 512

typedef struct
{
	SDL_Keycode key;
	SDL_Keymod mod;
} keymap_binding_t;

typedef struct
{
	keymap_action action;
	const char *name;
	const char *def;
	const char *doc;
} keymap_entry_t;

static const keymap_entry_t keymap_entries[] =
{
	{ KM_UP,         "move up",    "up",          "walk up" },
	{ KM_DOWN,       "move down",  "down",        "walk down" },
	{ KM_LEFT,       "move left",  "left",        "walk left" },
	{ KM_RIGHT,      "move right", "right",       "walk right" },
	{ KM_A,          "action a",   "z, a",        "action button A" },
	{ KM_B,          "action b",   "x, s",        "action button B" },
	{ KM_C,          "action c",   "c, d",        "action button C" },
	{ KM_START,      "start",      "enter, kpenter", "start / select button (continue, password entry)" },
	{ KM_MUTE,       "mute",       "m",           "toggle music and sound effects" },
	{ KM_QUICKSAVE,  "quicksave",  "f5",          "save the game to slot 1 ('quicksave')" },
	{ KM_QUICKLOAD,  "quickload",  "f7",          "load the game from slot 1 ('quicksave')" },
	{ KM_SAVE_SLOT_2, "save slot 2", "ctrl+f6",   "save the game to slot 2 ('save2')" },
	{ KM_SAVE_SLOT_3, "save slot 3", "ctrl+f7",   "save the game to slot 3 ('save3')" },
	{ KM_SAVE_SLOT_4, "save slot 4", "ctrl+f8",   "save the game to slot 4 ('save4')" },
	{ KM_SAVE_SLOT_5, "save slot 5", "ctrl+f9",   "save the game to slot 5 ('save5')" },
	{ KM_LOAD_SLOT_2, "load slot 2", "shift+f6",  "load the game from slot 2 ('save2')" },
	{ KM_LOAD_SLOT_3, "load slot 3", "shift+f7",  "load the game from slot 3 ('save3')" },
	{ KM_LOAD_SLOT_4, "load slot 4", "shift+f8",  "load the game from slot 4 ('save4')" },
	{ KM_LOAD_SLOT_5, "load slot 5", "shift+f9",  "load the game from slot 5 ('save5')" },
	{ KM_SCREENSHOT, "screenshot", "f12",         "write 'hota-screenshot.png'" },
	{ KM_FULLSCREEN, "fullscreen", "alt+enter",   "toggle fullscreen" },
	{ KM_SPEED,      "speed",      "space",       "hold to run the game 10x faster" },
	{ KM_RESET,      "reset",      "q",           "hold with action A to restart the level" },
	{ KM_UPSCALE,    "upscale",    "f2",          "cycle upscaling (classic renderer)" },
	{ KM_SCALER,     "scaler",     "f3",          "toggle Scale2x/3x smoothing (classic renderer)" },
	{ KM_FILTER,     "filter",     "f",           "toggle bilinear filtering / Omniscale (HD)" },
	{ KM_INTEGER,    "integer",    "i",           "toggle pixel-perfect integer scaling" },
	{ KM_QUIT,       "quit",       "escape",      "leave the game" }
};

#define KEYMAP_ENTRY_COUNT ((int)(sizeof(keymap_entries) / sizeof(keymap_entries[0])))

static keymap_binding_t keymap_bindings[KM_COUNT][KM_MAX_BINDINGS];
static int keymap_binding_count[KM_COUNT];

static const char *keymap_header =
	"# Heart of The Alien Redux - key bindings\n"
	"#\n"
	"# Every non-comment line has the form:\n"
	"#\n"
	"#     action = key[, key ...]\n"
	"#\n"
	"# Names are case insensitive. Supported key names:\n"
	"#\n"
	"#   letters/numbers   a ... z, 0 ... 9\n"
	"#   function keys     f1 ... f24\n"
	"#   arrows            left, right, up, down\n"
	"#   other             space, enter, escape, tab, backspace, insert,\n"
	"#                     delete, home, end, pageup, pagedown\n"
	"#   punctuation       - = [ ] \\ ; ' , . / `\n"
	"#   keypad            kp0 ... kp9, kpenter, kpplus, kpminus,\n"
	"#                     kpmultiply, kpdivide, kpdecimal, kpperiod\n"
	"#\n"
	"# Modifiers are written in front of the key with a '+':\n"
	"#\n"
	"#   ctrl+...   shift+...   alt+...   gui+...\n"
	"#\n"
	"# For example:  fullscreen = alt+enter\n"
	"#\n"
	"# Several keys may be listed for one action, separated by commas.\n"
	"# Leave the right side empty (or delete the line) to unbind it.\n"
	"# Unknown key names are reported on the console and ignored, so a\n"
	"# typo never prevents the game from starting.\n"
	"#\n"
	"# This file is created with the defaults on first start. Delete it\n"
	"# to get the defaults back.\n"
	"\n";

static const char *keymap_action_name_impl(keymap_action action)
{
	int i;

	for (i = 0; i < KEYMAP_ENTRY_COUNT; i++)
	{
		if (keymap_entries[i].action == action)
		{
			return keymap_entries[i].name;
		}
	}

	return "unknown";
}

const char *keymap_action_name(keymap_action action)
{
	return keymap_action_name_impl(action);
}

static void lowercase(char *text)
{
	for (; *text != '\0'; text++)
	{
		*text = (char)tolower((unsigned char)*text);
	}
}

static char *trim(char *text)
{
	char *end;

	while (*text != '\0' && isspace((unsigned char)*text))
	{
		text++;
	}

	end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
	{
		end--;
	}
	*end = '\0';

	return text;
}

/** Maps a few friendly names onto the SDL key names. */
static const char *resolve_alias(const char *name)
{
	static const struct
	{
		const char *alias;
		const char *real;
	} aliases[] =
	{
		{ "esc", "escape" },
		{ "enter", "return" },
		{ "del", "delete" },
		{ "ins", "insert" },
		{ "pgup", "pageup" },
		{ "pgdn", "pagedown" },
		{ "shift", "lshift" },
		{ "ctrl", "lctrl" },
		{ "control", "lctrl" },
		{ "alt", "lalt" },
		{ "spacebar", "space" },
		/* SDL3 calls the keypad keys "Keypad 0", "Keypad Enter", ... */
		{ "kpenter", "keypad enter" },
		{ "kp0", "keypad 0" },
		{ "kp1", "keypad 1" },
		{ "kp2", "keypad 2" },
		{ "kp3", "keypad 3" },
		{ "kp4", "keypad 4" },
		{ "kp5", "keypad 5" },
		{ "kp6", "keypad 6" },
		{ "kp7", "keypad 7" },
		{ "kp8", "keypad 8" },
		{ "kp9", "keypad 9" },
		{ "kpplus", "keypad +" },
		{ "kpminus", "keypad -" },
		{ "kpmultiply", "keypad *" },
		{ "kpdivide", "keypad /" },
		{ "kpdecimal", "keypad decimal" },
		{ "kpperiod", "keypad ." }
	};
	int i;

	for (i = 0; i < (int)(sizeof(aliases) / sizeof(aliases[0])); i++)
	{
		if (strcmp(name, aliases[i].alias) == 0)
		{
			return aliases[i].real;
		}
	}

	return name;
}

static SDL_Keymod modifier_from_name(const char *name)
{
	if (strcmp(name, "ctrl") == 0 || strcmp(name, "control") == 0)
	{
		return SDL_KMOD_CTRL;
	}
	if (strcmp(name, "shift") == 0)
	{
		return SDL_KMOD_SHIFT;
	}
	if (strcmp(name, "alt") == 0)
	{
		return SDL_KMOD_ALT;
	}
	if (strcmp(name, "gui") == 0 || strcmp(name, "win") == 0 ||
		strcmp(name, "meta") == 0 || strcmp(name, "cmd") == 0)
	{
		return SDL_KMOD_GUI;
	}

	return SDL_KMOD_NONE;
}

/** Parses one key description such as "f12" or "ctrl+shift+s". */
static int parse_binding(char *text, keymap_binding_t *binding)
{
	char *token;
	char *next;
	SDL_Keycode key = SDLK_UNKNOWN;
	SDL_Keymod mod = SDL_KMOD_NONE;

	lowercase(text);

	token = text;
	while (token != NULL && *token != '\0')
	{
		next = strchr(token, '+');
		if (next != NULL)
		{
			*next = '\0';
			next++;
		}

		token = trim(token);
		if (*token != '\0' && next != NULL)
		{
			SDL_Keymod value = modifier_from_name(token);

			if (value == SDL_KMOD_NONE)
			{
				/* a '+' inside a key name is not supported */
				return -1;
			}

			mod |= value;
		}
		else if (*token != '\0')
		{
			key = SDL_GetKeyFromName(resolve_alias(token));
			if (key == SDLK_UNKNOWN)
			{
				fprintf(stderr, "keys: unknown key name '%s'\n", token);
				return -1;
			}
		}

		token = next;
	}

	if (key == SDLK_UNKNOWN)
	{
		return -1;
	}

	binding->key = key;
	binding->mod = mod;
	return 0;
}

/** Parses a comma separated key list and stores it for an action.

    A non-empty list where every key name is invalid keeps the bindings
    that were already in effect (normally the defaults), so one typo in
    keys.txt does not make an action impossible to use. An empty list is
    an explicit request to unbind the action.
*/
static void parse_binding_list(char *value, keymap_action action)
{
	keymap_binding_t parsed[KM_MAX_BINDINGS];
	int parsed_count = 0;
	int had_text = 0;
	char *item;
	char *save = NULL;

	item = strtok_r(value, ",", &save);
	while (item != NULL)
	{
		keymap_binding_t binding;

		item = trim(item);
		if (*item != '\0')
		{
			had_text = 1;
		}

		if (*item != '\0' && parsed_count < KM_MAX_BINDINGS)
		{
			if (parse_binding(item, &binding) == 0)
			{
				parsed[parsed_count] = binding;
				parsed_count++;
			}
		}

		item = strtok_r(NULL, ",", &save);
	}

	if (parsed_count == 0 && had_text)
	{
		/* all names were invalid: keep the current bindings */
		return;
	}

	keymap_binding_count[action] = parsed_count;
	for (int i = 0; i < parsed_count; i++)
	{
		keymap_bindings[action][i] = parsed[i];
	}
}

static void apply_defaults(void)
{
	int i;
	char buffer[256];

	for (i = 0; i < KEYMAP_ENTRY_COUNT; i++)
	{
		SDL_strlcpy(buffer, keymap_entries[i].def, sizeof(buffer));
		parse_binding_list(buffer, keymap_entries[i].action);
	}
}

static int write_default_file(const char *filename)
{
	FILE *fp;
	int i;

	fp = hota_fopen(filename, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "keys: could not write '%s' (using defaults)\n", filename);
		return -1;
	}

	fputs(keymap_header, fp);
	for (i = 0; i < KEYMAP_ENTRY_COUNT; i++)
	{
		fprintf(fp, "# %s\n%s = %s\n\n", keymap_entries[i].doc,
			keymap_entries[i].name, keymap_entries[i].def);
	}

	if (ferror(fp))
	{
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

int keymap_init(const char *filename)
{
	FILE *fp;
	char line[KM_LINE_SIZE];
	int lineno = 0;

	apply_defaults();

	if (filename == NULL || *filename == '\0')
	{
		return 0;
	}

	fp = hota_fopen(filename, "r");
	if (fp == NULL)
	{
		/* first run: leave a documented default file behind */
		write_default_file(filename);
		return 0;
	}

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		char *text;
		char *equals;
		int i;
		int matched = 0;

		lineno++;
		text = trim(line);
		if (*text == '\0' || *text == '#' || *text == ';')
		{
			continue;
		}

		equals = strchr(text, '=');
		if (equals == NULL)
		{
			fprintf(stderr, "keys:%d: expected 'action = key'\n", lineno);
			continue;
		}

		*equals = '\0';
		text = trim(text);
		lowercase(text);

		for (i = 0; i < KEYMAP_ENTRY_COUNT; i++)
		{
			if (strcmp(text, keymap_entries[i].name) == 0)
			{
				parse_binding_list(trim(equals + 1),
					keymap_entries[i].action);
				matched = 1;
				break;
			}
		}

		if (!matched)
		{
			fprintf(stderr, "keys:%d: unknown action '%s'\n", lineno, text);
		}
	}

	fclose(fp);

	/* warnings must reach the console even when the game is killed
	 * before it writes anything else to stderr */
	fflush(stderr);
	return 0;
}

int keymap_matches_key(keymap_action action, SDL_Keycode key)
{
	int i;
	int count;

	if (action < 0 || action >= KM_COUNT)
	{
		return 0;
	}

	count = keymap_binding_count[action];
	for (i = 0; i < count; i++)
	{
		if (keymap_bindings[action][i].key == key)
		{
			return 1;
		}
	}

	return 0;
}

int keymap_matches(keymap_action action, SDL_Keycode key, SDL_Keymod mod)
{
	int i;
	int count;

	if (action < 0 || action >= KM_COUNT)
	{
		return 0;
	}

	count = keymap_binding_count[action];
	for (i = 0; i < count; i++)
	{
		if (keymap_bindings[action][i].key != key)
		{
			continue;
		}

		if ((mod & keymap_bindings[action][i].mod) ==
			keymap_bindings[action][i].mod)
		{
			return 1;
		}
	}

	return 0;
}
