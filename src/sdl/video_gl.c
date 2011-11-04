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

#include "af80.h"
#include "atari.h"
#include "cfg.h"
#include "colours.h"
#include "config.h"
#include "filter_ntsc.h"
#include "log.h"
#include "pbi_proto80.h"
#include "platform.h"
#include "screen.h"
#include "videomode.h"
#include "xep80.h"
#include "xep80_fonts.h"
#include "util.h"

#include "sdl/palette.h"
#include "sdl/video.h"
#include "sdl/video_gl.h"

static int currently_rotated = FALSE;
/* If TRUE, then 32 bit, else 16 bit screen. */
static int bpp_32 = FALSE;

int SDL_VIDEO_GL_filtering = 0;
int SDL_VIDEO_GL_pixel_format = SDL_VIDEO_GL_PIXEL_FORMAT_BGR16;

/* Path to the OpenGL shared library. */
static char const *library_path = NULL;

/* Pointers to OpenGL functions, loaded dynamically during initialisation. */
struct
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
	void(APIENTRY*GenBuffers)(GLsizei, GLuint*);
	void(APIENTRY*DeleteBuffers)(GLsizei, const GLuint*);
	void(APIENTRY*BindBuffer)(GLenum, GLuint);
	void(APIENTRY*BufferData)(GLenum, GLsizeiptr, const GLvoid*, GLenum);
	void*(APIENTRY*MapBuffer)(GLenum, GLenum);
	GLboolean(APIENTRY*UnmapBuffer)(GLenum);
} gl;

static void DisplayNormal(GLvoid *dest);
static void DisplayNTSCEmu(GLvoid *dest);
static void DisplayXEP80(GLvoid *dest);
static void DisplayProto80(GLvoid *dest);
static void DisplayAF80(GLvoid *dest);

static void (* const blit_funcs[VIDEOMODE_MODE_SIZE])(GLvoid *) = {
	&DisplayNormal,
	&DisplayNTSCEmu,
	&DisplayXEP80,
	&DisplayProto80,
	&DisplayAF80
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
	void(*calc_pal_func)(VIDEOMODE_MODE_t mode);
	void(*ntsc_blit_func)(atari_ntsc_t const*, ATARI_NTSC_IN_T const*, long, int, int, void*, long);
} pixel_format_t;

pixel_format_t const pixel_formats[4] = {
	{ GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV, 0x0000, &SDL_PALETTE_Calculate16_B5G6R5, &atari_ntsc_blit_bgr16 }, 
	{ GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0x0000, &SDL_PALETTE_Calculate16_R5G6B5, &atari_ntsc_blit_rgb16 }, /* NVIDIA 16-bit */
	{ GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, 0xff000000, &SDL_PALETTE_Calculate32_B8G8R8A8, &atari_ntsc_blit_bgra32 },
	{ GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, 0xff000000, &SDL_PALETTE_Calculate32_A8R8G8B8, &atari_ntsc_blit_argb32 } /* NVIDIA 32-bit */
};

/* Conversion between function pointers and 'void *' is forbidden in
   ISO C, but unfortunately unavoidable when using SDL_GL_GetProcAddress.
   So the code below is non-portable and gives a warning with gcc -ansi
   -Wall -pedantic, but is the only possible solution. */
static void (*GetGlFunc(const char* s))(void)
{
	void(*f)(void) = SDL_GL_GetProcAddress(s);
	if (f == NULL)
		Log_print("Unable to get function pointer for %s\n",s);
	return f;
}

/* Alocates memory for the screen texture, if needed. */
static void AllocTexture(void)
{
	if (!SDL_VIDEO_GL_pbo && screen_texture == NULL)
		/* The largest width is in NTSC-filtered full overscan mode - 672 pixels.
		   The largest height is in XEP-80 mode - 275 pixels. Add 1 pixel at each side
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
		gl.GenBuffers(1, &screen_pbo);
}

/* Cleans up the structures allocated in InitGlContext. */
static void CleanGlContext(void)
{
		if (SDL_VIDEO_GL_pbo)
			gl.DeleteBuffers(1, &screen_pbo);
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
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
		gl.BufferData(GL_PIXEL_UNPACK_BUFFER_ARB, 1024*512*(bpp_32 ? sizeof(Uint32) : sizeof(Uint16)), NULL, GL_DYNAMIC_DRAW_ARB);
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	}
}

