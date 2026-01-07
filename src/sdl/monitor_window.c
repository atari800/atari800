/*
 * sdl/monitor_window.c - SDL Monitor Window implementation
 *
 * Provides a separate SDL window for the monitor debugger on platforms
 * where stdin/stdout redirection causes problems (e.g., macOS).
 */

#include "config.h"

#ifdef MONITOR_WINDOW

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "monitor_window.h"
#include "log.h"
#include "font_atlas_data.h"

/* Detect SDL version */
#if SDL_MAJOR_VERSION == 1
#define USING_SDL1
#else
#define USING_SDL2
#endif

/* Window dimensions - configurable at runtime */
static int monitor_window_width = 960;
static int monitor_window_height = 600;

/* Font parameters - selected based on window width */
static int monitor_font_width;
static int monitor_font_height;
static int monitor_font_atlas_width;
static int monitor_font_atlas_height;
static int monitor_font_chars_per_row;
static int monitor_font_start_char;
static const unsigned char *monitor_font_atlas_data;

static int monitor_cols;
static int monitor_rows;

/* Colors */
#define COLOR_BACKGROUND 0x000000  /* Black */
#define COLOR_TEXT       0xFFFFFF  /* White */
#define COLOR_CURSOR     0x00FF00  /* Green */

/* Monitor window state */
#ifdef USING_SDL2
static SDL_Window *monitor_window = NULL;
static SDL_Renderer *monitor_renderer = NULL;
static SDL_Texture *monitor_texture = NULL;
static SDL_Surface *monitor_surface = NULL;
#else
static SDL_Surface *monitor_surface = NULL;
#endif
static SDL_Surface *font_atlas = NULL;
static int monitor_visible = 0;

/* Text buffer */
static char text_buffer[100][100];  /* Max size, will use MONITOR_ROWS/COLS */
static int current_row = 0;
static int current_col = 0;

/* Input buffer */
static char input_buffer[256];
static int input_pos = 0;
static int input_ready = 0;

static void clear_screen(void)
{
	int i, j;
	for (i = 0; i < monitor_rows; i++) {
		for (j = 0; j < monitor_cols; j++) {
			text_buffer[i][j] = ' ';
		}
		text_buffer[i][monitor_cols] = '\0';
	}
	current_row = 0;
	current_col = 0;
}

static void scroll_up(void)
{
	int i;
	for (i = 0; i < monitor_rows - 1; i++) {
		memcpy(text_buffer[i], text_buffer[i + 1], monitor_cols);
	}
	memset(text_buffer[monitor_rows - 1], ' ', monitor_cols);
	current_row = monitor_rows - 1;
	current_col = 0;
}

static void put_char(char c)
{
	if (c == '\n') {
		current_col = 0;
		current_row++;
		if (current_row >= monitor_rows) {
			scroll_up();
		}
		return;
	}

	if (c == '\r') {
		current_col = 0;
		return;
	}

	if (c == '\b') {
		if (current_col > 0) {
			current_col--;
			text_buffer[current_row][current_col] = ' ';
		}
		return;
	}

	if (current_col >= monitor_cols) {
		current_col = 0;
		current_row++;
		if (current_row >= monitor_rows) {
			scroll_up();
		}
	}

	text_buffer[current_row][current_col++] = c;
}

static void draw_char(int x, int y, char c)
{
	int char_index;
	int sx, sy, dx, dy;
	Uint8 *src_pixels;
	Uint8 *dest_pixels;
	Uint32 *dest_pixel;
	Uint8 alpha;

	/* Only render printable ASCII characters */
	if (c < monitor_font_start_char || c > 126)
		return;

	if (!font_atlas || !monitor_surface)
		return;

	/* Calculate position in font atlas */
	char_index = c - monitor_font_start_char;

	/* Manual blit with alpha blending (grayscale to white text) */
	if (SDL_MUSTLOCK(font_atlas)) SDL_LockSurface(font_atlas);
	if (SDL_MUSTLOCK(monitor_surface)) SDL_LockSurface(monitor_surface);

	src_pixels = (Uint8 *)font_atlas->pixels;
	dest_pixels = (Uint8 *)monitor_surface->pixels;

	for (sy = 0; sy < monitor_font_height; sy++) {
		for (sx = 0; sx < monitor_font_width; sx++) {
			dx = x * monitor_font_width + sx;
			dy = y * monitor_font_height + sy;

			if (dx >= monitor_surface->w || dy >= monitor_surface->h)
				continue;

			/* Get grayscale value from atlas (treat as alpha) */
			int src_x = (char_index % monitor_font_chars_per_row) * monitor_font_width + sx;
			int src_y = (char_index / monitor_font_chars_per_row) * monitor_font_height + sy;
			alpha = src_pixels[src_y * font_atlas->pitch + src_x];

			if (alpha > 0) {
				/* Draw white pixel with alpha */
				dest_pixel = (Uint32 *)(dest_pixels + dy * monitor_surface->pitch + dx * 4);
				*dest_pixel = SDL_MapRGB(monitor_surface->format, alpha, alpha, alpha);
			}
		}
	}

	if (SDL_MUSTLOCK(monitor_surface)) SDL_UnlockSurface(monitor_surface);
	if (SDL_MUSTLOCK(font_atlas)) SDL_UnlockSurface(font_atlas);
}

