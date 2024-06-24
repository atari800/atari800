/*
 * sdl/video_gl.c - SDL library specific port code - OpenGL accelerated video display
 *
 * Copyright (c) 2010 Tomasz Krasuski
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <SDL.h>
#include <SDL_opengl.h>
#include <stdlib.h>

#include "af80.h"
#include "bit3.h"
#include "artifact.h"
#include "atari.h"
#include "cfg.h"
#include "colours.h"
#include "config.h"
#include "filter_ntsc.h"
#include "log.h"
#include "pbi_proto80.h"
#ifdef PAL_BLENDING
#include "pal_blending.h"
#endif /* PAL_BLENDING */
#include "platform.h"
#include "screen.h"
#include "videomode.h"
#include "xep80.h"
#include "xep80_fonts.h"
#include "util.h"

#include "sdl/palette.h"
#include "sdl/video.h"
#include "sdl/video_gl.h"

#ifndef M_PI
# define M_PI 3.141592653589793
#endif

static int currently_rotated = FALSE;
/* If TRUE, then 32 bit, else 16 bit screen. */
static int bpp_32 = FALSE;

int SDL_VIDEO_GL_filtering = 0;
int SDL_VIDEO_GL_pixel_format = SDL_VIDEO_GL_PIXEL_FORMAT_BGR16;
#if SDL2
#  define SDL_OpenGL_FLAG SDL_WINDOW_OPENGL
#  define SDL_OpenGL_FULLSCREEN SDL_WINDOW_FULLSCREEN
static int screen_width = 0;
static int screen_height = 0;
float zoom_factor = 1.0f;
#else
#  define SDL_OpenGL_FLAG SDL_OPENGL
#  define SDL_OpenGL_FULLSCREEN SDL_FULLSCREEN
#endif

/* Path to the OpenGL shared library. */
static char const *library_path = NULL;

