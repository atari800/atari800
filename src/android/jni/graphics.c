/*
 * graphics.c - android drawing
 *
 * Copyright (C) 2010 Kostas Nakos
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

#include <malloc.h>
#include <string.h>
#include <GLES/gl.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GLES/glext.h>

#include "atari.h"
#include "screen.h"
#include "colours.h"
#include "akey.h"
#include "cpu.h"
#include "log.h"

#include "androidinput.h"
#include "graphics.h"

#define TEXTURE_WIDTH  512
#define TEXTURE_HEIGHT 256

#define OPACITY_CUTOFF 0.05f
#define OPACITY_STEP   0.02f
#define OPACITY_FRMSTR 75

#define BORDER_PCT 0.05f

int Android_ScreenW = 0;
int Android_ScreenH = 0;
int Android_Aspect;
int Android_CropScreen[] = {0, SCREEN_HEIGHT, SCANLINE_LEN, -SCREEN_HEIGHT};
int Android_PortPad;
int Android_CovlHold;
static struct RECT screenrect;
static int screenclear;
int Android_Bilinear;
float Android_Joyscale = 0.15f;

extern int *ovl_texpix;
extern int ovl_texw;
extern int ovl_texh;

/* graphics conversion */
static UWORD *palette = NULL;
static UWORD *hicolor_screen = NULL;

/* standard gl textures */
enum {
	TEX_SCREEN = 0,
	TEX_OVL,
	TEX_MAXNAMES
};
static GLuint texture[TEX_MAXNAMES];
static UWORD conkey_vrt[CONK_VERT_MAX];
static struct {
	int x;
	int y;
	int w;
	int h;
} conkey_lbl[CONK_VERT_MAX / 8];
static UWORD conkey_shadow[2 * 4];

void Android_PaletteUpdate(void)
{
	int i;

	if (!palette) {
		if ( !(palette = malloc(256 * sizeof(UWORD))) ) {
			Log_print("Cannot allocate memory for palette conversion.");
			return;
		}
	}
	memset(palette, 0, 256 * sizeof(UWORD));

	for (i = 0; i < 256; i++)
		palette[i] = ( (Colours_GetR(i) & 0xf8) << 8 ) |
					 ( (Colours_GetG(i) & 0xfc) << 3 ) |
					 ( (Colours_GetB(i) & 0xf8) >> 3 );
	/* force full redraw */
	Screen_EntireDirty();
}

