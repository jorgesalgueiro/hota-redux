/*
 * Key binding unit test for the Redux keymap module.
 *
 *   keymap_probe [file]
 *
 * Without a file the built-in defaults are checked. With a file the
 * bindings are loaded from it and a fixed set of expectations is used
 * (see tests/keys-sample.txt).
 */
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "../src/keymap.h"

static int failures = 0;

static void expect(const char *what, int got, int want)
{
	if (got != want)
	{
		printf("FAIL %-42s got %d want %d\n", what, got, want);
		failures++;
	}
	else
	{
		printf("ok   %-42s %d\n", what, got);
	}
}

int main(int argc, char **argv)
{
	int custom = argc > 1;

	SDL_Init(SDL_INIT_VIDEO);
	keymap_init(custom ? argv[1] : NULL);

	if (!custom)
	{
		expect("f12 = screenshot", keymap_matches(KM_SCREENSHOT, SDLK_F12, 0), 1);
		expect("escape = quit", keymap_matches(KM_QUIT, SDLK_ESCAPE, 0), 1);
		expect("z = action a", keymap_matches(KM_A, SDLK_Z, 0), 1);
		expect("a = action a", keymap_matches(KM_A, SDLK_A, 0), 1);
		expect("s = action b", keymap_matches(KM_B, SDLK_S, 0), 1);
		expect("f5 = quicksave", keymap_matches(KM_QUICKSAVE, SDLK_F5, 0), 1);
		expect("f7 = quickload", keymap_matches(KM_QUICKLOAD, SDLK_F7, 0), 1);
		expect("space = speed", keymap_matches(KM_SPEED, SDLK_SPACE, 0), 1);
		expect("q = reset", keymap_matches(KM_RESET, SDLK_Q, 0), 1);
		expect("alt+return = fullscreen",
			keymap_matches(KM_FULLSCREEN, SDLK_RETURN, SDL_KMOD_ALT), 1);
		expect("return alone != fullscreen",
			keymap_matches(KM_FULLSCREEN, SDLK_RETURN, 0), 0);
		expect("left = move left", keymap_matches(KM_LEFT, SDLK_LEFT, 0), 1);
	}
	else
	{
		/* tests/keys-sample.txt */
		expect("f9 = screenshot", keymap_matches(KM_SCREENSHOT, SDLK_F9, 0), 1);
		expect("f12 != screenshot", keymap_matches(KM_SCREENSHOT, SDLK_F12, 0), 0);
		expect("f10 = quit", keymap_matches(KM_QUIT, SDLK_F10, 0), 1);
		expect("escape != quit", keymap_matches(KM_QUIT, SDLK_ESCAPE, 0), 0);
		expect("ctrl+s = quicksave",
			keymap_matches(KM_QUICKSAVE, SDLK_S, SDL_KMOD_CTRL), 1);
		expect("s alone != quicksave", keymap_matches(KM_QUICKSAVE, SDLK_S, 0), 0);
		expect("enter = move up", keymap_matches(KM_UP, SDLK_RETURN, 0), 1);
		expect("up != move up", keymap_matches(KM_UP, SDLK_UP, 0), 0);
		expect("no move down binding", keymap_matches(KM_DOWN, SDLK_DOWN, 0), 0);
	}

	printf("%d failure(s)\n", failures);
	SDL_Quit();
	return failures == 0 ? 0 : 1;
}
