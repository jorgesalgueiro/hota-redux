/*
 * Heart of The Alien Redux: SDL OpenGL HD renderer
 *
 * Uses SDL3 for the window/context and OpenGL for a full-resolution
 * pixel-art upscale. The Omniscale shader is MIT licensed; see
 * omniscale_shader.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "gl_render.h"
#include "omniscale_shader.h"

#ifndef HAVE_OPENGL

/* Built without OpenGL support: the HD renderer reports itself as
 * unavailable and the SDL renderer is used instead. */

int gl_render_init(SDL_Window *window)
{
	(void)window;
	return -1;
}

void gl_render_done(void)
{
}

void gl_render_set_filter(int enabled)
{
	(void)enabled;
}

void gl_render_set_strength(float strength)
{
	(void)strength;
}

void gl_render_screenshot(const char *filename)
{
	(void)filename;
}

void gl_render_frame(const unsigned char *indices, int pitch,
	const SDL_Color *palette, int width, int height)
{
	(void)indices;
	(void)pitch;
	(void)palette;
	(void)width;
	(void)height;
}

#else /* HAVE_OPENGL */

#ifndef APIENTRY
#define APIENTRY
#endif

static PFNGLCREATESHADERPROC p_glCreateShader;
static PFNGLSHADERSOURCEPROC p_glShaderSource;
static PFNGLCOMPILESHADERPROC p_glCompileShader;
static PFNGLGETSHADERIVPROC p_glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC p_glGetShaderInfoLog;
static PFNGLDELETESHADERPROC p_glDeleteShader;
static PFNGLCREATEPROGRAMPROC p_glCreateProgram;
static PFNGLATTACHSHADERPROC p_glAttachShader;
static PFNGLLINKPROGRAMPROC p_glLinkProgram;
static PFNGLGETPROGRAMIVPROC p_glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC p_glGetProgramInfoLog;
static PFNGLDELETEPROGRAMPROC p_glDeleteProgram;
static PFNGLUSEPROGRAMPROC p_glUseProgram;
static PFNGLGETUNIFORMLOCATIONPROC p_glGetUniformLocation;
static PFNGLUNIFORM1IPROC p_glUniform1i;
static PFNGLUNIFORM1FPROC p_glUniform1f;
static PFNGLUNIFORM2FPROC p_glUniform2f;
static PFNGLBINDATTRIBLOCATIONPROC p_glBindAttribLocation;
static PFNGLVERTEXATTRIBPOINTERPROC p_glVertexAttribPointer;
static PFNGLENABLEVERTEXATTRIBARRAYPROC p_glEnableVertexAttribArray;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC p_glDisableVertexAttribArray;
static PFNGLGENFRAMEBUFFERSPROC p_glGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC p_glBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC p_glFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC p_glCheckFramebufferStatus;
static PFNGLDELETEFRAMEBUFFERSPROC p_glDeleteFramebuffers;

static SDL_Window *gl_window;
static SDL_GLContext gl_context;
static GLuint gl_texture;
static GLuint program_nearest;
static GLuint program_omniscale;
static int use_omniscale;
static float smooth_strength = 1.0f;
static int screenshot_requested;
static char screenshot_name[256];
static GLuint shot_fbo;
static GLuint shot_tex;
static int shot_width;
static int shot_height;

#define SRC_WIDTH  304
#define SRC_HEIGHT 192

static const char *vertex_shader_src =
	"#version 130\n"
	"in vec2 a_pos;\n"
	"in vec2 a_uv;\n"
	"out vec4 TEX0;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
	"    TEX0 = vec4(a_uv, 0.0, 1.0);\n"
	"}\n";

static const char *nearest_shader_src =
	"#version 130\n"
	"in vec4 TEX0;\n"
	"uniform sampler2D Texture;\n"
	"out vec4 FragColor;\n"
	"void main()\n"
	"{\n"
	"    FragColor = texture(Texture, TEX0.xy);\n"
	"}\n";

static void *get_proc(const char *name)
{
	SDL_FunctionPointer ptr = SDL_GL_GetProcAddress(name);
	if (ptr == NULL)
	{
		fprintf(stderr, "OpenGL: missing function %s\n", name);
	}
	return (void *)ptr;
}

