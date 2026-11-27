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

#ifndef RENDER_H
#define RENDER_H

int render_init(void);
int render_create_surface(void);
void render_done(void);

int get_scroll_register(void);
void set_scroll(int scroll);

void render(unsigned char *src);
void render_blank(void);

void set_palette(int which);
void set_palette_rgb12(unsigned char *rgb12);

void toggle_fullscreen(void);
void toggle_upscale(void);
void toggle_scaler(void);
void toggle_filter(void);
void toggle_integer_scale(void);

int render_set_upscale(int factor);
int render_get_upscale(void);

int get_current_palette(void);

void screenshot(void);

#endif // RENDER_H