void SDL_VIDEO_GL_Cleanup(void)
{
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL)
		CleanGlContext();
	FreeTexture();
}

/* Calculate the palette in the 32-bit BGRA format, or 16-bit BGR 5-6-5 format. */
static void CalcPalette(VIDEOMODE_MODE_t mode)
{
	if (mode != VIDEOMODE_MODE_NTSC_FILTER)
		(*pixel_formats[SDL_VIDEO_GL_pixel_format].calc_pal_func)(mode);
}

void SDL_VIDEO_GL_PaletteUpdate(void)
{
	CalcPalette(SDL_VIDEO_current_display_mode);
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
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
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
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
}

/* Sets pointers to OpenGL functions. Returns TRUE on success, FALSE on failure. */
static int InitGlFunctions(void)
{
	if ((gl.Viewport = (void(APIENTRY*)(GLint,GLint,GLsizei,GLsizei))GetGlFunc("glViewport")) == NULL ||
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

/* Checks availability of Pixel Buffer Objests extension and sets pointers of PBO-related OpenGL functions.
   Returns TRUE on success, FALSE on failure. */
static int InitGlPbo(void)
{
	const GLubyte *extensions = gl.GetString(GL_EXTENSIONS);
	if (!strstr((char *)extensions, "EXT_pixel_buffer_object")) {
		return FALSE;
	}
	if ((gl.GenBuffers = (void(APIENTRY*)(GLsizei, GLuint*))GetGlFunc("glGenBuffersARB")) == NULL ||
	    (gl.DeleteBuffers = (void(APIENTRY*)(GLsizei, const GLuint*))GetGlFunc("glDeleteBuffersARB")) == NULL ||
	    (gl.BindBuffer = (void(APIENTRY*)(GLenum, GLuint))GetGlFunc("glBindBufferARB")) == NULL ||
	    (gl.BufferData = (void(APIENTRY*)(GLenum, GLsizeiptr, const GLvoid*, GLenum))GetGlFunc("glBufferDataARB")) == NULL ||
	    (gl.MapBuffer = (void*(APIENTRY*)(GLenum, GLenum))GetGlFunc("glMapBufferARB")) == NULL ||
	    (gl.UnmapBuffer = (GLboolean(APIENTRY*)(GLenum))GetGlFunc("glUnmapBufferARB")) == NULL)
		return FALSE;

	return TRUE;
}

static void ModeInfo(void)
{
	char *fullstring = (SDL_VIDEO_screen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN ? "fullscreen" : "windowed";
	Log_print("Video Mode: %dx%dx%d %s, pixel format: %s", SDL_VIDEO_screen->w, SDL_VIDEO_screen->h,
		   SDL_VIDEO_screen->format->BitsPerPixel, fullstring, pixel_format_cfg_strings[SDL_VIDEO_GL_pixel_format]);
}

/* Return value of TRUE indicates that the video subsystem was reinitialised. */
static int SetVideoMode(int w, int h, int windowed)
{
	int reinit = FALSE;
	Uint32 flags = SDL_OPENGL | (windowed ? SDL_RESIZABLE : SDL_FULLSCREEN);
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
	ModeInfo();
	return reinit;
}

int SDL_VIDEO_GL_SetVideoMode(VIDEOMODE_resolution_t const *res, int windowed, VIDEOMODE_MODE_t mode, int rotate90)
{
	int new = SDL_VIDEO_screen == NULL; /* TRUE means the SDL/GL screen was not yet initialised */
	int context_updated = FALSE; /* TRUE means the OpenGL context has been recreated */
	currently_rotated = rotate90;

	/* Call SetVideoMode only when there was change in width, height, or windowed/fullscreen. */
	if (new || SDL_VIDEO_screen->w != res->width || SDL_VIDEO_screen->h != res->height ||
	    ((SDL_VIDEO_screen->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN) == windowed) {
		if (!new) {
			CleanGlContext();
		}
#if HAVE_WINDOWS_H
		if (new && !windowed) {
			/* Switching from fullscreen software mode directly to fullscreen OpenGL mode causes
			   glitches on Windows (eg. when switched to windowed mode, the window would spontaneously
			   go back to fullscreen each time it loses and regains focus). We avoid the issue by
			   switching to a windowed non-OpenGL mode inbetween. */
			SDL_SetVideoMode(320, 200, SDL_VIDEO_native_bpp, SDL_RESIZABLE);
		}
#endif /* HAVE_WINDOWS_H */
		if (SetVideoMode(res->width, res->height, windowed))
			/* Reinitialisation happened! Need to recreate GL context. */
			new = TRUE;
		if (!InitGlFunctions()) {
			Log_print("Cannot use OpenGL - some functions are not provided.");
			return FALSE;
		}
		if (new) {
			GLint tex_size;
			gl.GetIntegerv(GL_MAX_TEXTURE_SIZE, & tex_size);
			if (tex_size < 1024) {
				Log_print("Cannot use OpenGL - Supported texture size is too small (%d).", tex_size);
				return FALSE;
			}
		}
		pbo_available = InitGlPbo();
		if (!pbo_available)
			SDL_VIDEO_GL_pbo = FALSE;
		if (new) {
			Log_print("OpenGL initialized successfully. Version: %s", gl.GetString(GL_VERSION));
			if (pbo_available)
				Log_print("OpenGL Pixel Buffer Objects available.");
			else
			Log_print("OpenGL Pixel Buffer Objects not available.");
		}
		InitGlContext();
		context_updated = TRUE;
	}

	if (new) {
		FreeTexture();
		AllocTexture();
	}
	
	if (new
	    || SDL_PALETTE_tab[mode].palette != SDL_PALETTE_tab[SDL_VIDEO_current_display_mode].palette)
		CalcPalette(mode);

	if (context_updated)
		InitGlTextures();

	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */
	gl.Viewport(VIDEOMODE_dest_offset_left, VIDEOMODE_dest_offset_top, VIDEOMODE_dest_width, VIDEOMODE_dest_height);
	SetSubpixelShifts();
	SetGlDisplayList();
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

void SDL_VIDEO_GL_DisplayScreen(void)
{
	gl.BindTexture(GL_TEXTURE_2D, textures[0]);
	if (SDL_VIDEO_GL_pbo) {
		GLvoid *ptr;
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, screen_pbo);
		ptr = gl.MapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		(*blit_funcs[SDL_VIDEO_current_display_mode])(ptr);
		gl.UnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEOMODE_actual_width, VIDEOMODE_src_height,
		                 pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
		                 NULL);
		gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	} else {
		(*blit_funcs[SDL_VIDEO_current_display_mode])(screen_texture);
		gl.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEOMODE_actual_width, VIDEOMODE_src_height,
		                 pixel_formats[SDL_VIDEO_GL_pixel_format].format, pixel_formats[SDL_VIDEO_GL_pixel_format].type,
		                 screen_texture);
	}
	gl.CallList(screen_dlist);
	SDL_GL_SwapBuffers();
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
				Log_print("\t-bilitear-filter     Enable OpenGL bilinear filtering");
				Log_print("\t-no-bilitear-filter  Disable OpenGL bilinear filtering");
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
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL) {
		int new_bpp_32 = value >= SDL_VIDEO_GL_PIXEL_FORMAT_BGRA32;
		if (new_bpp_32 != bpp_32)
		{
			FreeTexture();
			bpp_32 = new_bpp_32;
			AllocTexture();
		}
		CalcPalette(SDL_VIDEO_current_display_mode);
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
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL) {
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
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL) {
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
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL) {
		SetSubpixelShifts();
		SetGlDisplayList();
	}
}

void SDL_VIDEO_GL_InterpolateScanlinesChanged(void)
{
	if (SDL_VIDEO_screen != NULL && (SDL_VIDEO_screen->flags & SDL_OPENGL) == SDL_OPENGL) {
		GLint filtering = SDL_VIDEO_interpolate_scanlines ? GL_LINEAR : GL_NEAREST;
		gl.BindTexture(GL_TEXTURE_2D, textures[1]);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
		gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
		SetSubpixelShifts();
		SetGlDisplayList();
	}
}
