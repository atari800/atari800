/* (C) 2000  Krzysztof Nikiel */
/* $Id$ */
#ifndef A800_SOUND_H
#define A800_SOUND_H

int initsound(int *argc, char *argv[]);
void uninitsound(void);
void sndrestore(void);
void sndhandler(void);

#endif