static void render_window(void)
{
	int row, col;
	SDL_Rect cursor_rect;
	Uint32 cursor_color;

	if (!monitor_surface || !monitor_visible)
		return;

	/* Clear background to black */
	SDL_FillRect(monitor_surface, NULL, SDL_MapRGB(monitor_surface->format, 0, 0, 0));

	/* Draw text buffer */
	for (row = 0; row < monitor_rows; row++) {
		for (col = 0; col < monitor_cols; col++) {
			if (text_buffer[row][col] != ' ') {
				draw_char(col, row, text_buffer[row][col]);
			}
		}
	}

	/* Draw cursor at bottom of line */
	cursor_rect.x = current_col * monitor_font_width;
	cursor_rect.y = current_row * monitor_font_height + (monitor_font_height - 2);
	cursor_rect.w = monitor_font_width;
	cursor_rect.h = 2;
	cursor_color = SDL_MapRGB(monitor_surface->format, 0, 255, 0);
	SDL_FillRect(monitor_surface, &cursor_rect, cursor_color);

	/* Update screen - different for SDL1 vs SDL2 */
#ifdef USING_SDL2
	SDL_UpdateTexture(monitor_texture, NULL, monitor_surface->pixels, monitor_surface->pitch);
	SDL_RenderClear(monitor_renderer);
	SDL_RenderCopy(monitor_renderer, monitor_texture, NULL, NULL);
	SDL_RenderPresent(monitor_renderer);
#else
	SDL_Flip(monitor_surface);
#endif
}

/* Select font based on window width */
static void select_font(void)
{
	if (monitor_window_width >= 800) {
		/* Use 10x20 font for wider windows */
		monitor_font_width = FONT_CHAR_10x20_WIDTH;
		monitor_font_height = FONT_CHAR_10x20_HEIGHT;
		monitor_font_atlas_width = FONT_ATLAS_10x20_WIDTH;
		monitor_font_atlas_height = FONT_ATLAS_10x20_HEIGHT;
		monitor_font_chars_per_row = FONT_10x20_CHARS_PER_ROW;
		monitor_font_start_char = FONT_10x20_START_CHAR;
		monitor_font_atlas_data = font_atlas_10x20_data;
		Log_print("Selected 10x20 font for %dx%d window", monitor_window_width, monitor_window_height);
	} else {
		/* Use 8x16 font for narrower windows */
		monitor_font_width = FONT_CHAR_8x16_WIDTH;
		monitor_font_height = FONT_CHAR_8x16_HEIGHT;
		monitor_font_atlas_width = FONT_ATLAS_8x16_WIDTH;
		monitor_font_atlas_height = FONT_ATLAS_8x16_HEIGHT;
		monitor_font_chars_per_row = FONT_8x16_CHARS_PER_ROW;
		monitor_font_start_char = FONT_8x16_START_CHAR;
		monitor_font_atlas_data = font_atlas_8x16_data;
		Log_print("Selected 8x16 font for %dx%d window", monitor_window_width, monitor_window_height);
	}

	/* Calculate rows and columns */
	monitor_cols = monitor_window_width / monitor_font_width;
	monitor_rows = monitor_window_height / monitor_font_height;
}

