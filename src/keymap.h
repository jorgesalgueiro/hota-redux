/*
 * Heart of The Alien Redux: user configurable key bindings
 *
 * Bindings are loaded from a plain text file (keys.txt by default) so
 * players can remap every key without rebuilding. See keymap.c for the
 * file format, which is also written as a comment header in the file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef KEYMAP_H
#define KEYMAP_H

#include <SDL3/SDL.h>

typedef enum
{
	KM_UP = 0,
	KM_DOWN,
	KM_LEFT,
	KM_RIGHT,
	KM_A,
	KM_B,
	KM_C,
	KM_QUICKSAVE,
	KM_QUICKLOAD,
	KM_SCREENSHOT,
	KM_FULLSCREEN,
	KM_SPEED,
	KM_RESET,
	KM_UPSCALE,
	KM_SCALER,
	KM_FILTER,
	KM_QUIT,
	KM_COUNT
} keymap_action;

/** Loads bindings from filename.
    A commented default file is written when it does not exist yet.
    @returns zero on success (defaults are kept on any error)
*/
int keymap_init(const char *filename);

/** Checks whether a key event matches an action's binding. */
int keymap_matches(keymap_action action, SDL_Keycode key, SDL_Keymod mod);

/** Human readable name of an action ("move up", "screenshot", ...). */
const char *keymap_action_name(keymap_action action);

#endif /* KEYMAP_H */
