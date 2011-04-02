#define CONK_VERT_MAX   (2 * 4 * 5)

#define SCREEN_WIDTH   384
#define SCREEN_HEIGHT  240
#define DEAD_WIDTH     48
#define SCANLINE_START (DEAD_WIDTH / 2)
#define SCANLINE_END   (SCREEN_WIDTH - DEAD_WIDTH / 2)
#define SCANLINE_LEN   (SCREEN_WIDTH - DEAD_WIDTH)

extern int Android_ScreenW;
extern int Android_ScreenH;
extern int Android_Aspect;
extern int Android_Bilinear;
extern int Android_CropScreen[];
extern float Android_Joyscale;

int  Android_InitGraphics(void);
void Android_ExitGraphics(void);

void Android_ConvertScreen(void);
void Android_PaletteUpdate(void);
void Android_Render(void);
void Update_Overlays(void);
void Joyovl_Scale(void);