/* Pointers to OpenGL functions, loaded dynamically during initialisation. */
static struct
{
	void(APIENTRY*Viewport)(GLint,GLint,GLsizei,GLsizei);
	void(APIENTRY*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
	void(APIENTRY*Clear)(GLbitfield);
	void(APIENTRY*Enable)(GLenum);
	void(APIENTRY*Disable)(GLenum);
	void(APIENTRY*GenTextures)(GLsizei, GLuint*);
	void(APIENTRY*DeleteTextures)(GLsizei, const GLuint*);
	void(APIENTRY*BindTexture)(GLenum, GLuint);
	void(APIENTRY*TexParameteri)(GLenum, GLenum, GLint);
	void(APIENTRY*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*);
	void(APIENTRY*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*);
	void(APIENTRY*TexCoord2f)(GLfloat, GLfloat);
	void(APIENTRY*Vertex3f)(GLfloat, GLfloat, GLfloat);
	void(APIENTRY*Color4f)(GLfloat, GLfloat, GLfloat, GLfloat);
	void(APIENTRY*BlendFunc)(GLenum,GLenum);
	void(APIENTRY*MatrixMode)(GLenum);
	void(APIENTRY*Ortho)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble);
	void(APIENTRY*LoadIdentity)(void);
	void(APIENTRY*Begin)(GLenum);
	void(APIENTRY*End)(void);
	void(APIENTRY*GetIntegerv)(GLenum, GLint*);
	const GLubyte*(APIENTRY*GetString)(GLenum);
	GLuint(APIENTRY*GenLists)(GLsizei);
	void(APIENTRY*DeleteLists)(GLuint, GLsizei);
	void(APIENTRY*NewList)(GLuint, GLenum);
	void(APIENTRY*EndList)(void);
	void(APIENTRY*CallList)(GLuint);
	void(APIENTRY*GenBuffersARB)(GLsizei, GLuint*);
	void(APIENTRY*DeleteBuffersARB)(GLsizei, const GLuint*);
	void(APIENTRY*BindBufferARB)(GLenum, GLuint);
	void(APIENTRY*BufferDataARB)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
	void*(APIENTRY*MapBuffer)(GLenum, GLenum);
	GLboolean(APIENTRY*UnmapBuffer)(GLenum);
#if SDL2
	GLenum (APIENTRY* GetError)(void);
	GLuint (APIENTRY* CreateShader)(GLenum);
	void   (APIENTRY* ShaderSource)(GLuint shader, GLsizei count, GLchar* const* string, const GLint* length);
	GLuint (APIENTRY* CreateProgram)(void);
	void   (APIENTRY* CompileShader)(GLuint shader);
	void   (APIENTRY* GetShaderiv)(GLuint shader, GLenum pname, GLint* params);
	void   (APIENTRY* GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
	void   (APIENTRY* AttachShader)(GLuint program, GLuint shader);
	void   (APIENTRY* LinkProgram)(GLuint program);
	void   (APIENTRY* GetProgramiv)(GLuint program, GLenum pname, GLint* params);
	void   (APIENTRY* GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
	void   (APIENTRY* DeleteShader)(GLuint shader);
	void   (APIENTRY* UseProgram)(GLuint program);
	void   (APIENTRY* Uniform1f)(GLint location, GLfloat v0);
	void   (APIENTRY* Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
	void   (APIENTRY* Uniform1i)(GLint location, GLint v0);
	void   (APIENTRY* UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
	void   (APIENTRY* ActiveTexture)(GLenum texture);
	void   (APIENTRY* VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
	void   (APIENTRY* EnableVertexAttribArray)(GLuint index);
	void   (APIENTRY* GenVertexArrays)(GLsizei n, GLuint* arrays);
	void   (APIENTRY* BindVertexArray)(GLuint array);
	void   (APIENTRY* GenBuffers)(GLsizei n, GLuint* buffers);
	void   (APIENTRY* BufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
	void   (APIENTRY* BindBuffer)(GLenum target, GLuint buffer);
	GLint  (APIENTRY* GetUniformLocation)(GLuint program, const GLchar* name);
	GLint  (APIENTRY* GetAttribLocation)(GLuint program, const GLchar* name);
	void   (APIENTRY* DrawElements)(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
#endif
} gl;

static void DisplayNormal(GLvoid *dest);
#if NTSC_FILTER
static void DisplayNTSCEmu(GLvoid *dest);
#endif
#ifdef XEP80_EMULATION
static void DisplayXEP80(GLvoid *dest);
#endif
#ifdef PBI_PROTO80
static void DisplayProto80(GLvoid *dest);
#endif
#ifdef AF80
static void DisplayAF80(GLvoid *dest);
#endif
#ifdef BIT3
static void DisplayBIT3(GLvoid *dest);
#endif
#ifdef PAL_BLENDING
static void DisplayPalBlending(GLvoid *dest);
#endif /* PAL_BLENDING */

static void (* blit_funcs[VIDEOMODE_MODE_SIZE])(GLvoid *) = {
	&DisplayNormal
#if NTSC_FILTER
	,&DisplayNTSCEmu
#endif
#ifdef XEP80_EMULATION
	,&DisplayXEP80
#endif
#ifdef PBI_PROTO80
	,&DisplayProto80
#endif
#ifdef AF80
	,&DisplayAF80
#endif
#ifdef BIT3
	,&DisplayBIT3
#endif
};

/* GL textures - [0] is screen, [1] is scanlines. */
static GLuint textures[2];

int SDL_VIDEO_GL_pbo = TRUE;

/* Indicates whether Pixel Buffer Objects GL extension is available.
   Available from OpenGL 2.1, it gives a significant boost in blit speed. */
static int pbo_available;
/* Name of the main screen Pixel Buffer Object. */
static GLuint screen_pbo;

/* Data for the screen texture. not used when PBOs are used. */
static GLvoid *screen_texture = NULL;

/* 16- and 32-bit ARGB textures, both of size 1x2, used for displaying scanlines.
   They contain a transparent black pixel above an opaque black pixel. */
static Uint32 const scanline_tex32[2] = { 0x00000000, 0xff000000 }; /* BGRA 8-8-8-8-REV */
/* The 16-bit one is padded to 32 bits, hence it contains 4 values, not 2. */
static Uint16 const scanline_tex16[4] = { 0x0000, 0x0000, 0x8000, 0x0000 }; /* BGRA 5-5-5-1-REV */

/* Variables used with "subpixel shifting". Screen and scanline textures
   sometimes are intentionally shifted by a part of a pixel to look better/clearer. */
static GLfloat screen_vshift;
static GLfloat screen_hshift;
static GLfloat scanline_vshift;
static int paint_scanlines;

/* GL Display List for placing the screen and scanline textures on the screen. */
static GLuint screen_dlist;

static char const * const pixel_format_cfg_strings[SDL_VIDEO_GL_PIXEL_FORMAT_SIZE] = {
	"BGR16",
	"RGB16",
	"BGRA32",
	"ARGB32"
};

typedef struct pixel_format_t {
	GLint internal_format;
	GLenum format;
	GLenum type;
	Uint32 black_pixel;
	Uint32 rmask;
	Uint32 gmask;
	Uint32 bmask;
	void(*calc_pal_func)(void *dest, int const *palette, int size);
	void(*ntsc_blit_func)(atari_ntsc_t const*, ATARI_NTSC_IN_T const*, long, int, int, void*, long);
} pixel_format_t;

pixel_format_t const pixel_formats[4] = {
	{ GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, 0x0000,
	  0x0000001f, 0x000007e0, 0x0000f800,
	  &SDL_PALETTE_Calculate16_B5G6R5, &atari_ntsc_blit_bgr16 },
	{ GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0x0000,
	  0x0000f800, 0x000007e0, 0x0000001f,
	  &SDL_PALETTE_Calculate16_R5G6B5, &atari_ntsc_blit_rgb16 }, /* NVIDIA 16-bit */
	{ GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, 0xff000000,
	  0x0000ff00, 0x00ff0000, 0xff000000,
	  &SDL_PALETTE_Calculate32_B8G8R8A8, &atari_ntsc_blit_bgra32 },
	{ GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0xff000000,
	  0x00ff0000, 0x0000ff00, 0x000000ff,
	  &SDL_PALETTE_Calculate32_A8R8G8B8, &atari_ntsc_blit_argb32 } /* NVIDIA 32-bit */
};

/* Conversion between function pointers and 'void *' is forbidden in
   ISO C, but unfortunately unavoidable when using SDL_GL_GetProcAddress.
   So the code below is non-portable and gives a warning with gcc -ansi
   -Wall -pedantic, but is the only possible solution. */
static void (*GetGlFunc(const char* s))(void)
{
/* suppress warning: ISO C forbids conversion of object pointer to function pointer type [-pedantic] */
#ifdef __GNUC__
	__extension__
#endif
	void(*f)(void) = (void(*)(void))SDL_GL_GetProcAddress(s);
	if (f == NULL)
		Log_print("Unable to get function pointer for %s\n",s);
	return f;
}

/* Alocates memory for the screen texture, if needed. */
static void AllocTexture(void)
{
	if (!SDL_VIDEO_GL_pbo && screen_texture == NULL)
		/* The largest width is in NTSC-filtered full overscan mode - 672 pixels.
		   The largest height is in PAL XEP-80 mode - 300 pixels. Add 1 pixel at each side
		   to nicely render screen borders. The texture is 1024x512, which is more than
		   enough - although it's rounded to powers of 2 to be more compatible (earlier
		   versions of OpenGL supported only textures with width/height of powers of 2). */
		screen_texture = Util_malloc(1024*512*(bpp_32 ? sizeof(Uint32) : sizeof(Uint16)));
}

/* Frees memory for the screen texture, if needed. */
static void FreeTexture(void)
{
	if (screen_texture != NULL) {
		free(screen_texture);
		screen_texture = NULL;
	}
}

/* Sets up the initial parameters of the OpenGL context. See also CleanGlContext. */
static void InitGlContext(void)
{
	GLint filtering = SDL_VIDEO_GL_filtering ? GL_LINEAR : GL_NEAREST;
#if SDL2
	filtering = GL_NEAREST;
#endif
	gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.Clear(GL_COLOR_BUFFER_BIT);

	gl.Enable(GL_TEXTURE_2D);
	gl.GenTextures(2, textures);

	/* Screen texture. */
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	/* Scanlines texture. */
	filtering = SDL_VIDEO_interpolate_scanlines ? GL_LINEAR : GL_NEAREST;
	gl.BindTexture(GL_TEXTURE_2D, textures[1]);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	gl.MatrixMode(GL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(-1.0, 1.0, -1.0, 1.0, 0.0, 10.0);
	gl.MatrixMode(GL_MODELVIEW);
	gl.LoadIdentity();
	screen_dlist = gl.GenLists(1);
	if (SDL_VIDEO_GL_pbo)
		gl.GenBuffersARB(1, &screen_pbo);
}

/* Cleans up the structures allocated in InitGlContext. */
static void CleanGlContext(void)
{
	if (!gl.DeleteLists) return;

		if (SDL_VIDEO_GL_pbo)
			gl.DeleteBuffersARB(1, &screen_pbo);
		gl.DeleteLists(screen_dlist, 1);
		gl.DeleteTextures(2, textures);
}

/* Sets up the initial parameters of all used textures and the PBO. */
static void InitGlTextures(void)
{
	/* Texture for the display surface. */
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	gl.TexImage2D(GL_TEXTURE_2D, 0, pixel_formats[SDL_VIDEO_GL_pixel_format].internal_format, 1024, 512, 0,
	              pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
	              NULL);
	/* Texture for scanlines. */
	gl.BindTexture(GL_TEXTURE_2D, textures[1]);
	if (bpp_32)
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 2, 0,
		              GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
		              scanline_tex32);
	else
		gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 2, 0,
		              GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
		              scanline_tex16);
	if (SDL_VIDEO_GL_pbo) {
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
		gl.BufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, 1024*512*(bpp_32 ? sizeof(Uint32) : sizeof(Uint16)), NULL, GL_DYNAMIC_DRAW_ARB);
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
}

void SDL_VIDEO_GL_Cleanup(void)
{
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG)
		CleanGlContext();
	FreeTexture();
}

void SDL_VIDEO_GL_GetPixelFormat(PLATFORM_pixel_format_t *format)
{
	format->bpp = bpp_32 ? 32 : 16;
	format->rmask = pixel_formats[SDL_VIDEO_GL_pixel_format].rmask;
	format->gmask = pixel_formats[SDL_VIDEO_GL_pixel_format].gmask;
	format->bmask = pixel_formats[SDL_VIDEO_GL_pixel_format].bmask;
}

void SDL_VIDEO_GL_MapRGB(void *dest, int const *palette, int size)
{
	(*pixel_formats[SDL_VIDEO_GL_pixel_format].calc_pal_func)(dest, palette, size);
}

/* Calculate the palette in the 32-bit BGRA format, or 16-bit BGR 5-6-5 format. */
static void UpdatePaletteLookup(VIDEOMODE_MODE_t mode)
{
	SDL_VIDEO_UpdatePaletteLookup(mode, bpp_32);
}

void SDL_VIDEO_GL_PaletteUpdate(void)
{
	UpdatePaletteLookup(SDL_VIDEO_current_display_mode);
}

/* Set parameters that will shift the screen and scanline textures a bit,
   in order to look better/cleaner on screen. */
static void SetSubpixelShifts(void)
{
	int dest_width;
	int dest_height;
	int vmult, hmult;
	if (currently_rotated) {
		dest_width = VIDEOMODE_dest_height;
		dest_height = VIDEOMODE_dest_width;
	} else {
		dest_width = VIDEOMODE_dest_width;
		dest_height = VIDEOMODE_dest_height;
	}
	vmult = dest_height / VIDEOMODE_src_height;
	hmult = dest_width / VIDEOMODE_src_width;

	paint_scanlines = vmult >= 2 && SDL_VIDEO_scanlines_percentage != 0;

	if (dest_height % VIDEOMODE_src_height == 0 &&
	    SDL_VIDEO_GL_filtering &&
	    !(vmult & 1))
		screen_vshift = 0.5 / vmult;
	else
		screen_vshift = 0.0;

	
	if (dest_height % VIDEOMODE_src_height == 0 &&
	    ((SDL_VIDEO_interpolate_scanlines && !(vmult & 1)) ||
	     (!SDL_VIDEO_interpolate_scanlines && (vmult & 3) == 3)
	    )
	   )
		scanline_vshift = -0.25 + 0.5 / vmult;
	else
		scanline_vshift = -0.25;

	if (dest_width % VIDEOMODE_src_width == 0 &&
	    SDL_VIDEO_GL_filtering &&
	    !(hmult & 1))
		screen_hshift = 0.5 / hmult;
	else
		screen_hshift = 0.0;
}

/* Sets up the GL Display List that creates a textured rectangle of the main
   screen and a second, translucent, rectangle with scanlines. */
static void SetGlDisplayList(void)
{
#if SDL2
	return;
#endif
	gl.NewList(screen_dlist, GL_COMPILE);
	gl.Clear(GL_COLOR_BUFFER_BIT);
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	gl.Begin(GL_QUADS);
	if (currently_rotated) {
		gl.TexCoord2f(screen_hshift/1024.0f, ((GLfloat)VIDEOMODE_src_height + screen_vshift)/512.0f);
		gl.Vertex3f(1.0f, -1.0f, -2.0f);
		gl.TexCoord2f(((GLfloat)VIDEOMODE_actual_width + screen_hshift)/1024.0f, ((GLfloat)VIDEOMODE_src_height + screen_vshift)/512.0f);
		gl.Vertex3f(1.0f, 1.0f, -2.0f);
		gl.TexCoord2f(((GLfloat)VIDEOMODE_actual_width + screen_hshift)/1024.0f, screen_vshift/512.0f);
		gl.Vertex3f(-1.0f, 1.0f, -2.0f);
		gl.TexCoord2f(screen_hshift/1024.0f, screen_vshift/512.0f);
		gl.Vertex3f(-1.0f, -1.0f, -2.0f);
	} else {
		gl.TexCoord2f(screen_hshift/1024.0f, ((GLfloat)VIDEOMODE_src_height + screen_vshift)/512.0f);
		gl.Vertex3f(-1.0f, -1.0f, -2.0f);
		gl.TexCoord2f(((GLfloat)VIDEOMODE_actual_width + screen_hshift)/1024.0f, ((GLfloat)VIDEOMODE_src_height + screen_vshift)/512.0f);
		gl.Vertex3f(1.0f, -1.0f, -2.0f);
		gl.TexCoord2f(((GLfloat)VIDEOMODE_actual_width + screen_hshift)/1024.0f, screen_vshift/512.0f);
		gl.Vertex3f(1.0f, 1.0f, -2.0f);
		gl.TexCoord2f(screen_hshift/1024.0f, screen_vshift/512.0f);
		gl.Vertex3f(-1.0f, 1.0f, -2.0f);
	}
	gl.End();
	if (paint_scanlines) {
		gl.Enable(GL_BLEND);
		gl.Color4f(1.0f, 1.0f, 1.0f, ((GLfloat)SDL_VIDEO_scanlines_percentage / 100.0f));
		gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		gl.BindTexture(GL_TEXTURE_2D, textures[1]);
		gl.Begin(GL_QUADS);
		if (currently_rotated) {
			gl.TexCoord2f(0.0f, (GLfloat)VIDEOMODE_src_height + scanline_vshift);
			gl.Vertex3f(1.0f, -1.0f, -1.0f);
			gl.TexCoord2f(1.0f, (GLfloat)VIDEOMODE_src_height + scanline_vshift);
			gl.Vertex3f(1.0f, 1.0f, -1.0f);
			gl.TexCoord2f(1.0f, scanline_vshift);
			gl.Vertex3f(-1.0f, 1.0f, -1.0f);
			gl.TexCoord2f(0.0f, scanline_vshift);
			gl.Vertex3f(-1.0f, -1.0f, -1.0f);
		} else {
			gl.TexCoord2f(0.0f, (GLfloat)VIDEOMODE_src_height + scanline_vshift);
			gl.Vertex3f(-1.0f, -1.0f, -1.0f);
			gl.TexCoord2f(1.0f, (GLfloat)VIDEOMODE_src_height + scanline_vshift);
			gl.Vertex3f(1.0f, -1.0f, -1.0f);
			gl.TexCoord2f(1.0f, scanline_vshift);
			gl.Vertex3f(1.0f, 1.0f, -1.0f);
			gl.TexCoord2f(0.0f, scanline_vshift);
			gl.Vertex3f(-1.0f, 1.0f, -1.0f);
		}
		gl.End();
		gl.Disable(GL_BLEND);
	}
	gl.EndList();
}

/* Resets the screen texture/PBO to all-black. */
static void CleanDisplayTexture(void)
{
	GLvoid *ptr;
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	if (SDL_VIDEO_GL_pbo) {
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
		ptr = gl.MapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	}
	else
		ptr = screen_texture;
	if (bpp_32) {
		Uint32* tex = (Uint32 *)ptr;
		unsigned int i;
		for (i = 0; i < 1024*512; i ++)
			/* Set alpha channel to full opacity. */
			tex[i] = pixel_formats[SDL_VIDEO_GL_pixel_format].black_pixel;

	} else
		memset(ptr, 0x00, 1024*512*sizeof(Uint16));
	if (SDL_VIDEO_GL_pbo) {
		gl.UnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
		ptr = NULL;
	}
	if (bpp_32)
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				ptr);
	else
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 512,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
				ptr);
	if (SDL_VIDEO_GL_pbo)
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
}

#if SDL2
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-function-pointer-types"
#endif

/* Sets pointers to OpenGL functions. Returns TRUE on success, FALSE on failure. */
static int InitGlFunctions(void)
{
	if (
#if SDL2
	(gl.GetError = GetGlFunc("glGetError")) == NULL ||
	(gl.CreateShader = GetGlFunc("glCreateShader")) == NULL ||
	(gl.ShaderSource = GetGlFunc("glShaderSource")) == NULL ||
	(gl.CreateProgram = GetGlFunc("glCreateProgram")) == NULL ||
	(gl.CompileShader = GetGlFunc("glCompileShader")) == NULL ||
	(gl.GetShaderiv = GetGlFunc("glGetShaderiv")) == NULL ||
	(gl.GetShaderInfoLog = GetGlFunc("glGetShaderInfoLog")) == NULL ||
	(gl.AttachShader = GetGlFunc("glAttachShader")) == NULL ||
	(gl.LinkProgram = GetGlFunc("glLinkProgram")) == NULL ||
	(gl.GetProgramiv = GetGlFunc("glGetProgramiv")) == NULL ||
	(gl.GetProgramInfoLog = GetGlFunc("glGetProgramInfoLog")) == NULL ||
	(gl.DeleteShader = GetGlFunc("glDeleteShader")) == NULL ||
	(gl.UseProgram = GetGlFunc("glUseProgram")) == NULL ||
	(gl.Uniform1f = GetGlFunc("glUniform1f")) == NULL ||
	(gl.Uniform2f = GetGlFunc("glUniform2f")) == NULL ||
	(gl.Uniform1i = GetGlFunc("glUniform1i")) == NULL ||
	(gl.UniformMatrix4fv = GetGlFunc("glUniformMatrix4fv")) == NULL ||
	(gl.ActiveTexture = GetGlFunc("glActiveTexture")) == NULL ||
	(gl.BindBuffer = GetGlFunc("glBindBuffer")) == NULL ||
	(gl.BufferData = GetGlFunc("glBufferData")) == NULL ||
	(gl.GenBuffers = GetGlFunc("glGenBuffers")) == NULL ||
	(gl.VertexAttribPointer = GetGlFunc("glVertexAttribPointer")) == NULL ||
	(gl.EnableVertexAttribArray = GetGlFunc("glEnableVertexAttribArray")) == NULL ||
	(gl.GenVertexArrays = GetGlFunc("glGenVertexArrays")) == NULL ||
	(gl.BindVertexArray = GetGlFunc("glBindVertexArray")) == NULL ||
	(gl.GetUniformLocation = GetGlFunc("glGetUniformLocation")) == NULL ||
	(gl.GetAttribLocation = GetGlFunc("glGetAttribLocation")) == NULL ||
	(gl.DrawElements = GetGlFunc("glDrawElements")) == NULL ||
#endif
	    (gl.Viewport = (void(APIENTRY*)(GLint,GLint,GLsizei,GLsizei))GetGlFunc("glViewport")) == NULL ||
	    (gl.ClearColor = (void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat))GetGlFunc("glClearColor")) == NULL ||
	    (gl.Clear = (void(APIENTRY*)(GLbitfield))GetGlFunc("glClear")) == NULL ||
	    (gl.Enable = (void(APIENTRY*)(GLenum))GetGlFunc("glEnable")) == NULL ||
	    (gl.Disable = (void(APIENTRY*)(GLenum))GetGlFunc("glDisable")) == NULL ||
	    (gl.GenTextures = (void(APIENTRY*)(GLsizei, GLuint*))GetGlFunc("glGenTextures")) == NULL ||
	    (gl.DeleteTextures = (void(APIENTRY*)(GLsizei, const GLuint*))GetGlFunc("glDeleteTextures")) == NULL ||
	    (gl.BindTexture = (void(APIENTRY*)(GLenum, GLuint))GetGlFunc("glBindTexture")) == NULL ||
	    (gl.TexParameteri = (void(APIENTRY*)(GLenum, GLenum, GLint))GetGlFunc("glTexParameteri")) == NULL ||
	    (gl.TexImage2D = (void(APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*))GetGlFunc("glTexImage2D")) == NULL ||
	    (gl.TexSubImage2D = (void(APIENTRY*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*))GetGlFunc("glTexSubImage2D")) == NULL ||
	    (gl.TexCoord2f = (void(APIENTRY*)(GLfloat, GLfloat))GetGlFunc("glTexCoord2f")) == NULL ||
	    (gl.Vertex3f = (void(APIENTRY*)(GLfloat, GLfloat, GLfloat))GetGlFunc("glVertex3f")) == NULL ||
	    (gl.Color4f = (void(APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat))GetGlFunc("glColor4f")) == NULL ||
	    (gl.BlendFunc = (void(APIENTRY*)(GLenum,GLenum))GetGlFunc("glBlendFunc")) == NULL ||
	    (gl.MatrixMode = (void(APIENTRY*)(GLenum))GetGlFunc("glMatrixMode")) == NULL ||
	    (gl.Ortho = (void(APIENTRY*)(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble))GetGlFunc("glOrtho")) == NULL ||
	    (gl.LoadIdentity = (void(APIENTRY*)(void))GetGlFunc("glLoadIdentity")) == NULL ||
	    (gl.Begin = (void(APIENTRY*)(GLenum))GetGlFunc("glBegin")) == NULL ||
	    (gl.End = (void(APIENTRY*)(void))GetGlFunc("glEnd")) == NULL ||
	    (gl.GetIntegerv = (void(APIENTRY*)(GLenum, GLint*))GetGlFunc("glGetIntegerv")) == NULL ||
	    (gl.GetString = (const GLubyte*(APIENTRY*)(GLenum))GetGlFunc("glGetString")) == NULL ||
	    (gl.GenLists = (GLuint(APIENTRY*)(GLsizei))GetGlFunc("glGenLists")) == NULL ||
	    (gl.DeleteLists = (void(APIENTRY*)(GLuint, GLsizei))GetGlFunc("glDeleteLists")) == NULL ||
	    (gl.NewList = (void(APIENTRY*)(GLuint, GLenum))GetGlFunc("glNewList")) == NULL ||
	    (gl.EndList = (void(APIENTRY*)(void))GetGlFunc("glEndList")) == NULL ||
	    (gl.CallList = (void(APIENTRY*)(GLuint))GetGlFunc("glCallList")) == NULL)
		return FALSE;
	return TRUE;
}

#if SDL2
#pragma GCC diagnostic pop
#endif

/* Checks availability of Pixel Buffer Objests extension and sets pointers of PBO-related OpenGL functions.
   Returns TRUE on success, FALSE on failure. */
static int InitGlPbo(void)
{
#if SDL2
	// OpenGL 3.3 and up
	/*
	int pbo = FALSE;
	GLint n = 0; 
	gl.GetIntegerv(GL_NUM_EXTENSIONS, &n);

	PFNGLGETSTRINGIPROC glGetStringi = (PFNGLGETSTRINGIPROC)GetGlFunc("glGetStringi");
	if (!glGetStringi) return FALSE;

	for (GLint i = 0; i < n; ++i) { 
		const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
		printf("ext: '%s'\n", extension);
		if (strcmp(extension, "GL_ARB_pixel_buffer_object") == 0) {
			pbo = TRUE;
			break;
		}
	} 
	if (!pbo) return FALSE;
	*/
	return FALSE;
#else
	const GLubyte *extensions = gl.GetString(GL_EXTENSIONS);
	if (!strstr((char *)extensions, "EXT_pixel_buffer_object")) {
		return FALSE;
	}
#endif
	if ((gl.GenBuffersARB = (void(APIENTRY*)(GLsizei, GLuint*))GetGlFunc("glGenBuffersARB")) == NULL ||
	    (gl.DeleteBuffersARB = (void(APIENTRY*)(GLsizei, const GLuint*))GetGlFunc("glDeleteBuffersARB")) == NULL ||
	    (gl.BindBufferARB = (void(APIENTRY*)(GLenum, GLuint))GetGlFunc("glBindBufferARB")) == NULL ||
	    (gl.BufferDataARB = (void(APIENTRY*)(GLenum, GLsizeiptr, const GLvoid*, GLenum))GetGlFunc("glBufferDataARB")) == NULL ||
	    (gl.MapBuffer = (void*(APIENTRY*)(GLenum, GLenum))GetGlFunc("glMapBufferARB")) == NULL ||
	    (gl.UnmapBuffer = (GLboolean(APIENTRY*)(GLenum))GetGlFunc("glUnmapBufferARB")) == NULL)
		return FALSE;

	return TRUE;
}

static void ModeInfo(void)
{
	const char *fullstring = (SDL_VIDEO_screen->flags & SDL_OpenGL_FULLSCREEN) == SDL_OpenGL_FULLSCREEN ? "fullscreen" : "windowed";
	Log_print("Video Mode: %dx%dx%d %s, pixel format: %s", SDL_VIDEO_screen->w, SDL_VIDEO_screen->h,
		   SDL_VIDEO_screen->format->BitsPerPixel, fullstring, pixel_format_cfg_strings[SDL_VIDEO_GL_pixel_format]);
}


/* Return value of TRUE indicates that the video subsystem was reinitialised. */
static int SetVideoMode(int w, int h, int windowed)
{
	int reinit = FALSE;
#if SDL2
	static SDL_Surface OpenGL_screen_dummy;
	static SDL_PixelFormat pix;

	Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN;
	if (!windowed) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	if (SDL_VIDEO_wnd && (!SDL_VIDEO_screen || SDL_VIDEO_screen->flags != flags)) {
		SDL_DestroyWindow(SDL_VIDEO_wnd);
		SDL_VIDEO_wnd = 0;
	}

	if (!SDL_VIDEO_wnd) {
		SDL_VIDEO_wnd = SDL_CreateWindow(Atari800_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);
		if (!SDL_VIDEO_wnd) {
			Log_print("Creating an OpenGL window with size %dx%d failed: %s", w, h, SDL_GetError());
			Log_flushlog();
			exit(-1);
		}

		SDL_GLContext ctx = SDL_GL_CreateContext(SDL_VIDEO_wnd);
		if (!ctx) {
			Log_print("OpenGL context could not be created: %s", SDL_GetError());
			Log_flushlog();
			exit(-1);
		}
	
		memset(&OpenGL_screen_dummy, 0, sizeof(OpenGL_screen_dummy));
		memset(&pix, 0, sizeof(pix));
		SDL_VIDEO_screen = &OpenGL_screen_dummy;
		SDL_VIDEO_screen->format = &pix;
		SDL_VIDEO_screen->w = w;
		SDL_VIDEO_screen->h = h;
		SDL_VIDEO_screen->flags = flags;

		reinit = TRUE;
	}
	else {
		int cw, ch;
		SDL_GetWindowSize(SDL_VIDEO_wnd, &cw, &ch);
		if (w != cw || h != ch) {
			SDL_SetWindowSize(SDL_VIDEO_wnd, w, h);
		}
	}

	int width = 0, height = 0;
	SDL_GL_GetDrawableSize(SDL_VIDEO_wnd, &width, &height);
	SDL_VIDEO_width = width;
	SDL_VIDEO_height = height;
	screen_width = width;
	screen_height = height;
	VIDEOMODE_dest_scale_factor = (double)width / w;

	SDL_VIDEO_vsync_available = TRUE;
	if (SDL_VIDEO_vsync) {
		SDL_GL_SetSwapInterval(1); // VSync
	}

#else
	Uint32 flags = SDL_OPENGL | (windowed ? SDL_RESIZABLE : SDL_OpenGL_FULLSCREEN);
	/* In OpenGL mode, the SDL screen is always opened with the default
	   desktop depth - it is the most compatible way. */
	SDL_VIDEO_screen = SDL_SetVideoMode(w, h, SDL_VIDEO_native_bpp, flags);
	if (SDL_VIDEO_screen == NULL) {
		/* Some SDL_SetVideoMode errors can be averted by reinitialising the SDL video subsystem. */
		Log_print("Setting video mode: %dx%dx%d failed: %s. Reinitialising video.", w, h, SDL_VIDEO_native_bpp, SDL_GetError());
		SDL_VIDEO_ReinitSDL();
		reinit = TRUE;
		SDL_VIDEO_screen = SDL_SetVideoMode(w, h, SDL_VIDEO_native_bpp, flags);
		if (SDL_VIDEO_screen == NULL) {
			Log_print("Setting Video Mode: %dx%dx%d failed: %s", w, h, SDL_VIDEO_native_bpp, SDL_GetError());
			Log_flushlog();
			exit(-1);
		}
	}
	SDL_VIDEO_width = SDL_VIDEO_screen->w;
	SDL_VIDEO_height = SDL_VIDEO_screen->h;
	SDL_VIDEO_vsync_available = FALSE;
#endif
	ModeInfo();
	return reinit;
}

#if SDL2

static GLuint progID = 0;
static GLuint buffers[3];
static GLuint vaos[1];
static GLint our_texture;
static GLint sh_scanlines;
static GLint sh_curvature;
static GLint sh_aPos;
static GLint sh_aTexCoord;
static GLint sh_vp_matrix;
static GLint sh_resolution;
static GLint sh_pixelSpread;
static GLint sh_glow;

#define	TEX_WIDTH	1024
#define	TEX_HEIGHT	512
#define	WIDTH		320
#define	HEIGHT		Screen_HEIGHT

static const int kVertexCount = 4;
static const int kIndexCount = 6;

static const GLfloat vertices[] = {
	-1.0f, -1.0f, 0.0f,
	+1.0f, -1.0f, 0.0f,
	+1.0f, +1.0f, 0.0f,
	-1.0f, +1.0f, 0.0f,
};

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static void SDL_set_up_uvs(void) {
	float min_u	= 0.0f;
	float max_u	= (float)VIDEOMODE_custom_horizontal_area / TEX_WIDTH;
	float min_v	= 0.0f;
	float max_v	= (float)VIDEOMODE_custom_vertical_area / TEX_HEIGHT;
	const GLfloat uvs[] = {
		min_u, min_v,
		max_u, min_v,
		max_u, max_v,
		min_u, max_v,
	};
	gl.BindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	gl.BufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	gl.VertexAttribPointer(sh_aTexCoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
}

static void SDL_set_up_opengl(void) {
	gl.GenTextures(2, textures);
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	// gl.TexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEX_WIDTH, TEX_HEIGHT, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	// gl.BindTexture(GL_TEXTURE_2D, textures[1]); // color palette
	// gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	our_texture = gl.GetUniformLocation(progID, "ourTexture");
	sh_scanlines = gl.GetUniformLocation(progID, "scanlinesFactor");
	sh_curvature = gl.GetUniformLocation(progID, "screenCurvature");
	sh_aPos = gl.GetAttribLocation(progID, "aPos");
	sh_aTexCoord = gl.GetAttribLocation(progID, "aTexCoord");
	sh_vp_matrix = gl.GetUniformLocation(progID, "u_vp_matrix");
	sh_resolution = gl.GetUniformLocation(progID, "u_resolution");
	sh_pixelSpread = gl.GetUniformLocation(progID, "u_pixelSpread");
	sh_glow = gl.GetUniformLocation(progID, "u_glowCoeff");

	gl.GenBuffers(3, buffers);
	gl.GenVertexArrays(1, vaos);
	gl.BindVertexArray(vaos[0]);
	gl.BindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	gl.BufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW);
	gl.VertexAttribPointer(sh_aPos, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);
	gl.EnableVertexAttribArray(sh_aPos);

	SDL_set_up_uvs();
	gl.EnableVertexAttribArray(sh_aTexCoord);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GLushort), indices, GL_STATIC_DRAW);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	gl.Disable(GL_DEPTH_TEST);
	gl.Disable(GL_DITHER);
}