/* Lazy initialization - create window only when first needed */
static int create_monitor_window(void)
{
	Uint8 *pixels;
	int i;

	if (font_atlas) {
		/* Already initialized */
		return 1;
	}

	/* Select font based on window width */
	select_font();

	Log_print("Creating SDL monitor window");
#ifdef USING_SDL2
	Log_print("Using SDL2 code path");
#else
	Log_print("Using SDL1 code path");
#endif

	/* Create font atlas surface from embedded data */
#ifdef USING_SDL2
	font_atlas = SDL_CreateRGBSurface(0, monitor_font_atlas_width, monitor_font_atlas_height, 8, 0, 0, 0, 0);
#else
	font_atlas = SDL_CreateRGBSurface(SDL_SWSURFACE, monitor_font_atlas_width, monitor_font_atlas_height, 8, 0, 0, 0, 0);
#endif

	if (!font_atlas) {
		Log_print("Failed to create font atlas surface: %s", SDL_GetError());
		return 0;
	}

	/* Set up grayscale palette */
	SDL_Color palette[256];
	for (i = 0; i < 256; i++) {
		palette[i].r = i;
		palette[i].g = i;
		palette[i].b = i;
#ifdef USING_SDL2
		palette[i].a = 255;
#endif
	}
#ifdef USING_SDL2
	SDL_SetPaletteColors(font_atlas->format->palette, palette, 0, 256);
#else
	SDL_SetColors(font_atlas, palette, 0, 256);
#endif

	/* Copy embedded font data to surface */
	if (SDL_MUSTLOCK(font_atlas)) SDL_LockSurface(font_atlas);
	pixels = (Uint8 *)font_atlas->pixels;
	memcpy(pixels, monitor_font_atlas_data, monitor_font_atlas_width * monitor_font_atlas_height);
	if (SDL_MUSTLOCK(font_atlas)) SDL_UnlockSurface(font_atlas);

	Log_print("Font atlas created successfully (%dx%d, %dx%d per char)",
		monitor_font_atlas_width, monitor_font_atlas_height,
		monitor_font_width, monitor_font_height);

#ifdef USING_SDL2
	/* SDL2: Create window and renderer */
	monitor_window = SDL_CreateWindow(
		"Atari800 Monitor",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		monitor_window_width,
		monitor_window_height,
		SDL_WINDOW_HIDDEN  /* Start hidden, will be shown when F8 is pressed */
	);

	if (!monitor_window) {
		Log_print("Failed to create monitor window: %s", SDL_GetError());
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
		return 0;
	}

	monitor_renderer = SDL_CreateRenderer(monitor_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!monitor_renderer) {
		Log_print("Failed to create renderer: %s", SDL_GetError());
		SDL_DestroyWindow(monitor_window);
		monitor_window = NULL;
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
		return 0;
	}

	/* Create surface for rendering */
	monitor_surface = SDL_CreateRGBSurface(
		0,
		monitor_window_width,
		monitor_window_height,
		32,
		0x00FF0000,
		0x0000FF00,
		0x000000FF,
		0xFF000000
	);

	if (!monitor_surface) {
		Log_print("Failed to create surface: %s", SDL_GetError());
		SDL_DestroyRenderer(monitor_renderer);
		monitor_renderer = NULL;
		SDL_DestroyWindow(monitor_window);
		monitor_window = NULL;
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
		return 0;
	}

	/* Create texture */
	monitor_texture = SDL_CreateTexture(
		monitor_renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		monitor_window_width,
		monitor_window_height
	);

	if (!monitor_texture) {
		Log_print("Failed to create texture: %s", SDL_GetError());
		SDL_FreeSurface(monitor_surface);
		monitor_surface = NULL;
		SDL_DestroyRenderer(monitor_renderer);
		monitor_renderer = NULL;
		SDL_DestroyWindow(monitor_window);
		monitor_window = NULL;
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
		return 0;
	}
#else
	/* SDL1: Create video surface */
	monitor_surface = SDL_SetVideoMode(
		monitor_window_width,
		monitor_window_height,
		32,
		SDL_SWSURFACE
	);

	if (!monitor_surface) {
		Log_print("Failed to create monitor window: %s", SDL_GetError());
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
		return 0;
	}

	SDL_WM_SetCaption("Atari800 Monitor", NULL);
#endif

	clear_screen();

	return 1;
}

void SDL_MONITOR_WINDOW_SetSize(int width, int height)
{
	monitor_window_width = width;
	monitor_window_height = height;
	Log_print("Monitor window size set to %dx%d", width, height);
}

int SDL_MONITOR_WINDOW_Init(void)
{
	Log_print("SDL monitor window support initialized (lazy creation)");
	/* Don't create window yet - will be created on first use */
	return 1;
}

void SDL_MONITOR_WINDOW_Exit(void)
{
#ifdef USING_SDL2
	if (monitor_texture) {
		SDL_DestroyTexture(monitor_texture);
		monitor_texture = NULL;
	}

	if (monitor_surface) {
		SDL_FreeSurface(monitor_surface);
		monitor_surface = NULL;
	}

	if (monitor_renderer) {
		SDL_DestroyRenderer(monitor_renderer);
		monitor_renderer = NULL;
	}

	if (monitor_window) {
		SDL_DestroyWindow(monitor_window);
		monitor_window = NULL;
	}
#else
	/* SDL1: monitor_surface is managed by SDL, don't free it */
	monitor_surface = NULL;
#endif

	if (font_atlas) {
		SDL_FreeSurface(font_atlas);
		font_atlas = NULL;
	}

	monitor_visible = 0;
}