int Android_InitGraphics(void)
{
	const UWORD poly[] = { 0,16, 24,16, 32,0, 8,0 };
	int i, tmp, w, h;
	float tmp2, tmp3;
	struct RECT *r;

	/* Allocate stuff */
	if (!hicolor_screen) {
		if ( !(hicolor_screen = malloc(TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(UWORD))) ) {
			Log_print("Cannot allocate memory for hicolor screen.");
			return FALSE;
		}
	}
	memset(hicolor_screen, 0, TEXTURE_WIDTH * TEXTURE_HEIGHT * sizeof(UWORD));

	/* Setup GL */
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glGenTextures(TEX_MAXNAMES, texture);
	glPixelStorei(GL_PACK_ALIGNMENT, 8);

	/* overlays texture */
	glBindTexture(GL_TEXTURE_2D, texture[TEX_OVL]);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ovl_texw, ovl_texh, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, ovl_texpix);

	/* playfield texture */
	glBindTexture(GL_TEXTURE_2D, texture[TEX_SCREEN]);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
					Android_Bilinear ? GL_LINEAR : GL_NEAREST);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					Android_Bilinear ? GL_LINEAR : GL_NEAREST);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterx(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, Android_CropScreen);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_RGB,
				 GL_UNSIGNED_SHORT_5_6_5, hicolor_screen);

	/* Setup view for console key polygons */
	glLoadIdentity();
	glOrthof(0, Android_ScreenW, Android_ScreenH, 0, 0, 1);
	glEnableClientState(GL_VERTEX_ARRAY);

	glClear(GL_COLOR_BUFFER_BIT);
	/* Finsh GL init with an error check */
	if (glGetError() != GL_NO_ERROR) {
		Log_print("Cannot initialize OpenGL");
		return FALSE;
	}

	/* Console keys' polygons */
	tmp2 = ((float) (Android_ScreenW >> 1)) / ((float) 4.5f * poly[4]);
	tmp3 = ((float) Android_ScreenH) / ((float) 14 * poly[1]);
	if (tmp2 > tmp3)
		tmp2 = tmp3;
	if (tmp2 < 2.0f)
		tmp2 = 2.0f;
	for (i = 0; i < CONK_VERT_MAX; i += 2) {
		/* generate & scale */
		conkey_vrt[i    ] = poly[i % 8] * tmp2 +
							((i > 7) ? (conkey_vrt[(i / 8 - 1) * 8 + 2] + 4) : 0);
		conkey_vrt[i + 1] = poly[(i + 1) % 8] * tmp2;
	}
	tmp = Android_ScreenW - conkey_vrt[CONK_VERT_MAX - 4];
	for (i = 0; i < CONK_VERT_MAX; i += 2) {
		/* translate */
		conkey_vrt[i    ] += tmp;
		conkey_vrt[i + 1] += (4 + ((Android_ScreenW < Android_ScreenH) ? Android_PortPad : 0));
	}

	/* Determine location of console keys' labels. */
	{
		int key_w = conkey_vrt[2] - conkey_vrt[0];
		int key_h = conkey_vrt[3] - conkey_vrt[5];
		int key_h_slant = conkey_vrt[6] - conkey_vrt[0];
		int label_w = key_w * key_h * 4 / (key_h_slant + key_h*4);
		int label_h = label_w / 4;
		for (i = 0; i < CONK_NUM; ++i) {
			conkey_lbl[i].x = conkey_vrt[i*8 + 6];
			conkey_lbl[i].y = Android_ScreenH - (conkey_vrt[i*8 + 7] + label_h + 1);
			conkey_lbl[i].w = label_w;
			conkey_lbl[i].h = label_h;
		}
	}

	AndroidInput_ConOvl.keycoo = conkey_vrt;
	AndroidInput_ConOvl.bbox.l = conkey_vrt[0];
	AndroidInput_ConOvl.bbox.b = conkey_vrt[1];
	AndroidInput_ConOvl.bbox.r = conkey_vrt[CONK_VERT_MAX - 4];
	AndroidInput_ConOvl.bbox.t = conkey_vrt[CONK_VERT_MAX - 3];
	AndroidInput_ConOvl.hotlen = 0.1f *
			(Android_ScreenW < Android_ScreenH ? Android_ScreenW : Android_ScreenH);
	r = &(AndroidInput_ConOvl.bbox);
	conkey_shadow[0] = r->l - COVL_SHADOW_OFF;
	conkey_shadow[1] = r->b + COVL_SHADOW_OFF;
	conkey_shadow[2] = r->r;
	conkey_shadow[3] = r->b + COVL_SHADOW_OFF;
	conkey_shadow[4] = r->r;
	conkey_shadow[5] = r->t - COVL_SHADOW_OFF;
	conkey_shadow[6] = r->l - COVL_SHADOW_OFF;
	conkey_shadow[7] = r->t - COVL_SHADOW_OFF;

	/* Scale joystick overlays */
	Joyovl_Scale();
	Joy_Reposition();

	/* Aspect correct scaling */
	memset(&screenrect, 0, sizeof(struct RECT));
	if ( ((Android_ScreenW > Android_ScreenH) + 1) & Android_Aspect) {
		w = Android_CropScreen[2];
		h = -Android_CropScreen[3];
		/* fit horizontally */
		tmp2 = ((float) Android_ScreenW) / ((float) w);
		screenrect.h = tmp2 * h;
		if (screenrect.h > Android_ScreenH) {
			/* fit vertically */
			tmp2 = ((float) Android_ScreenH) / ((float) h);
			screenrect.h = Android_ScreenH;
		}
		screenrect.w = tmp2 * w;
		/* center */
		tmp = (Android_ScreenW - screenrect.r + 1) / 2;
		screenrect.l += tmp;
		h = Android_ScreenH;
		if (Android_ScreenH > Android_ScreenW)
			h >>= 1;	/* assume keyboard takes up half the height in portrait */
		tmp = (h - screenrect.b + 1) / 2;
		if (tmp < 0)
			tmp = 0;
		tmp = (Android_ScreenH - h) + tmp - ((Android_ScreenW < Android_ScreenH) ? Android_PortPad : 0);
		screenrect.t += tmp;
		screenclear = TRUE;
	} else {
		screenrect.t = screenrect.l = 0;
		screenrect.w = Android_ScreenW;
		screenrect.h = Android_ScreenH;
		screenclear = FALSE;
	}

	/* Initialize palette */
	Android_PaletteUpdate();

	return TRUE;
}

