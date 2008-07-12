#ifndef COLOURS_H_
#define COLOURS_H_

extern int *Colours_table;

#define Colours_GetR(x) ((UBYTE) (Colours_table[x] >> 16))
#define Colours_GetG(x) ((UBYTE) (Colours_table[x] >> 8))
#define Colours_GetB(x) ((UBYTE) Colours_table[x])
#define Colours_GetY(x) (0.30 * Colours_GetR(x) + 0.59 * Colours_GetG(x) + 0.11 * Colours_GetB(x))
void Colours_SetRGB(int i, int r, int g, int b, int *colortable_ptr);

int Colours_Read(const char *filename, int *colortable_ptr);
void Colours_Generate(int black, int white, int colshift, int *colortable_ptr);
void Colours_Adjust(int black, int white, int colintens, int *colortable_ptr);
void Colours_SetVideoSystem(int mode);
void Colours_InitialiseMachine(void);
void Colours_Initialise(int *argc, char *argv[]);

#endif /* COLOURS_H_ */
