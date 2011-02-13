#define CONK_VERT_MAX   (2 * 4 * 5)

extern int Android_ScreenW;
extern int Android_ScreenH;
extern int Android_Aspect;
extern int Android_Bilinear;
extern float Android_Joyscale;

int  Android_InitGraphics(void);
void Android_ExitGraphics(void);

void Android_ConvertScreen(void);
void Android_PaletteUpdate(void);
void Android_Render(void);
void Update_Overlays(void);
void Joyovl_Scale(void);