static GLuint compile_shader(GLenum type, const char *source)
{
	GLuint shader = p_glCreateShader(type);
	GLint ok = 0;

	if (shader == 0)
	{
		return 0;
	}

	p_glShaderSource(shader, 1, &source, NULL);
	p_glCompileShader(shader);
	p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLint length = 0;
		char *log;

		p_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
		log = (char *)malloc(length > 1 ? (size_t)length : 1);
		if (log != NULL)
		{
			p_glGetShaderInfoLog(shader, length, NULL, log);
			fprintf(stderr, "OpenGL: shader compile failed:\n%s\n", log);
			free(log);
		}
		p_glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static GLuint build_program(const char *fragment_source)
{
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
	GLuint program;
	GLint ok = 0;

	if (vs == 0 || fs == 0)
	{
		if (vs != 0) p_glDeleteShader(vs);
		if (fs != 0) p_glDeleteShader(fs);
		return 0;
	}

	program = p_glCreateProgram();
	p_glAttachShader(program, vs);
	p_glAttachShader(program, fs);
	p_glBindAttribLocation(program, 0, "a_pos");
	p_glBindAttribLocation(program, 1, "a_uv");
	p_glLinkProgram(program);
	p_glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok)
	{
		GLint length = 0;
		char *log;

		p_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
		log = (char *)malloc(length > 1 ? (size_t)length : 1);
		if (log != NULL)
		{
			p_glGetProgramInfoLog(program, length, NULL, log);
			fprintf(stderr, "OpenGL: program link failed:\n%s\n", log);
			free(log);
		}
		p_glDeleteProgram(program);
		program = 0;
	}

	p_glDeleteShader(vs);
	p_glDeleteShader(fs);
	return program;
}

int gl_render_init(SDL_Window *window)
{
	const GLubyte *version;

	gl_window = window;
	gl_context = SDL_GL_GetCurrentContext();
	if (gl_context == NULL)
	{
		return -1;
	}

	p_glCreateShader = (PFNGLCREATESHADERPROC)get_proc("glCreateShader");
	p_glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc("glShaderSource");
	p_glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc("glCompileShader");
	p_glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc("glGetShaderiv");
	p_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc("glGetShaderInfoLog");
	p_glDeleteShader = (PFNGLDELETESHADERPROC)get_proc("glDeleteShader");
	p_glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc("glCreateProgram");
	p_glAttachShader = (PFNGLATTACHSHADERPROC)get_proc("glAttachShader");
	p_glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc("glLinkProgram");
	p_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc("glGetProgramiv");
	p_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc("glGetProgramInfoLog");
	p_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc("glDeleteProgram");
	p_glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc("glUseProgram");
	p_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc("glGetUniformLocation");
	p_glUniform1i = (PFNGLUNIFORM1IPROC)get_proc("glUniform1i");
	p_glUniform1f = (PFNGLUNIFORM1FPROC)get_proc("glUniform1f");
	p_glUniform2f = (PFNGLUNIFORM2FPROC)get_proc("glUniform2f");
	p_glBindAttribLocation = (PFNGLBINDATTRIBLOCATIONPROC)get_proc("glBindAttribLocation");
	p_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc("glVertexAttribPointer");
	p_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc("glEnableVertexAttribArray");
	p_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)get_proc("glDisableVertexAttribArray");

	/* optional: used for occlusion independent screenshots */
	p_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)get_proc("glGenFramebuffers");
	p_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)get_proc("glBindFramebuffer");
	p_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)get_proc("glFramebufferTexture2D");
	p_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)get_proc("glCheckFramebufferStatus");
	p_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)get_proc("glDeleteFramebuffers");

	if (p_glCreateShader == NULL || p_glShaderSource == NULL ||
		p_glCompileShader == NULL || p_glGetShaderiv == NULL ||
		p_glGetShaderInfoLog == NULL || p_glDeleteShader == NULL ||
		p_glCreateProgram == NULL || p_glAttachShader == NULL ||
		p_glLinkProgram == NULL || p_glGetProgramiv == NULL ||
		p_glGetProgramInfoLog == NULL || p_glDeleteProgram == NULL ||
		p_glUseProgram == NULL || p_glGetUniformLocation == NULL ||
		p_glUniform1i == NULL || p_glUniform1f == NULL || p_glUniform2f == NULL ||
		p_glBindAttribLocation == NULL || p_glVertexAttribPointer == NULL ||
		p_glEnableVertexAttribArray == NULL || p_glDisableVertexAttribArray == NULL)
	{
		return -1;
	}

	version = glGetString(GL_VERSION);
	fprintf(stderr, "OpenGL %s\n", version != NULL ? (const char *)version : "(unknown)");

	program_nearest = build_program(nearest_shader_src);
	program_omniscale = build_program(omniscale_fragment_shader);
	if (program_nearest == 0 || program_omniscale == 0)
	{
		return -1;
	}

	glGenTextures(1, &gl_texture);
	glBindTexture(GL_TEXTURE_2D, gl_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SRC_WIDTH, SRC_HEIGHT, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);

	/* Do not block in SwapWindow: the game has its own frame limiter and
	 * a blocked swap would freeze input and audio whenever the window
	 * is occluded. */
	SDL_GL_SetSwapInterval(0);
	gl_render_set_filter(0);
	return 0;
}

