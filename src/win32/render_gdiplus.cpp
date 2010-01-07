#include "config.h"
#include "render_gdiplus.h"
#include "screen.h"

// GDI+ used to support interpolated filtering
#include <gdiplus.h>
using namespace Gdiplus;

extern "C" {
#include "colours.h"
}

// GDI+ startup stuff
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;
// End GDI+ startup stuff

// Required startup function call for the GDI+ engine
extern "C" void startupgdiplus(void)
{
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

// Shutdown the GDI+ engine
extern "C" void shutdowngdiplus(void)
{
	GdiplusShutdown(gdiplusToken);
}

// GDI+ specific frame rendering code 
extern "C" void refreshv_gdiplus(UBYTE *scr_ptr, FRAMEPARAMS *fp)
{
	int i, j, k, cycles;
	UINT* pixels;
	int scanlinelength;
	int bmWidth = Screen_WIDTH - 48;
	int bmHeight = Screen_HEIGHT;
	InterpolationMode im;
	COLORREF cr;

	// Choose the appropriate interpolation filter
	switch (fp->filter)
	{
		case NEARESTNEIGHBOR:
			im = InterpolationModeNearestNeighbor;
			break;
		case BILINEAR:
			im = InterpolationModeBilinear;
			break;
		case BICUBIC:
			im = InterpolationModeBicubic;
			break;
		case HQBILINEAR:
			im = InterpolationModeHighQualityBilinear;
			break;
		case HQBICUBIC:
			im = InterpolationModeHighQualityBicubic;
			break;
		default:
			im = InterpolationModeNearestNeighbor;
	}
	
	bmHeight *= fp->scanlinemode;		   

	Rect r(0, 0 , bmWidth, bmHeight);	
	
	Bitmap* bitmap = new Bitmap(bmWidth, bmHeight, PixelFormat32bppRGB);

	BitmapData bitmapData;
	bitmap->LockBits(&r, ImageLockModeWrite, PixelFormat32bppRGB, &bitmapData);
	pixels = (UINT*)bitmapData.Scan0;
	scanlinelength = bitmapData.Stride / 4;

	// Copy the atari screen buffer to bitmap and add scanlines if specified.	
	fp->scanlinemode == NONE ? cycles = 1 : cycles = fp->scanlinemode - 1;
	for (i = 0; i < bmHeight; i+=fp->scanlinemode) {
		for (j = 0; j < bmWidth; j++) {
			cr = Colours_table[*scr_ptr++];
			for (k = 0; k < cycles ; k++) {			
				pixels[(i + k) * scanlinelength + j] = cr;
			}
		}
		scr_ptr+=48;
	}

	bitmap->UnlockBits(&bitmapData);

	Graphics* graphics = new Graphics(fp->hdc);
	graphics->SetInterpolationMode(im);
	graphics->DrawImage(bitmap, fp->horizoffset, fp->vertoffset, fp->width, fp->height);

	delete graphics;
	delete bitmap;
}