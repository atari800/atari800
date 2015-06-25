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
	UINT* pixels;
	int pixel = 0;
	int i, x, y;
	int scanlinelength;
	InterpolationMode im;
	int h_adj = 0;
	int v_adj = 0;
	
	// calculate bitmap height & width 
	int bmWidth = fp->view.right - fp->view.left;
	int bmHeight = fp->view.bottom - fp->view.top;
	
	bmHeight *= fp->scanlinemode + 1;
	
	// gdiplus specific window fill adjustment
	if (!fp->scanlinemode) {
		v_adj = 1;
		h_adj = 1;
	}
	
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
	
	Rect r(0, 0 , bmWidth, bmHeight);	
	
	Bitmap* bitmap = new Bitmap(bmWidth, bmHeight, PixelFormat32bppRGB);

	BitmapData bitmapData;
	bitmap->LockBits(&r, ImageLockModeWrite, PixelFormat32bppRGB, &bitmapData);
	pixels = (UINT*)bitmapData.Scan0;
	scanlinelength = bitmapData.Stride / 4;

	// copy screen buffer to the bitmap
	scr_ptr += fp->view.top * Screen_WIDTH + fp->view.left;
	for (y = fp->view.top; y < fp->view.bottom; y++) {
		for (x = fp->view.left; x < fp->view.right; x++) {
			if (y < 0 || y >= Screen_HEIGHT || x < 0 || x >= Screen_WIDTH)
				pixels[pixel] = Colours_table[0];
			else
				pixels[pixel] = Colours_table[*scr_ptr];
				
				for (i = 0; i < fp->scanlinemode; i++) {
					pixels[pixel + i * scanlinelength] = pixels[pixel]; 
				}
				
			scr_ptr++;
			pixel++;
		}
		scr_ptr += Screen_WIDTH - bmWidth;
	    pixel += scanlinelength * fp->scanlinemode;
	}

	bitmap->UnlockBits(&bitmapData);

	Graphics* graphics = new Graphics(fp->hdc);
	graphics->SetInterpolationMode(im);
	graphics->DrawImage(bitmap, fp->x_origin, fp->y_origin, fp->width + h_adj, fp->height + v_adj);

	delete graphics;
	delete bitmap;
}