static void SetOrtho(float m[4][4], float scale_x, float scale_y, float angle) {
	memset(m, 0, 4*4*sizeof(float));
	m[2][2] = 1.0f;
	m[3][0] = 0;
	m[3][1] = 0;
	m[3][2] = 0;
	m[3][3] = 1;
	// rotate and scale:
	angle = angle * M_PI / 180;
	double s = sin(angle);
	double c = cos(angle);
	m[0][0] = scale_x * c;
	m[0][1] = scale_x * s;
	m[1][0] = -scale_y * -s;
	m[1][1] = -scale_y * c;
}

static char* read_text_file(const char* path) {
	FILE* fp = fopen(path , "rb");
	if (!fp) return NULL;

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);
	rewind(fp);

	char* buffer = malloc(size + 1);
	if (!buffer) {
		fclose(fp);
		return NULL;
	}

	/* copy the file into the buffer */
	if (fread(buffer, size, 1, fp) != 1) {
		fclose(fp);
		free(buffer);
		return NULL;
	}

	buffer[size] = 0;
	fclose(fp);

	return buffer;
}

#endif /* SDL2 */


int SDL_VIDEO_GL_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90)
{
	int isnew = SDL_VIDEO_screen == NULL; /* TRUE means the SDL/GL screen was not yet initialised */
	int context_updated = FALSE; /* TRUE means the OpenGL context has been recreated */
	currently_rotated = rotate90;

	/* Call SetVideoMode only when there was change in width, height, or windowed/fullscreen. */
	if (isnew || SDL_VIDEO_screen->w != res->width || SDL_VIDEO_screen->h != res->height ||
	    ((SDL_VIDEO_screen->flags & SDL_OpenGL_FULLSCREEN) == SDL_OpenGL_FULLSCREEN) == windowed) {
		if (!isnew) {
			CleanGlContext();
		}
#if HAVE_WINDOWS_H
		if (isnew && !windowed) {
			/* Switching from fullscreen software mode directly to fullscreen OpenGL mode causes
			   glitches on Windows (eg. when switched to windowed mode, the window would spontaneously
			   go back to fullscreen each time it loses and regains focus). We avoid the issue by
			   switching to a windowed non-OpenGL mode inbetween. */
			SDL_SetVideoMode(320, 200, SDL_VIDEO_native_bpp, SDL_RESIZABLE);
		}
#endif /* HAVE_WINDOWS_H */
		if (SetVideoMode(res->width, res->height, windowed))
			/* Reinitialisation happened! Need to recreate GL context. */
			isnew = TRUE;
		if (!InitGlFunctions()) {
			Log_print("Cannot use OpenGL - some functions are not provided.");
			return FALSE;
		}
		if (isnew) {
			GLint tex_size;
			gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, & tex_size);
			if (tex_size < 1024) {
				Log_print("Cannot use OpenGL - Supported texture size is too small (%d).", tex_size);
				return FALSE;
			}
#if SDL2
			// add shaders
#ifdef DEBUG_SHADERS
			GLchar* vertexShader = 
			read_text_file("atari800-shader.vert");
			if (!vertexShader) {
				Log_print("Missing vertex shader file 'atari800-shader.vert'");
				exit(1);
			}
			GLchar* fragmentShader = read_text_file("atari800-shader.frag");
			if (!fragmentShader) {
				Log_print("Missing fragment shader file 'atari800-shader.frag'");
				exit(1);
			}
#else
#include "sdl/gen-atari800-shader.vert.h"
			GLchar* vertexShader = Util_malloc(vertexShaderArr_len + 1);
			strncpy(vertexShader, (char*)vertexShaderArr, vertexShaderArr_len);
			vertexShader[vertexShaderArr_len] = 0;
#include "sdl/gen-atari800-shader.frag.h"
			GLchar* fragmentShader = Util_malloc(fragmentShaderArr_len + 1);
			strncpy(fragmentShader, (char*)fragmentShaderArr, fragmentShaderArr_len);
			fragmentShader[fragmentShaderArr_len] = 0;
#endif
			GLint success = 0;

			int vertex = gl.CreateShader(GL_VERTEX_SHADER);
			gl.ShaderSource(vertex, 1, &vertexShader, NULL);
			gl.CompileShader(vertex);
			gl.GetShaderiv(vertex, GL_COMPILE_STATUS, &success);
			if (!success) {
				char buf[500];
				gl.GetShaderInfoLog(vertex, 500, NULL, buf);
				Log_print("Cannot use OpenGL - error compiling vertex shader: %s", buf);
				exit(1);
			}
			int fragment = gl.CreateShader(GL_FRAGMENT_SHADER);
			gl.ShaderSource(fragment, 1, &fragmentShader, NULL);
			gl.CompileShader(fragment);
			gl.GetShaderiv(fragment, GL_COMPILE_STATUS, &success);
			if (!success) {
				char buf[500];
				gl.GetShaderInfoLog(fragment, 500, NULL, buf);
				Log_print("Cannot use OpenGL - error compiling fragment shader: %s", buf);
				exit(1);
			}

			progID = gl.CreateProgram();
			gl.AttachShader(progID, vertex);
			gl.AttachShader(progID, fragment);
			gl.LinkProgram(progID);
			gl.GetProgramiv(progID, GL_LINK_STATUS, &success);
			if (!success) {
				char buf[500];
				gl.GetProgramInfoLog(progID, 500, NULL, buf);
				Log_print("Cannot use OpenGL - error linking shader program: %s", buf);
				exit(1);
			}

			gl.DeleteShader(vertex);
			gl.DeleteShader(fragment);
			free(vertexShader);
			free(fragmentShader);

			SDL_set_up_opengl();
		}
		else {
			SDL_set_up_uvs();
#endif
		}
		pbo_available = InitGlPbo();
		if (!pbo_available)
			SDL_VIDEO_GL_pbo = FALSE;
		if (isnew) {
			Log_print("OpenGL initialized successfully. Version: %s", gl.GetString(GL_VERSION));
#if !SDL2
			if (pbo_available)
				Log_print("OpenGL Pixel Buffer Objects available.");
			else
				Log_print("OpenGL Pixel Buffer Objects not available.");
#endif
		}
		InitGlContext();
		context_updated = TRUE;
	}

	if (isnew) {
		FreeTexture();
		AllocTexture();
	}

	UpdatePaletteLookup(mode);

	if (context_updated)
		InitGlTextures();

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	if (mode == VIDEOMODE_MODE_NORMAL) {
#ifdef PAL_BLENDING
		if (ARTIFACT_mode == ARTIFACT_PAL_BLEND)
			blit_funcs[0] = &DisplayPalBlending;
		else
#endif /* PAL_BLENDING */
			blit_funcs[0] = &DisplayNormal;
	}

