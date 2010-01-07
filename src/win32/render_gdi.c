#include "render_gdi.h"
#include "screen_win32.h"
#include "colours.h"
#include "screen.h"

extern HWND hWndMain;
extern char *myname;

// GDI specific rendering code
void refreshv_gdi(UBYTE *scr_ptr, FRAMEPARAMS *fp)
{
	HDC hCdc;
	BITMAPINFO bi;
	DWORD *pixels = NULL;
	COLORREF cr;
	HBITMAP hBitmap;
	int i, j, k; 
	int cycles = 1;
	int bmWidth = Screen_WIDTH - 48;
	int bmHeight = Screen_HEIGHT;
	bmHeight *= fp->scanlinemode;

	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); // structure size in bytes
	bi.bmiHeader.biWidth = bmWidth; 
	bi.bmiHeader.biHeight = bmHeight; 
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB; //BI_BITFIELDS OR BI_RGB???
	bi.bmiHeader.biSizeImage = 0; // for BI_RGB set to 0
	bi.bmiHeader.biXPelsPerMeter = 2952; //75dpi=2952bpm
	bi.bmiHeader.biYPelsPerMeter = 2952; //75dpi=2952bpm
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	hCdc = CreateCompatibleDC(fp->hdc);
	hBitmap = CreateDIBSection(hCdc, &bi, DIB_RGB_COLORS, (void *)&pixels, NULL, 0);
	if (!hBitmap) {
		MessageBox(hWndMain, "Could not create bitmap", myname, MB_OK);
		DestroyWindow(hWndMain);
		return;
	}

	// Copy the atari screen buffer to bitmap and add scanlines if specified.
	if (fp->scanlinemode != NONE)
		cycles = fp->scanlinemode - 1;

	for (i = 0; i < bmHeight; i+=fp->scanlinemode) {
		for (j = 0; j < bmWidth; j++) {
			cr = Colours_table[*scr_ptr++];
			for (k = 0; k < cycles ; k++) {			
				pixels[(i + k) * bmWidth + j] = cr;
			}
		}
		scr_ptr+=48;
	}

	SelectObject(hCdc, hBitmap);

	// Draw the bitmap on the screen. The image must be flipped vertically
	if (!StretchBlt(fp->hdc, fp->horizoffset, fp->vertoffset, fp->width, fp->height,
		hCdc, 0, bi.bmiHeader.biHeight, bi.bmiHeader.biWidth, -bi.bmiHeader.biHeight, SRCCOPY)) 
	{
		MessageBox(hWndMain, "Could not StretchBlt", myname, MB_OK);
		DestroyWindow(hWndMain);
	}

	DeleteDC(hCdc);
	DeleteObject(hBitmap);
}
