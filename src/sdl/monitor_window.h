#ifndef SDL_MONITOR_WINDOW_H_
#define SDL_MONITOR_WINDOW_H_

#include "config.h"

#ifdef MONITOR_WINDOW

#include <stddef.h>

/* Set monitor window dimensions (must be called before Init) */
void SDL_MONITOR_WINDOW_SetSize(int width, int height);

/* Initialize the monitor window */
int SDL_MONITOR_WINDOW_Init(void);

/* Cleanup the monitor window */
void SDL_MONITOR_WINDOW_Exit(void);

/* Show/hide the monitor window */
void SDL_MONITOR_WINDOW_Show(int show);

/* Check if monitor window is visible */
int SDL_MONITOR_WINDOW_IsVisible(void);

/* Get a line of input from the monitor window */
/* Returns: pointer to buffer, or NULL on error */
char *SDL_MONITOR_WINDOW_GetLine(const char *prompt, char *buffer, size_t size);

/* Print formatted output to the monitor window */
void SDL_MONITOR_WINDOW_Printf(const char *format, ...);

/* Process SDL events for the monitor window */
void SDL_MONITOR_WINDOW_ProcessEvents(void);

#endif /* MONITOR_WINDOW */

#endif /* SDL_MONITOR_WINDOW_H_ */