#if SDL2
	gl.Viewport(0, 0, screen_width, screen_height);
#else
	gl.Viewport(VIDEOMODE_dest_offset_left, VIDEOMODE_dest_offset_top, VIDEOMODE_dest_width, VIDEOMODE_dest_height);
	SetSubpixelShifts();
	SetGlDisplayList();
#endif
	CleanDisplayTexture();
	return TRUE;
}

int SDL_VIDEO_GL_SupportsVideomode(VIDEOMODE_MODE_t mode, int stretch, int rotate90)
{
	/* OpenGL supports rotation and stretching in all display modes. */
	return TRUE;
}

static void DisplayNormal(GLvoid *dest)
{
	Uint8 *screen = (Uint8 *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	if (bpp_32)
		SDL_VIDEO_BlitNormal32((Uint32*)dest, screen, VIDEOMODE_actual_width, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp32);
	else {
		int pitch;
		if (VIDEOMODE_actual_width & 0x01)
			pitch = VIDEOMODE_actual_width / 2 + 1;
		else
			pitch = VIDEOMODE_actual_width / 2;
		SDL_VIDEO_BlitNormal16((Uint32*)dest, screen, pitch, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp16);
	}
}

#ifdef PAL_BLENDING
static void DisplayPalBlending(GLvoid *dest)
{
	Uint8 *screen = (Uint8 *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	if (bpp_32)
		PAL_BLENDING_Blit32((ULONG*)dest, screen, VIDEOMODE_actual_width, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_src_offset_top % 2);
	else {
		int pitch;
		if (VIDEOMODE_actual_width & 0x01)
			pitch = VIDEOMODE_actual_width / 2 + 1;
		else
			pitch = VIDEOMODE_actual_width / 2;
		PAL_BLENDING_Blit16((ULONG*)dest, screen, pitch, VIDEOMODE_src_width, VIDEOMODE_src_height, VIDEOMODE_src_offset_top % 2);
	}
}
#endif /* PAL_BLENDING */

#if NTSC_FILTER
static void DisplayNTSCEmu(GLvoid *dest)
{
	(*pixel_formats[SDL_VIDEO_GL_pixel_format].ntsc_blit_func)(
		FILTER_NTSC_emu,
		(ATARI_NTSC_IN_T *) ((UBYTE *)Screen_atari + Screen_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left),
		Screen_WIDTH,
		VIDEOMODE_src_width,
		VIDEOMODE_src_height,
		dest,
		VIDEOMODE_actual_width * (bpp_32 ? 4 : 2));
}
#endif

#ifdef XEP80_EMULATION
static void DisplayXEP80(GLvoid *dest)
{
	static int xep80Frame = 0;
	Uint8 *screen;
	xep80Frame++;
	if (xep80Frame == 60) xep80Frame = 0;
	if (xep80Frame > 29)
		screen = XEP80_screen_1;
	else
		screen = XEP80_screen_2;

	screen += XEP80_SCRN_WIDTH * VIDEOMODE_src_offset_top + VIDEOMODE_src_offset_left;
	if (bpp_32)
		SDL_VIDEO_BlitXEP80_32((Uint32*)dest, screen, VIDEOMODE_actual_width, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp32);
	else
		SDL_VIDEO_BlitXEP80_16((Uint32*)dest, screen, VIDEOMODE_actual_width / 2, VIDEOMODE_src_width, VIDEOMODE_src_height, SDL_PALETTE_buffer.bpp16);
}
#endif

#ifdef PBI_PROTO80
static void DisplayProto80(GLvoid *dest)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	if (bpp_32)
		SDL_VIDEO_BlitProto80_32((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width, first_line, last_line, SDL_PALETTE_buffer.bpp32);
	else
		SDL_VIDEO_BlitProto80_16((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width/2, first_line, last_line, SDL_PALETTE_buffer.bpp16);
}
#endif

#ifdef AF80
static void DisplayAF80(GLvoid *dest)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	static int AF80Frame = 0;
	int blink;
	AF80Frame++;
	if (AF80Frame == 60) AF80Frame = 0;
	blink = AF80Frame >= 30;
	if (bpp_32)
		SDL_VIDEO_BlitAF80_32((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width, first_line, last_line, blink, SDL_PALETTE_buffer.bpp32);
	else
		SDL_VIDEO_BlitAF80_16((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width/2, first_line, last_line, blink, SDL_PALETTE_buffer.bpp16);
}
#endif

#ifdef BIT3
static void DisplayBIT3(GLvoid *dest)
{
	int first_column = (VIDEOMODE_src_offset_left+7) / 8;
	int last_column = (VIDEOMODE_src_offset_left + VIDEOMODE_src_width) / 8;
	int first_line = VIDEOMODE_src_offset_top;
	int last_line = first_line + VIDEOMODE_src_height;
	static int BIT3Frame = 0;
	int blink;
	BIT3Frame++;
	if (BIT3Frame == 60) BIT3Frame = 0;
	blink = BIT3Frame >= 30;
	if (bpp_32)
		SDL_VIDEO_BlitBIT3_32((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width, first_line, last_line, blink, SDL_PALETTE_buffer.bpp32);
	else
		SDL_VIDEO_BlitBIT3_16((Uint32*)dest, first_column, last_column, VIDEOMODE_actual_width/2, first_line, last_line, blink, SDL_PALETTE_buffer.bpp16);
}
#endif


void SDL_VIDEO_GL_DisplayScreen(void)
{
#if SDL2
	if (!screen_width || !screen_height) return;

	gl.Clear(GL_COLOR_BUFFER_BIT);
	gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl.Enable(GL_BLEND);
	gl.Uniform1i(our_texture, 0);
	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	(*blit_funcs[SDL_VIDEO_current_display_mode])(screen_texture);
	gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEOMODE_actual_width, VIDEOMODE_src_height,
		pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
		screen_texture);

	gl.BindVertexArray(vaos[0]);
	float sx = 1.0f, sy = 1.0f;
	float vw = VIDEOMODE_custom_horizontal_area;
	float vh = VIDEOMODE_custom_vertical_area;
	// Screen aspect ratio adjustment
	float a = (float)screen_width / screen_height;
	float a0 = currently_rotated ? vh / vw : vw / vh;
	if (a > a0) {
		sx = a0 / a;
	}
	else {
		sy = a / a0;
	}

	int keep = VIDEOMODE_GetKeepAspect();
	if (keep == VIDEOMODE_KEEP_ASPECT_REAL) {
		float ar = VIDEOMODE_GetPixelAspectRatio(VIDEOMODE_MODE_NORMAL);
		if (ar > 1.0) {
			sy /= ar;
		}
		else {
			sx *= ar;
		}
	}

	float proj[4][4];
	SetOrtho(proj, sx * zoom_factor, sy * zoom_factor, currently_rotated ? 90 : 0);

	gl.UniformMatrix4fv(sh_vp_matrix, 1, GL_FALSE, &proj[0][0]);
	gl.Uniform2f(sh_resolution, vw, vh);

	float scanlinesFactor = SDL_VIDEO_interpolate_scanlines ? SDL_VIDEO_scanlines_percentage / 100.0f : 0.0f;
	gl.Uniform1f(sh_scanlines, scanlinesFactor);
	gl.Uniform1f(sh_curvature, SDL_VIDEO_crt_barrel_distortion / 20.0f);
	gl.Uniform1f(sh_pixelSpread, SDL_VIDEO_crt_beam_shape ? pow((27.0f - SDL_VIDEO_crt_beam_shape) / 20.0f, 2) : 0.0f);
	gl.Uniform1f(sh_glow, SDL_VIDEO_crt_phosphor_glow / 20.0f);

	gl.UseProgram(progID);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	gl.DrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0);
	SDL_GL_SwapWindow(SDL_VIDEO_wnd);

	gl.BindBuffer(GL_ARRAY_BUFFER, 0);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#else
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	if (SDL_VIDEO_GL_pbo) {
		GLvoid *ptr;
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
		ptr = gl.MapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		(*blit_funcs[SDL_VIDEO_current_display_mode])(ptr);
		gl.UnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEOMODE_actual_width, VIDEOMODE_src_height,
		                 pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
		                 NULL);
		gl.BindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	} else {
		(*blit_funcs[SDL_VIDEO_current_display_mode])(screen_texture);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEOMODE_actual_width, VIDEOMODE_src_height,
		                 pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
		                 screen_texture);
	}
	gl.CallList(screen_dlist);
	SDL_GL_SwapBuffers();
#endif /* SDL2 */
}

int SDL_VIDEO_GL_ReadConfig(char *option, char *parameters)
{
	if (strcmp(option, "PIXEL_FORMAT") == 0) {
		int i = CFG_MatchTextParameter(parameters, pixel_format_cfg_strings, SDL_VIDEO_GL_PIXEL_FORMAT_SIZE);
		if (i < 0)
			return FALSE;
		SDL_VIDEO_GL_pixel_format = i;
	}
	else if (strcmp(option, "BILINEAR_FILTERING") == 0)
		return (SDL_VIDEO_GL_filtering = Util_sscanbool(parameters)) != -1;
	else if (strcmp(option, "OPENGL_PBO") == 0)
		return (SDL_VIDEO_GL_pbo = Util_sscanbool(parameters)) != -1;
	else
		return FALSE;
	return TRUE;
}

void SDL_VIDEO_GL_WriteConfig(FILE *fp)
{
	fprintf(fp, "PIXEL_FORMAT=%s\n", pixel_format_cfg_strings[SDL_VIDEO_GL_pixel_format]);
	fprintf(fp, "BILINEAR_FILTERING=%d\n", SDL_VIDEO_GL_filtering);
	fprintf(fp, "OPENGL_PBO=%d\n", SDL_VIDEO_GL_pbo);
}

/* Loads the OpenGL library. Return TRUE on success, FALSE on failure. */
static int InitGlLibrary(void)
{
	if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) != 0) {
		Log_print("Cannot use OpenGL - unable to set GL attribute: %s\n",SDL_GetError());
		return FALSE;
	}
	if (SDL_GL_LoadLibrary(library_path) < 0) {
		Log_print("Cannot use OpenGL - unable to dynamically open OpenGL library: %s\n",SDL_GetError());
		return FALSE;
	}
	return TRUE;
}

