#ifndef SDL_SOUND_H_
#define SDL_SOUND_H_

/* Pause and close SDL audio. Required by Issue #97. */
void SDL_SOUND_HardPause(void);
/* Reopen and continue SDL audio. Required by Issue #97. */
void SDL_SOUND_HardContinue(void);

#endif /* SDL_SOUND_H_ */