void gl_render_done(void)
{
	if (shot_fbo != 0 && p_glDeleteFramebuffers != NULL)
	{
		p_glDeleteFramebuffers(1, &shot_fbo);
		shot_fbo = 0;
	}

	if (shot_tex != 0)
	{
		glDeleteTextures(1, &shot_tex);
		shot_tex = 0;
	}

	if (program_nearest != 0)
	{
		p_glDeleteProgram(program_nearest);
		program_nearest = 0;
	}
	if (program_omniscale != 0)
	{
		p_glDeleteProgram(program_omniscale);
		program_omniscale = 0;
	}
	if (gl_texture != 0)
	{
		glDeleteTextures(1, &gl_texture);
		gl_texture = 0;
	}
}

void gl_render_set_filter(int enabled)
{
	use_omniscale = enabled ? 1 : 0;
}

void gl_render_set_strength(float strength)
{
	if (strength < 0.0f)
	{
		strength = 0.0f;
	}
	if (strength > 1.0f)
	{
		strength = 1.0f;
	}
	smooth_strength = strength;
}

void gl_render_screenshot(const char *filename)
{
	SDL_strlcpy(screenshot_name, filename, sizeof(screenshot_name));
	screenshot_requested = 1;
}

static void save_screenshot(int x, int y, int width, int height)
{
	unsigned char *pixels;
	SDL_Surface *surface;
	int row;

	if (width <= 0 || height <= 0)
	{
		return;
	}

	pixels = (unsigned char *)SDL_malloc((size_t)width * height * 4);
	if (pixels == NULL)
	{
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
	if (surface != NULL && SDL_MUSTLOCK(surface))
	{
		SDL_LockSurface(surface);
	}

	if (surface != NULL)
	{
		for (row = 0; row < height; row++)
		{
			memcpy((unsigned char *)surface->pixels + row * surface->pitch,
				pixels + (height - 1 - row) * width * 4, (size_t)width * 4);
		}

		if (SDL_MUSTLOCK(surface))
		{
			SDL_UnlockSurface(surface);
		}

		if (!SDL_SavePNG(surface, screenshot_name[0] ? screenshot_name : "hota-screenshot.png"))
		{
			fprintf(stderr, "failed to save screenshot: %s\n", SDL_GetError());
		}
		SDL_DestroySurface(surface);
	}

	SDL_free(pixels);
}

/** Draws the game texture into the current framebuffer at the given
    rectangle. Both the on-screen frame and the offscreen screenshot
    capture use this. */
static void draw_scene(GLuint program, int x, int y, int w, int h)
{
	/* Vertices must be in "Z" order for a triangle strip: the two
	 * triangles share the quad diagonal. Listing the corners around the
	 * perimeter instead leaves a triangle of the screen unpainted. */
	static const float quad_pos[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
	static const float quad_uv[8] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };

	glViewport(x, y, w, h);

	glBindTexture(GL_TEXTURE_2D, gl_texture);
	p_glUseProgram(program);
	p_glUniform1i(p_glGetUniformLocation(program, "Texture"), 0);

	if (program == program_omniscale)
	{
		p_glUniform2f(p_glGetUniformLocation(program, "TextureSize"),
			(float)SRC_WIDTH, (float)SRC_HEIGHT);
		p_glUniform2f(p_glGetUniformLocation(program, "InputSize"),
			(float)SRC_WIDTH, (float)SRC_HEIGHT);
		p_glUniform2f(p_glGetUniformLocation(program, "OutputSize"),
			(float)w, (float)h);
		p_glUniform1f(p_glGetUniformLocation(program, "Strength"),
			smooth_strength);
	}

	p_glEnableVertexAttribArray(0);
	p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad_pos);
	p_glEnableVertexAttribArray(1);
	p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad_uv);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	p_glDisableVertexAttribArray(0);
	p_glDisableVertexAttribArray(1);
}