void Joyovl_Scale(void)
{
	int tmp;

	tmp = ( (Android_ScreenW > Android_ScreenH) ?
			 Android_ScreenW : Android_ScreenH ) * Android_Joyscale;
	if (!Android_Paddle) {
		AndroidInput_JoyOvl.joyarea.r = AndroidInput_JoyOvl.joyarea.l + tmp;
		AndroidInput_JoyOvl.joyarea.b = AndroidInput_JoyOvl.joyarea.t + tmp;
	} else {
		if (!Android_PlanetaryDefense) {
			AndroidInput_JoyOvl.joyarea.l = Android_Joyleft ? BORDER_PCT * Android_ScreenW : Android_Split;
			AndroidInput_JoyOvl.joyarea.r = Android_Joyleft ? Android_Split : (1.0f - BORDER_PCT) * Android_ScreenW;
			AndroidInput_JoyOvl.joyarea.b = AndroidInput_JoyOvl.joyarea.t + 8 + (tmp >> 3);
		} else {
		AndroidInput_JoyOvl.joyarea.l = AndroidInput_JoyOvl.joyarea.t = 0;
		AndroidInput_JoyOvl.joyarea.r = Android_ScreenW;
		AndroidInput_JoyOvl.joyarea.b = Android_ScreenH;
		}
	}
	AndroidInput_JoyOvl.firewid = tmp >> 3;
}

void Android_ConvertScreen(void)
{
	int x, y;
	UBYTE *src, *src_line;
	UWORD *dst, *dst_line;
#ifdef DIRTYRECT
	UBYTE *dirty, *dirty_line;
#endif

#ifdef DIRTYRECT
	dirty_line = Screen_dirty + SCANLINE_START / 8;
#endif
	src_line = ((UBYTE *) Screen_atari) + SCANLINE_START;
	dst_line = hicolor_screen;

	for (y = 0; y < SCREEN_HEIGHT; y++) {
#ifdef DIRTYRECT
		dirty = dirty_line;
#else
		src = src_line;
		dst = dst_line;
#endif
		for (x = 0; x < SCANLINE_LEN; x += 8) {
#ifdef DIRTYRECT
			if (*dirty) {
				src = src_line + x;
				dst = dst_line + x;
				do {
#endif
					*dst++ = palette[*src++]; *dst++ = palette[*src++];
					*dst++ = palette[*src++]; *dst++ = palette[*src++];
					*dst++ = palette[*src++]; *dst++ = palette[*src++];
					*dst++ = palette[*src++]; *dst++ = palette[*src++];
#ifdef DIRTYRECT
					*dirty++ = 0;
					x += 8;
				} while (*dirty && x < SCANLINE_LEN);
			}
			dirty++;
#endif
		}
#ifdef DIRTYRECT
		dirty_line += SCREEN_WIDTH / 8;
#endif
		src_line += SCREEN_WIDTH;
		dst_line += SCANLINE_LEN;
	}
}

void Android_Render(void)
{
	const static int crop_joy[] = {0, 0, 64, 64};
	const static int crop_fire[] = {65, 0, 16, 15};
	const static int crop_lbl[][4] = { 	{65, 64, 40, -9},
   										{65, 24, 40, -9},
										{65, 34, 40, -9},
										{65, 44, 40, -9},
										{65, 54, 40, -9}  };
	const static int crop_all[] = {0, 64, 128, -64};
	const struct RECT *r;
	const struct POINT *p;
	int i;

	if (screenclear)
		glClear(GL_COLOR_BUFFER_BIT);

	/* --------------------- playfield --------------------- */
	glBindTexture(GL_TEXTURE_2D, texture[TEX_SCREEN]);
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCANLINE_LEN, SCREEN_HEIGHT, GL_RGB,
					GL_UNSIGNED_SHORT_5_6_5, hicolor_screen);
	r = &screenrect;
	glDrawTexiOES(r->l, r->t, 0, r->w, r->h);
	if (glGetError() != GL_NO_ERROR) Log_print("OpenGL error at playfield");

	/* --------------------- overlays --------------------- */
	glEnable(GL_BLEND);			/* enable blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvx(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, texture[TEX_OVL]);

	if (!AndroidInput_JoyOvl.ovl_visible) goto ck;	/*!*/

	/* joystick area */
	glColor4f(1.0f, 1.0f, 1.0f, AndroidInput_JoyOvl.areaopacitycur);
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop_joy);
	r = &AndroidInput_JoyOvl.joyarea;
	glDrawTexiOES(r->l, Android_ScreenH - r->b, 0, r->r - r->l, r->b - r->t);
	if (glGetError() != GL_NO_ERROR) Log_print("OpenGL error at joy area");

	/* stick */
	if (AndroidInput_JoyOvl.stickopacity >= OPACITY_CUTOFF) {
		glColor4f(1.0f, 1.0f, 1.0f, AndroidInput_JoyOvl.stickopacity);
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop_fire);
		p = &AndroidInput_JoyOvl.joystick;
		i = AndroidInput_JoyOvl.firewid;
		glDrawTexiOES(p->x - i, Android_ScreenH - (p->y + i), 0, i << 1, i << 1);
		if (glGetError() != GL_NO_ERROR) Log_print("OpenGL error at stick");
	}

	/* fire */
	if (AndroidInput_JoyOvl.fireopacity >= OPACITY_CUTOFF) {
		glColor4f(1.0f, 1.0f, 1.0f, AndroidInput_JoyOvl.fireopacity);
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop_fire);
		p = &AndroidInput_JoyOvl.fire;
		i = AndroidInput_JoyOvl.firewid;
		glDrawTexiOES(p->x - i, Android_ScreenH - (p->y + i), 0, i << 1, i << 1);
		if (glGetError() != GL_NO_ERROR) Log_print("OpenGL error at fire");
	}
