/*
 * Heart of The Alien Redux: SDL OpenGL HD renderer
 *
 * Renders the 304x192 indexed framebuffer at the native window resolution
 * using either nearest-neighbor or the Omniscale pixel-art filter.
 */

#ifndef GL_RENDER_H
#define GL_RENDER_H

#include <SDL3/SDL.h>

/** Initializes the OpenGL context state and shaders.
    @returns zero on success
*/
int gl_render_init(SDL_Window *window);

/** Shuts down the OpenGL renderer. */
void gl_render_done(void);

/** Selects the high-quality Omniscale filter (1) or sharp nearest (0). */
void gl_render_set_filter(int enabled);

/** Sets how strongly Omniscale smooths diagonals (0.0 = nearest,
    1.0 = full reconstruction). */
void gl_render_set_strength(float strength);

/** Uploads and draws one frame.
    @param indices 8-bit palette indices (top-down)
    @param pitch   bytes per source row
    @param palette 16-entry palette
*/
void gl_render_frame(const unsigned char *indices, int pitch,
	const SDL_Color *palette, int width, int height);

/** Requests a high-resolution screenshot on the next presented frame. */
void gl_render_screenshot(const char *filename);

/** Presents a black frame (used while a room is loading). */
void gl_render_blank(void);

#endif // GL_RENDER_H