/** Creates/resizes the offscreen capture target. Returns zero when the
    framebuffer object path is unavailable. */
static GLuint ensure_shot_target(int width, int height)
{
	if (p_glGenFramebuffers == NULL || p_glBindFramebuffer == NULL ||
		p_glFramebufferTexture2D == NULL)
	{
		return 0;
	}

	if (shot_fbo == 0)
	{
		p_glGenFramebuffers(1, &shot_fbo);
		if (shot_fbo == 0)
		{
			return 0;
		}
	}

	if (shot_tex == 0 || shot_width != width || shot_height != height)
	{
		GLenum status;

		if (shot_tex == 0)
		{
			glGenTextures(1, &shot_tex);
		}

		glBindTexture(GL_TEXTURE_2D, shot_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		p_glBindFramebuffer(GL_FRAMEBUFFER, shot_fbo);
		p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D, shot_tex, 0);

		status = p_glCheckFramebufferStatus != NULL
			? p_glCheckFramebufferStatus(GL_FRAMEBUFFER)
			: GL_FRAMEBUFFER_COMPLETE;

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
			p_glDeleteFramebuffers(1, &shot_fbo);
			shot_fbo = 0;
			glDeleteTextures(1, &shot_tex);
			shot_tex = 0;
			shot_width = 0;
			shot_height = 0;
			return 0;
		}

		shot_width = width;
		shot_height = height;
	}

	p_glBindFramebuffer(GL_FRAMEBUFFER, shot_fbo);
	return shot_fbo;
}

/** Captures one frame. Renders offscreen so the result does not depend on
    whether the window is visible, then falls back to reading the window
    back buffer. */
static void capture_screenshot(GLuint program, int x, int y, int w, int h)
{
	if (ensure_shot_target(w, h) != 0)
	{
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		draw_scene(program, 0, 0, w, h);
		save_screenshot(0, 0, w, h);
		p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else
	{
		save_screenshot(x, y, w, h);
	}
}

void gl_render_frame(const unsigned char *indices, int pitch,
	const SDL_Color *palette, int width, int height)
{
	static unsigned char rgba[SRC_WIDTH * SRC_HEIGHT * 4];
	GLuint program;
	int x, y, pixel;
	int win_w = 0, win_h = 0;
	int view_w, view_h, view_x, view_y;
	float scale_x, scale_y, scale;

	(void)width;
	(void)height;

	if (gl_window == NULL || gl_context == NULL || gl_texture == 0)
	{
		return;
	}

	if (!SDL_GetWindowSizeInPixels(gl_window, &win_w, &win_h) || win_w <= 0 || win_h <= 0)
	{
		return;
	}

	/* Expand the 4-bit palette indices into RGBA once per frame. */
	for (y = 0; y < SRC_HEIGHT; y++)
	{
		const unsigned char *row = indices + y * pitch;
		for (x = 0; x < SRC_WIDTH; x++)
		{
			pixel = row[x] & 0x0f;
			rgba[(y * SRC_WIDTH + x) * 4 + 0] = palette[pixel].r;
			rgba[(y * SRC_WIDTH + x) * 4 + 1] = palette[pixel].g;
			rgba[(y * SRC_WIDTH + x) * 4 + 2] = palette[pixel].b;
			rgba[(y * SRC_WIDTH + x) * 4 + 3] = 255;
		}
	}

	glBindTexture(GL_TEXTURE_2D, gl_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SRC_WIDTH, SRC_HEIGHT,
		GL_RGBA, GL_UNSIGNED_BYTE, rgba);

	scale_x = (float)win_w / (float)SRC_WIDTH;
	scale_y = (float)win_h / (float)SRC_HEIGHT;
	scale = (scale_x < scale_y) ? scale_x : scale_y;
	view_w = (int)(SRC_WIDTH * scale + 0.5f);
	view_h = (int)(SRC_HEIGHT * scale + 0.5f);
	view_x = (win_w - view_w) / 2;
	view_y = (win_h - view_h) / 2;

	program = use_omniscale ? program_omniscale : program_nearest;

	if (p_glBindFramebuffer != NULL)
	{
		p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	glViewport(0, 0, win_w, win_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	draw_scene(program, view_x, view_y, view_w, view_h);

	if (screenshot_requested)
	{
		screenshot_requested = 0;
		capture_screenshot(program, view_x, view_y, view_w, view_h);
	}

	SDL_GL_SwapWindow(gl_window);
}

#endif /* HAVE_OPENGL */
