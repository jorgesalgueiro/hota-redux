/*
 * Heart of The Alien: Renderer
 * Copyright (c) 2004 Gil Megidish
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
#include <string.h>
#include <SDL3/SDL.h>

#include "debug.h"
#include "render.h"
#include "game2bin.h"
#include "client.h"
#include "upscale.h"
#include "gl_render.h"
#include "platform.h"

static int fullscreen = 0;
static int scroll_reg = 0;
extern int fullscreen_flag;
extern int integer_scale_flag;
extern int test_flag;

#define VIDEO_CONFIG_FILENAME "video.cfg"

static SDL_Palette *palette;
static SDL_Color palette_colors[16];
static SDL_Surface *surface;
static SDL_Texture *texture;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_GLContext gl_context;
static int use_gl = 0;

/* internal pixel-art upscaling (1 = native 304x192) */
static int upscale_factor = 1;
static SDL_Surface *upscale_surface;
static SDL_Surface *upscale_temp;

static int palette_changed = 0;
static int current_palette = 0;

#define GAME_WIDTH  304
#define GAME_HEIGHT 192
#define MAX_UPSCALE 4

/** Reads the remembered window size and integer-scaling flag. */
static void load_video_config(int *width, int *height, int *integer_scale)
{
	FILE *fp;
	char line[128];

	*width = 0;
	*height = 0;
	*integer_scale = 0;

	fp = hota_fopen(VIDEO_CONFIG_FILENAME, "r");
	if (fp == NULL)
	{
		return;
	}

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		int value;

		if (sscanf(line, "width=%d", &value) == 1)
		{
			*width = value;
		}
		else if (sscanf(line, "height=%d", &value) == 1)
		{
			*height = value;
		}
		else if (sscanf(line, "integer=%d", &value) == 1)
		{
			*integer_scale = value ? 1 : 0;
		}
	}

	fclose(fp);

	if (*width < 304 || *height < 192 ||
		*width > 16384 || *height > 16384)
	{
		*width = 0;
		*height = 0;
	}
}

/** Remembers the current windowed size and integer-scaling flag. */
static void save_video_config()
{
	int width = 0;
	int height = 0;
	FILE *fp;

	if (window == NULL || fullscreen || test_flag != 0)
	{
		return;
	}

	if (!SDL_GetWindowSize(window, &width, &height) ||
		width < 304 || height < 192)
	{
		return;
	}

	fp = hota_fopen(VIDEO_CONFIG_FILENAME, "w");
	if (fp == NULL)
	{
		return;
	}

	fprintf(fp, "width=%d\n", width);
	fprintf(fp, "height=%d\n", height);
	fprintf(fp, "integer=%d\n", cls.integer_scale ? 1 : 0);
	fclose(fp);
}

/** Returns the current palette used
    @returns palette

    Palettes are stored in game2.bin
*/
int get_current_palette()
{
	return current_palette;
}

/** Sets VSCROLL value
    @param scroll
*/
void set_scroll(int scroll)
{
	scroll_reg = scroll;
}

/** Returns VSCROLL register value
    @returns vscroll
*/
int get_scroll_register()
{
	return scroll_reg;
}

/** Returns the largest integer window scale that fits comfortably on the
    primary display, clamped to a sane range.
*/
static int get_auto_scale()
{
	SDL_Rect bounds;
	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	int sx, sy, scale;

	if (display != 0 && SDL_GetDisplayUsableBounds(display, &bounds))
	{
		sx = (bounds.w * 9 / 10) / GAME_WIDTH;
		sy = (bounds.h * 9 / 10) / GAME_HEIGHT;
		scale = (sx < sy) ? sx : sy;

		if (scale < 2) scale = 2;
		if (scale > MAX_UPSCALE) scale = MAX_UPSCALE;
		return scale;
	}

	return 2;
}

static void apply_scale_mode()
{
	if (texture == NULL)
	{
		return;
	}

	if (cls.filtered)
	{
		SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
	}
	else
	{
		SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
	}
}

