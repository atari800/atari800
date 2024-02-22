#ifndef SDL_INPUT_H_
#define SDL_INPUT_H_

#include <stdio.h>

#if SDL2
enum INPUT_joystick_diagonals {
	JoystickNoDiagonals = 0,
	JoystickNarrowDiagonalsZone,
	JoystickWideDiagonalsZone,
};
enum INPUT_joystick_button_action {
	JoystickNoAction = 0,
	JoystickUiAction,
	JoystickAtariKey,
	JoystickKeyboard
};
struct INPUT_joystick_button {
	enum INPUT_joystick_button_action action; // type of action
	int key; // an AKEY or UI_MENU_* or SDL_Keycode, depending on the action type
};
#define INPUT_JOYSTICK_MAX_BUTTONS 15
#endif

/*Configuration of a real SDL joystick*/
typedef struct SDL_INPUT_RealJSConfig_t {
	int use_hat;    
#if SDL2
	int axes;
	enum INPUT_joystick_diagonals diagonal_zones;
	struct INPUT_joystick_button buttons[INPUT_JOYSTICK_MAX_BUTTONS];
#endif
} SDL_INPUT_RealJSConfig_t;

int SDL_INPUT_ReadConfig(char *option, char *parameters);
void SDL_INPUT_WriteConfig(FILE *fp);

int SDL_INPUT_Initialise(int *argc, char *argv[]);
void SDL_INPUT_Exit(void);
/* Restarts input after e.g. exiting the console monitor. */
void SDL_INPUT_Restart(void);

void SDL_INPUT_Mouse(void);

/*Get pointer to a real joystick configuration (for UI)*/
SDL_INPUT_RealJSConfig_t* SDL_INPUT_GetRealJSConfig(int joyIndex);

#endif /* SDL_INPUT_H_ */