void SDL_VIDEO_GL_InitSDL(void)
{
	SDL_VIDEO_opengl_available = InitGlLibrary();
#if SDL2
	// for OpenGL 4.1
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
}

int SDL_VIDEO_GL_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE;           /* error, argument missing! */
		int a_i = FALSE;           /* error, argument invalid! */

		if (strcmp(argv[i], "-pixel-format") == 0) {
			if (i_a) {
				if ((SDL_VIDEO_GL_pixel_format = CFG_MatchTextParameter(argv[++i], pixel_format_cfg_strings, SDL_VIDEO_GL_PIXEL_FORMAT_SIZE)) < 0)
					a_i = TRUE;
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-bilinear-filter") == 0)
			SDL_VIDEO_GL_filtering = TRUE;
		else if (strcmp(argv[i], "-no-bilinear-filter") == 0)
			SDL_VIDEO_GL_filtering = FALSE;
		else if (strcmp(argv[i], "-pbo") == 0)
			SDL_VIDEO_GL_pbo = TRUE;
		else if (strcmp(argv[i], "-no-pbo") == 0)
			SDL_VIDEO_GL_pbo = FALSE;
		else if (strcmp(argv[i], "-opengl-lib") == 0) {
			if (i_a)
				library_path = argv[++i];
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-pixel-format bgr16|rgb16|bgra32|argb32");
				Log_print("\t                     Set internal pixel format (affects performance)");
				Log_print("\t-bilinear-filter     Enable OpenGL bilinear filtering");
				Log_print("\t-no-bilinear-filter  Disable OpenGL bilinear filtering");
				Log_print("\t-pbo                 Use OpenGL Pixel Buffer Objects if available");
				Log_print("\t-no-pbo              Don't use OpenGL Pixel Buffer Objects");
				Log_print("\t-opengl-lib <path>   Use a custom OpenGL shared library");
			}
			argv[j++] = argv[i];
		}
		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
	}
	*argc = j;

	if (help_only)
		return TRUE;

	bpp_32 = SDL_VIDEO_GL_pixel_format >= SDL_VIDEO_GL_PIXEL_FORMAT_BGRA32;

	return TRUE;
}