/** Destroys the current output texture and upscaling buffers.
*/
static void destroy_upscale_objects()
{
	if (texture != NULL)
	{
		SDL_DestroyTexture(texture);
		texture = NULL;
	}

	if (upscale_surface != NULL)
	{
		SDL_DestroySurface(upscale_surface);
		upscale_surface = NULL;
	}

	if (upscale_temp != NULL)
	{
		SDL_DestroySurface(upscale_temp);
		upscale_temp = NULL;
	}
}

/** Selects the internal pixel-art upscaling factor (1..4).
    @returns zero on success
*/
int render_set_upscale(int factor)
{
	int width, height;

	if (cls.integer_scale)
	{
		/* Integer presentation is based on the native 304x192 image,
		 * so the internal upscaler would only shift the logical size
		 * and make the integer factor smaller. */
		factor = 1;
	}

	if (factor < 1) factor = 1;
	if (factor > MAX_UPSCALE) factor = MAX_UPSCALE;

	if (use_gl)
	{
		/* HD mode resolves the image in the shader; the factor only
		 * controls the legacy SDL fallback. */
		upscale_factor = factor;
		return 0;
	}

	destroy_upscale_objects();
	upscale_factor = factor;

	width = GAME_WIDTH * factor;
	height = GAME_HEIGHT * factor;

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
		SDL_TEXTUREACCESS_STREAMING, width, height);
	if (texture == NULL)
	{
		LOG(("failed to create %dx%d texture: %s\n", width, height, SDL_GetError()));

		/* fall back to the native resolution */
		upscale_factor = 1;
		width = GAME_WIDTH;
		height = GAME_HEIGHT;
		texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
			SDL_TEXTUREACCESS_STREAMING, width, height);
		if (texture == NULL)
		{
			return -1;
		}
	}

	if (upscale_factor > 1)
	{
		upscale_surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_INDEX8);
		if (upscale_surface == NULL)
		{
			destroy_upscale_objects();
			return render_set_upscale(1);
		}
		SDL_SetSurfacePalette(upscale_surface, palette);
	}

	if (upscale_factor == 4)
	{
		upscale_temp = SDL_CreateSurface(GAME_WIDTH * 2, GAME_HEIGHT * 2,
			SDL_PIXELFORMAT_INDEX8);
		if (upscale_temp == NULL)
		{
			destroy_upscale_objects();
			return render_set_upscale(1);
		}
	}

	SDL_SetTexturePalette(texture, palette);
	apply_scale_mode();
	SDL_SetRenderLogicalPresentation(renderer, width, height,
		cls.integer_scale ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE :
		SDL_LOGICAL_PRESENTATION_LETTERBOX);

	LOG(("using %dx internal upscaling (%dx%d output texture)\n",
		upscale_factor, width, height));
	return 0;
}

/** Returns the active internal upscaling factor.
*/
int render_get_upscale()
{
	return upscale_factor;
}

/** Cycles through 1x, 2x, 3x and 4x upscaling at runtime.
*/
void toggle_upscale()
{
	int factor;

	if (use_gl)
	{
		/* HD mode always resolves to the window resolution */
		LOG(("HD mode: upscale is handled by the shader\n"));
		return;
	}

	factor = upscale_factor + 1;

	if (factor > MAX_UPSCALE)
	{
		factor = 1;
	}

	if (render_set_upscale(factor) == 0)
	{
		cls.upscale = factor;
	}
}

/** Toggles between sharp nearest-neighbor scaling (default) and the
    smoother Scale2x/Scale3x pixel-art filters.
*/
void toggle_scaler()
{
	if (use_gl)
	{
		/* HD mode: nearest shader <-> Omniscale */
		cls.hd_sharp = !cls.hd_sharp;
		gl_render_set_filter(!cls.hd_sharp);
		LOG(("HD filter: %s\n", cls.hd_sharp ? "nearest" : "omniscale"));
		return;
	}

	cls.smooth = !cls.smooth;
}

/** Toggles pixel-perfect integer scaling. */
void toggle_integer_scale()
{
	cls.integer_scale = !cls.integer_scale;
	LOG(("integer scaling: %s\n", cls.integer_scale ? "on" : "off"));

	if (use_gl)
	{
		/* the shader picks this up on the next frame */
		return;
	}

	render_set_upscale(cls.integer_scale ? 1 :
		(cls.upscale > 0 ? cls.upscale : cls.scale));
}