void SDL_MONITOR_WINDOW_Show(int show)
{
	static int first_show = 1;

	Log_print("SDL_MONITOR_WINDOW_Show called with show=%d", show);

	if (show) {
		/* Create window on first use (lazy initialization) */
		if (!monitor_surface) {
			Log_print("Window not created yet, creating now...");
			if (!create_monitor_window()) {
				Log_print("ERROR: Failed to create monitor window!");
				return;
			}
		}

		monitor_visible = 1;
		Log_print("Setting monitor_visible=1");

#ifdef USING_SDL2
		if (monitor_window) {
			Log_print("Calling SDL_ShowWindow");
			SDL_ShowWindow(monitor_window);
			SDL_RaiseWindow(monitor_window);
		}
#endif

		if (first_show) {
			/* Add welcome message on first show */
			Log_print("Showing monitor window for first time");
			SDL_MONITOR_WINDOW_Printf("Atari800 Monitor - SDL Window Mode\n");
			SDL_MONITOR_WINDOW_Printf("Type 'help' for commands, 'cont' to continue emulation\n");
			SDL_MONITOR_WINDOW_Printf("\n");
			first_show = 0;
		}

		render_window();
		Log_print("render_window called");
	} else {
		monitor_visible = 0;
		Log_print("Setting monitor_visible=0");
#ifdef USING_SDL2
		if (monitor_window) {
			SDL_HideWindow(monitor_window);
		}
#endif
	}
}

int SDL_MONITOR_WINDOW_IsVisible(void)
{
	return monitor_visible;
}

void SDL_MONITOR_WINDOW_Printf(const char *format, ...)
{
	char buffer[1024];
	va_list args;
	int i;

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	for (i = 0; buffer[i]; i++) {
		put_char(buffer[i]);
	}

	if (monitor_visible) {
		render_window();
	}
}

void SDL_MONITOR_WINDOW_ProcessEvents(void)
{
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			/* Don't quit the whole app, just hide monitor */
			SDL_MONITOR_WINDOW_Show(0);
			input_ready = 1;
			strcpy(input_buffer, "cont");
		}
		else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_RETURN) {
				input_buffer[input_pos] = '\0';
				input_ready = 1;
				SDL_MONITOR_WINDOW_Printf("\n");
			}
			else if (event.key.keysym.sym == SDLK_BACKSPACE) {
				if (input_pos > 0) {
					input_pos--;
					put_char('\b');
					render_window();
				}
			}
			else if (event.key.keysym.sym == SDLK_ESCAPE) {
				/* ESC = exit monitor and hide window */
				SDL_MONITOR_WINDOW_Show(0);
				input_ready = 1;
				strcpy(input_buffer, "cont");
				SDL_MONITOR_WINDOW_Printf("\n");
			}
#ifdef USING_SDL1
			else if (event.key.keysym.unicode && event.key.keysym.unicode < 128) {
				/* SDL1: Handle regular ASCII input via unicode field */
				if (input_pos < sizeof(input_buffer) - 1) {
					char c = (char)event.key.keysym.unicode;
					if (c >= 32 && c < 127) {
						input_buffer[input_pos++] = c;
						put_char(c);
						render_window();
					}
				}
			}
#endif
		}
#ifdef USING_SDL2
		else if (event.type == SDL_TEXTINPUT) {
			/* SDL2: Handle text input */
			if (input_pos < sizeof(input_buffer) - 1) {
				char c = event.text.text[0];
				if (c >= 32 && c < 127) {
					input_buffer[input_pos++] = c;
					put_char(c);
					render_window();
				}
			}
		}
#endif
	}
}

char *SDL_MONITOR_WINDOW_GetLine(const char *prompt, char *buffer, size_t size)
{
	if (!monitor_surface) {
		return NULL;
	}

	/* Display prompt */
	SDL_MONITOR_WINDOW_Printf("%s", prompt);

#ifdef USING_SDL2
	/* Enable text input for SDL2 */
	SDL_StartTextInput();
#else
	/* Enable unicode for SDL1 */
	SDL_EnableUNICODE(1);
#endif

	/* Reset input */
	input_pos = 0;
	input_ready = 0;
	memset(input_buffer, 0, sizeof(input_buffer));

	/* Wait for input */
	while (!input_ready) {
		SDL_MONITOR_WINDOW_ProcessEvents();
		SDL_Delay(10);
	}

#ifdef USING_SDL2
	/* Disable text input */
	SDL_StopTextInput();
#endif

	/* Copy to output buffer */
	strncpy(buffer, input_buffer, size - 1);
	buffer[size - 1] = '\0';

	return buffer;
}

#endif /* MONITOR_WINDOW */