/*		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop_all);
		glDrawTexiOES(0, 0, 0, 128, 64);
*/

	/* console keys */
ck:	if (AndroidInput_ConOvl.ovl_visible) {
		glDisable(GL_TEXTURE_2D);	/* disable texturing */

		glColor4f(0.0f, 0.0f, 0.0f, AndroidInput_ConOvl.opacity * 0.7);
		glVertexPointer(2, GL_SHORT, 0, conkey_shadow);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		glColor4f(0.5f, 0.9f, 1.0f, AndroidInput_ConOvl.opacity);
		glVertexPointer(2, GL_SHORT, 0, conkey_vrt);
		for (i = 0; i < (CONK_VERT_MAX >> 1); i += 4)
			glDrawArrays(GL_LINE_LOOP, i, 4);
		if (AndroidInput_ConOvl.hitkey >= 0) {
			glColor4f(0.34f, 0.67f, 1.0f, AndroidInput_ConOvl.opacity);
			glDrawArrays(GL_TRIANGLE_FAN, AndroidInput_ConOvl.hitkey << 2, 4);
		}
		glEnable(GL_TEXTURE_2D);	/* enable texturing */

		glColor4f(1.0f, 1.0f, 1.0f, AndroidInput_ConOvl.opacity);
		for (i = 0; i < CONK_NUM; ++i) {
			glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, crop_lbl[i]);
			glDrawTexiOES(conkey_lbl[i].x, conkey_lbl[i].y, 0, conkey_lbl[i].w, conkey_lbl[i].h);
		}
		if (glGetError() != GL_NO_ERROR) Log_print("OpenGL error at console overlay");
	}

	glDisable(GL_BLEND);		/* disable blending */
}

void Update_Overlays(void)
{
	struct joy_overlay_state *s;
	struct consolekey_overlay_state *c;

	s = &AndroidInput_JoyOvl;
	c = &AndroidInput_ConOvl;

	/* joy et co. */
	if (s->fireopacity > OPACITY_CUTOFF)
		s->fireopacity -= 0.05f;
	else
		s->fireopacity = 0.0f;
	if (s->stickopacity > OPACITY_CUTOFF) {
		s->stickopacity -= 0.05f;
		s->areaopacityfrm = 0;
		s->areaopacitycur = s->areaopacityset;
	} else {
		s->stickopacity = 0.0f;
		s->areaopacityfrm++;
		if (s->areaopacityfrm > OPACITY_FRMSTR) {
			if (s->areaopacitycur > OPACITY_CUTOFF)
				s->areaopacitycur -= OPACITY_STEP;
			else {
				s->areaopacitycur = OPACITY_CUTOFF;
				s->areaopacityfrm--;
			}
		}
	}

	/* console keys */
	switch (c->ovl_visible) {
	case COVL_READY:
		if (c->hitkey == CONK_NOKEY)
			c->statecnt -= Android_CovlHold;
			if (c->statecnt <= 0) {
				c->statecnt = 0;
				c->ovl_visible = COVL_FADEOUT;
			}
		break;
	case COVL_FADEOUT:
		if (c->opacity > OPACITY_CUTOFF)
			c->opacity -= 2 * OPACITY_STEP;
		else {
			c->ovl_visible = COVL_HIDDEN;
			c->opacity = 0.0f;
		}
		break;
	case COVL_FADEIN:
		if (c->opacity < COVL_MAX_OPACITY)
			c->opacity += 4 * OPACITY_STEP;
		else {
			c->ovl_visible = COVL_READY;
			c->opacity = COVL_MAX_OPACITY;
			c->statecnt = COVL_HOLD_TIME << 1;
		}
		break;
	default: /* COVL_HIDDEN */
		;
	}
	if (c->hitkey == CONK_RESET) {
		if (c->resetcnt >= RESET_HARD) {
			Atari800_Coldstart();
		} else if (c->resetcnt >= RESET_SOFT) {
			Atari800_Warmstart();
			CPU_cim_encountered = FALSE;
		}
		c->resetcnt++;
	} else {
		c->resetcnt = 0;
	}
}

void Android_ExitGraphics(void)
{
	if (hicolor_screen)
		free(hicolor_screen);
	hicolor_screen = NULL;

	if (palette)
		free(palette);
	palette = NULL;

	glDeleteTextures(TEX_MAXNAMES, texture);
}