/** Toggles smooth (bilinear) hardware filtering.
    In HD mode this selects the high-quality Omniscale filter.
*/
void toggle_filter()
{
	if (use_gl)
	{
		/* bilinear does not exist in HD mode, reuse the key for the
		 * nearest/Omniscale switch */
		toggle_scaler();
		return;
	}

	cls.filtered = !cls.filtered;
	apply_scale_mode();
}

/** Renders a virtual screen
    @param src
*/
void render(unsigned char *src)
{
	int y, p;

	if (src == NULL || surface == NULL)
	{
		return;
	}

	/* keep the scroll within the framebuffer bounds */
	if (scroll_reg > 192)
	{
		scroll_reg = 192;
	}
	else if (scroll_reg < -192)
	{
		scroll_reg = -192;
	}

	if (palette_changed)
	{
		palette_changed = 0;
		SDL_SetPaletteColors(palette, palette_colors, 0, 16);
		if (texture != NULL)
		{
			SDL_SetTexturePalette(texture, palette);
		}
	}

	if (SDL_MUSTLOCK(surface)) SDL_LockSurface(surface);

	if (scroll_reg == 0)
	{
		/* no scroll */
		for (y=0; y<192; y++)
		{
			memcpy((char *)surface->pixels + y*surface->pitch, src + 304*y, 304);
		}
	}
	else if (scroll_reg < 0)
	{
		/* scroll from bottom */
		p = - scroll_reg;
		for (y=p; y<192; y++)
		{
			memcpy((char *)surface->pixels + (y-p)*surface->pitch, src + 304*y, 304);
		}

		for (y=192; y<192+p; y++)
		{
			memcpy((char *)surface->pixels + (y-p)*surface->pitch, src + 304*191, 304);
		}
	}
	else 
	{
		/* scroll from top */
		p = scroll_reg;
		for (y=0; y<p; y++)
		{
			memcpy((char *)surface->pixels + y*surface->pitch, src, 304);
		}

		for (y=p; y<192; y++)
		{
			memcpy((char *)surface->pixels + y*surface->pitch, src + 304*(y-p), 304);
		}
	}

	if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

	scroll_reg = 0;

	if (use_gl)
	{
		gl_render_frame((const unsigned char *)surface->pixels, surface->pitch,
			palette_colors, GAME_WIDTH, GAME_HEIGHT);
		return;
	}

	if (upscale_factor == 1)
	{
		SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
	}
	else
	{
		unsigned char *dst;
		int dst_pitch;

		if (SDL_MUSTLOCK(upscale_surface)) SDL_LockSurface(upscale_surface);
		dst = (unsigned char *)upscale_surface->pixels;
		dst_pitch = upscale_surface->pitch;

		if (upscale_factor == 2)
		{
			if (cls.smooth)
			{
				upscale2x(dst, dst_pitch, (const unsigned char *)surface->pixels,
					surface->pitch, GAME_WIDTH, GAME_HEIGHT);
			}
			else
			{
				upscale_nearest(dst, dst_pitch,
					(const unsigned char *)surface->pixels, surface->pitch,
					GAME_WIDTH, GAME_HEIGHT, 2);
			}
		}
		else if (upscale_factor == 3)
		{
			if (cls.smooth)
			{
				upscale3x(dst, dst_pitch, (const unsigned char *)surface->pixels,
					surface->pitch, GAME_WIDTH, GAME_HEIGHT);
			}
			else
			{
				upscale_nearest(dst, dst_pitch,
					(const unsigned char *)surface->pixels, surface->pitch,
					GAME_WIDTH, GAME_HEIGHT, 3);
			}
		}
		else /* 4x */
		{
			if (cls.smooth)
			{
				unsigned char *tmp;
				int tmp_pitch;

				if (SDL_MUSTLOCK(upscale_temp)) SDL_LockSurface(upscale_temp);
				tmp = (unsigned char *)upscale_temp->pixels;
				tmp_pitch = upscale_temp->pitch;

				upscale2x(tmp, tmp_pitch, (const unsigned char *)surface->pixels,
					surface->pitch, GAME_WIDTH, GAME_HEIGHT);
				upscale2x(dst, dst_pitch, tmp, tmp_pitch,
					GAME_WIDTH * 2, GAME_HEIGHT * 2);

				if (SDL_MUSTLOCK(upscale_temp)) SDL_UnlockSurface(upscale_temp);
			}
			else
			{
				upscale_nearest(dst, dst_pitch,
					(const unsigned char *)surface->pixels, surface->pitch,
					GAME_WIDTH, GAME_HEIGHT, 4);
			}
		}

		if (SDL_MUSTLOCK(upscale_surface)) SDL_UnlockSurface(upscale_surface);

		SDL_UpdateTexture(texture, NULL, upscale_surface->pixels,
			upscale_surface->pitch);
	}

	SDL_RenderClear(renderer);
	SDL_RenderTexture(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

/** Shows a black frame.

    Used while a room is being loaded, so the last frame of the previous
    room is not left on screen. Without this the password/continue screen
    from the previous room can flash for a moment when the next room
    starts (for example when entering the first level).
*/
void render_blank()
{
	if (use_gl)
	{
		gl_render_blank();
		return;
	}

	if (renderer == NULL)
	{
		return;
	}

	/* Clear the actual window; the palette index zero is not guaranteed
	 * to be black, so do not reuse the indexed framebuffer here. */
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
}

/** Module initializer
    @returns zero on success
*/
int render_init()
{
	return 0;
}

/** Converts a Sega CD RGB444 to RGB888 
    @param rgb12   pointer to 16x2 of palette data
*/
void set_palette_rgb12(unsigned char *rgb12)
{
	int i;

	for (i=0; i<16; i++)
	{
		int c, r, g, b;

		c = (rgb12[i*2] << 8) | rgb12[i*2+1];
		r = (c & 0xf) << 4;
		g = ((c >> 4) & 0xf) << 4;
		b = ((c >> 8) & 0xf) << 4;

		palette_colors[i].r = r | (r >> 4);
		palette_colors[i].g = g | (g >> 4);
		palette_colors[i].b = b | (b >> 4);
		palette_colors[i].a = 0xFF;
	}

	palette_changed = 1;
}

void set_palette(int which)
{
	unsigned char rgb12[16*2] = {0};
	int copied;

	copied = copy_from_game2bin(rgb12, 0x5cb8 + (which * 16 * 2), sizeof(rgb12));
	if (copied != (int)sizeof(rgb12))
	{
		LOG(("set_palette: palette %d is out of range\n", which));
		return;
	}

	current_palette = which;
	set_palette_rgb12(rgb12);

	//palette_colors[255].r = 255;
	//palette_colors[255].g = 0;
	//palette_colors[255].b = 255;
}

void toggle_fullscreen()
{
	/* Remember the windowed size before it is replaced by the screen
	 * dimensions. */
	if (fullscreen == 0)
	{
		save_video_config();
	}

	fullscreen = 1 ^ fullscreen;

	SDL_SetWindowFullscreen(window, fullscreen);

	if (fullscreen == 0)
	{
		SDL_ShowCursor();
	}
	else
	{
		LOG(("setting fullscreen mode\n"));
		SDL_HideCursor();
	}
}

int render_create_surface()
{
	int window_scale, upscale;
	int window_width, window_height;
	int cfg_width = 0, cfg_height = 0, cfg_integer = 0;

	surface = SDL_CreateSurface(GAME_WIDTH, GAME_HEIGHT, SDL_PIXELFORMAT_INDEX8);
	if (surface == NULL)
	{
		return -1;
	}

	palette = SDL_CreatePalette(16);
	if (palette == NULL)
	{
		return -2;
	}
	SDL_SetSurfacePalette(surface, palette);

	load_video_config(&cfg_width, &cfg_height, &cfg_integer);

	if (integer_scale_flag >= 0)
	{
		cls.integer_scale = integer_scale_flag;
	}
	else
	{
		cls.integer_scale = cfg_integer;
	}

	if (cls.scale > 0)
	{
		window_scale = cls.scale;
		window_width = GAME_WIDTH * window_scale;
		window_height = GAME_HEIGHT * window_scale;
	}
	else if (cfg_width > 0 && cfg_height > 0)
	{
		SDL_Rect bounds;
		SDL_DisplayID display = SDL_GetPrimaryDisplay();

		window_width = cfg_width;
		window_height = cfg_height;

		/* Do not restore a window larger than the current display
		 * (for example after moving to a smaller screen). */
		if (display != 0 && SDL_GetDisplayUsableBounds(display, &bounds))
		{
			if (window_width > bounds.w) window_width = bounds.w;
			if (window_height > bounds.h) window_height = bounds.h;
			if (window_width < GAME_WIDTH) window_width = GAME_WIDTH;
			if (window_height < GAME_HEIGHT) window_height = GAME_HEIGHT;
		}

		window_scale = window_width / GAME_WIDTH;
		if (window_scale < 1) window_scale = 1;
	}
	else
	{
		window_scale = get_auto_scale();
		window_width = GAME_WIDTH * window_scale;
		window_height = GAME_HEIGHT * window_scale;
	}

	cls.scale = window_scale;
	LOG(("window %dx%d (scale %d, integer %d)\n",
		window_width, window_height, window_scale, cls.integer_scale));

	if (cls.hd)
	{
		#ifdef HAVE_OPENGL
		/* request a compatibility context so the simple vertex
		 * attribute path and GLSL 1.30 keep working */
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
			SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

		window = SDL_CreateWindow("Heart of The Alien Redux",
			window_width, window_height,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

		if (window != NULL)
		{
			gl_context = SDL_GL_CreateContext(window);

			if (gl_context != NULL
				&& SDL_GL_MakeCurrent(window, gl_context)
				&& gl_render_init(window) == 0)
			{
				use_gl = 1;
				LOG(("using HD renderer (OpenGL)\n"));
			}
			else
			{
				LOG(("HD renderer unavailable (%s), falling back\n",
					SDL_GetError()));

				if (gl_context != NULL)
				{
					SDL_GL_DestroyContext(gl_context);
					gl_context = NULL;
				}

				SDL_DestroyWindow(window);
				window = NULL;
			}
		}
		else
		{
			LOG(("no OpenGL window (%s), falling back\n", SDL_GetError()));
		}
		#endif /* HAVE_OPENGL */
	}

	if (!use_gl)
	{
		window = SDL_CreateWindow("Heart of The Alien Redux",
			window_width, window_height, SDL_WINDOW_RESIZABLE);
		if (window == NULL)
		{
			return -3;
		}

		renderer = SDL_CreateRenderer(window, NULL);
		if (renderer == NULL)
		{
			return -4;
		}
	}

	upscale = cls.upscale;
	if (upscale <= 0)
	{
		upscale = window_scale;
		if (upscale > MAX_UPSCALE)
		{
			upscale = MAX_UPSCALE;
		}
	}

	if (render_set_upscale(upscale) < 0)
	{
		return -5;
	}

	if (use_gl)
	{
		cls.hd_sharp = cls.hd_sharp ? 1 : 0;
		gl_render_set_filter(!cls.hd_sharp);
		gl_render_set_strength((float)cls.hd_smooth / 100.0f);
	}

	if (fullscreen_flag)
	{
		toggle_fullscreen();
	}

	return 0;
}

/** Releases every video resource; safe to call more than once.
*/
void render_done()
{
	save_video_config();

	if (use_gl)
	{
		gl_render_done();

		if (gl_context != NULL)
		{
			SDL_GL_DestroyContext(gl_context);
			gl_context = NULL;
		}

		use_gl = 0;
	}

	destroy_upscale_objects();

	if (renderer != NULL)
	{
		SDL_DestroyRenderer(renderer);
		renderer = NULL;
	}

	if (window != NULL)
	{
		SDL_DestroyWindow(window);
		window = NULL;
	}

	if (palette != NULL)
	{
		SDL_DestroyPalette(palette);
		palette = NULL;
	}

	if (surface != NULL)
	{
		SDL_DestroySurface(surface);
		surface = NULL;
	}
}

void screenshot()
{
	SDL_Surface *shot = surface;

	if (use_gl)
	{
		gl_render_screenshot("hota-screenshot.png");
		return;
	}

	if (upscale_factor > 1 && upscale_surface != NULL)
	{
		shot = upscale_surface;
	}

	if (!SDL_SavePNG(shot, "hota-screenshot.png"))
	{
		fprintf(stderr, "failed to save screenshot: %s\n", SDL_GetError());
	}
}