void SDL_VIDEO_GL_SetPixelFormat(int value)
{
	SDL_VIDEO_GL_pixel_format = value;
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG) {
		int new_bpp_32 = value >= SDL_VIDEO_GL_PIXEL_FORMAT_BGRA32;
		if (new_bpp_32 != bpp_32)
		{
			FreeTexture();
			bpp_32 = new_bpp_32;
			AllocTexture();
		}
		UpdatePaletteLookup(SDL_VIDEO_current_display_mode);
		InitGlTextures();
		CleanDisplayTexture();
	}
	else
		bpp_32 = value;

}

void SDL_VIDEO_GL_TogglePixelFormat(void)
{
	SDL_VIDEO_GL_SetPixelFormat((SDL_VIDEO_GL_pixel_format + 1) % SDL_VIDEO_GL_PIXEL_FORMAT_SIZE);
}

void SDL_VIDEO_GL_SetFiltering(int value)
{
	SDL_VIDEO_GL_filtering = value;
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG) {
		GLint filtering = value ? GL_LINEAR : GL_NEAREST;
		gl.BindTexture(GL_TEXTURE_2D, textures[0]);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
		SetSubpixelShifts();
		SetGlDisplayList();
	}
}

void SDL_VIDEO_GL_ToggleFiltering(void)
{
	SDL_VIDEO_GL_SetFiltering(!SDL_VIDEO_GL_filtering);
}

