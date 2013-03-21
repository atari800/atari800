/*
 * atari_rpi.c - Raspberry Pi support by djdron
 *
 * Copyright (c) 2013 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * This file is based on Portable ZX-Spectrum emulator.
 * Copyright (C) 2001-2012 SMT, Dexus, Alone Coder, deathsoft, djdron, scor
 *
 * Atari800 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <bcm_host.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <SDL.h>
#include <assert.h>
#include "config.h"
#include "atari.h"
#include "atari_ntsc/atari_ntsc.h"
#include "util.h"

void gles2_create();
void gles2_destroy();
void gles2_draw(int width, int height);
void gles2_palette_changed();

static uint32_t screen_width = 0;
static uint32_t screen_height = 0;
static EGLDisplay display = NULL;
static EGLSurface surface = NULL;
static EGLContext context = NULL;
static EGL_DISPMANX_WINDOW_T nativewindow;
static SDL_Surface* screen = NULL;

extern int op_filtering;
extern float op_zoom;

int SDL_VIDEO_ReadConfig(char *option, char *parameters)
{
	if(strcmp(option, "VIDEO_FILTERING") == 0)
	{
		int v = Util_sscanbool(parameters);
		if(v >= 0)
		{
			op_filtering = v;
			return TRUE;
		}
	}
	else if(strcmp(option, "VIDEO_ZOOM") == 0)
	{
		double z = 1.0f;
		if(Util_sscandouble(parameters, &z))
		{
			op_zoom = z;
			return TRUE;
		}
	}
	return FALSE;
}

void SDL_VIDEO_WriteConfig(FILE *fp)
{
	fprintf(fp, "VIDEO_FILTERING=%d\n", op_filtering);
	fprintf(fp, "VIDEO_ZOOM=%.2f\n", op_zoom);
}

// some dummies (unresolved externals from sdl/video.c, sdl/input.c)
int VIDEOMODE_Update() { return TRUE; }
int VIDEOMODE_SetWindowSize(unsigned int width, unsigned int height) { return TRUE; }
int VIDEOMODE_ToggleWindowed() { return TRUE; }
int VIDEOMODE_ToggleHorizontalArea() { return TRUE; }
int VIDEOMODE_Toggle80Column() { return TRUE; }

void SDL_VIDEO_SetScanlinesPercentage(int value) {}
int SDL_VIDEO_scanlines_percentage = 5;
int SDL_VIDEO_width = 1;
int SDL_VIDEO_height = 1;
atari_ntsc_t *FILTER_NTSC_emu = NULL;
atari_ntsc_setup_t FILTER_NTSC_setup;
void FILTER_NTSC_Update(atari_ntsc_t *filter) {}
void FILTER_NTSC_NextPreset() {}

void PLATFORM_PaletteUpdate() { gles2_palette_changed(); }
void SDL_VIDEO_InitSDL();

int SDL_VIDEO_Initialise(int *argc, char *argv[])
{
	bcm_host_init();
	SDL_VIDEO_InitSDL();
	return 1;
}

void SDL_VIDEO_InitSDL()
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_WM_SetCaption(Atari800_TITLE, "Atari800");
	SDL_EnableUNICODE(1);
	SDL_ShowCursor(SDL_DISABLE);	/* hide mouse cursor */

	screen = SDL_SetVideoMode(0,0, 32, SDL_SWSURFACE); // hack to make keyboard events work

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(display != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	EGLBoolean result = eglInitialize(display, NULL, NULL);
	assert(EGL_FALSE != result);

	// get an appropriate EGL frame buffer configuration
	EGLint num_config;
	EGLConfig config;
	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};
	result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);

	result = eglBindAPI(EGL_OPENGL_ES_API);
	assert(EGL_FALSE != result);

	// create an EGL rendering context
	static const EGLint context_attributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
	assert(context != EGL_NO_CONTEXT);

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0, &screen_width, &screen_height);
	assert(success >= 0);

	VC_RECT_T dst_rect;
	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = screen_width;
	dst_rect.height = screen_height;

	VC_RECT_T src_rect;
	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = screen_width << 16;
	src_rect.height = screen_height << 16;

	DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0);
	DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);
	DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
	  0, &dst_rect, 0, &src_rect, DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_NO_ROTATE);

	nativewindow.element = dispman_element;
	nativewindow.width = screen_width;
	nativewindow.height = screen_height;
	vc_dispmanx_update_submit_sync(dispman_update);

	surface = eglCreateWindowSurface(display, config, &nativewindow, NULL);
	assert(surface != EGL_NO_SURFACE);

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	assert(EGL_FALSE != result);

	gles2_create();
}

void SDL_VIDEO_Exit()
{
	if(screen)
	{
		SDL_FreeSurface(screen);
		screen = NULL;
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	gles2_destroy();
	// Release OpenGL resources
	eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	eglDestroySurface( display, surface );
	eglDestroyContext( display, context );
	eglTerminate( display );
	bcm_host_deinit();
}

void PLATFORM_DisplayScreen()
{
	gles2_draw(screen_width, screen_height);
	eglSwapBuffers(display, surface);
}

