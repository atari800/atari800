#ifndef _COLOURS_H_
#define _COLOURS_H_

extern int colortable[256];

#define Palette_GetR(x) (colortable[x] >> 16 & 0xff)
#define Palette_GetG(x) (colortable[x] >> 8 & 0xff)
#define Palette_GetB(x) (colortable[x] & 0xff)
#define Palette_GetY(x) (0.30 * Palette_GetR(x) + 0.59 * Palette_GetG(x) + 0.11 * Palette_GetB(x))
void Palette_SetRGB(int i, int r, int g, int b);

int Palette_Read(const char *filename);
void Palette_Generate(int black, int white, int colshift);
void Palette_Adjust(int black, int white, int colintens);
void Palette_Initialise(int *argc, char *argv[]);

#endif /* _COLOURS_H_ */