int SDL_VIDEO_GL_SetPbo(int value)
{
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG) {
		/* Return false if PBOs are requested but not available. */
		if (value && !pbo_available)
			return FALSE;
		CleanGlContext();
		FreeTexture();
		SDL_VIDEO_GL_pbo = value;
		InitGlContext();
		AllocTexture();
		InitGlTextures();
		SetGlDisplayList();
		CleanDisplayTexture();
	}
	else
		SDL_VIDEO_GL_pbo = value;
	return TRUE;
}

int SDL_VIDEO_GL_TogglePbo(void)
{
	return SDL_VIDEO_GL_SetPbo(!SDL_VIDEO_GL_pbo);
}

void SDL_VIDEO_GL_ScanlinesPercentageChanged(void)
{
#if SDL2
	SDL_VIDEO_GL_DisplayScreen();
 #else
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG) {
		SetSubpixelShifts();
		SetGlDisplayList();
	}
#endif
}

void SDL_VIDEO_GL_InterpolateScanlinesChanged(void)
{
#if SDL2
	SDL_VIDEO_GL_DisplayScreen();
 #else
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OpenGL_FLAG) == SDL_OpenGL_FLAG) {
		GLint filtering = SDL_VIDEO_interpolate_scanlines ? GL_LINEAR : GL_NEAREST;
		gl.BindTexture(GL_TEXTURE_2D, textures[1]);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
		SetSubpixelShifts();
		SetGlDisplayList();
	}
#endif
}
