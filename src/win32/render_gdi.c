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
	HBITMAP hBitmap;
	DWORD *pixels = NULL;
	int pixel = 0;
	int i, x, y; 
	int v_adj = 0;
	
	// calculate bitmap height & width
	int bmWidth = fp->view.right - fp->view.left;
	int bmHeight = fp->view.bottom - fp->view.top;
	
	bmHeight *= fp->scanlinemode + 1;	
	
    // GDI specific window fill adjustment
	if (!fp->scanlinemode) {
		v_adj = (int) fp->height / bmHeight;
	}
               
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); 
	bi.bmiHeader.biWidth = bmWidth; 
	bi.bmiHeader.biHeight = bmHeight; 
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB; 
	bi.bmiHeader.biSizeImage = 0; 
	bi.bmiHeader.biXPelsPerMeter = 0; 
	bi.bmiHeader.biYPelsPerMeter = 0; 
	bi.bmiHeader.biClrUsed = 0;
	bi.bmiHeader.biClrImportant = 0;

	hCdc = CreateCompatibleDC(fp->hdc);
	hBitmap = CreateDIBSection(hCdc, &bi, DIB_RGB_COLORS, (void *)&pixels, NULL, 0);
	if (!hBitmap) {
		MessageBox(hWndMain, "Could not create bitmap", myname, MB_OK);
		DestroyWindow(hWndMain);
		return;
	}

	// copy screen buffer to the bitmap
	scr_ptr += fp->view.top * Screen_WIDTH + fp->view.left;
	for (y = fp->view.top; y < fp->view.bottom; y++) {
		for (x = fp->view.left; x < fp->view.right; x++) {
			if (y < 0 || y >= Screen_HEIGHT || x < 0 || x >= Screen_WIDTH)
				pixels[pixel] = Colours_table[0];
			else
				pixels[pixel] = Colours_table[*scr_ptr];
			
				for (i = 0; i < fp->scanlinemode; i++) {
					pixels[pixel + i * bmWidth] = pixels[pixel]; 
				}
				
			scr_ptr++;
			pixel++;
		}
		scr_ptr += Screen_WIDTH - bmWidth;
	    pixel += bmWidth * fp->scanlinemode;
	}
	
	SelectObject(hCdc, hBitmap);

	// Draw the bitmap on the screen. The image must be flipped vertically
	if (!StretchBlt(fp->hdc, fp->x_origin, fp->y_origin, fp->width, fp->height + v_adj,
		hCdc, 0, bi.bmiHeader.biHeight, bi.bmiHeader.biWidth, -bi.bmiHeader.biHeight, SRCCOPY)) 
	{
		MessageBox(hWndMain, "Could not StretchBlt", myname, MB_OK);
		DestroyWindow(hWndMain);
	}

	DeleteDC(hCdc);
	DeleteObject(hBitmap);
}
