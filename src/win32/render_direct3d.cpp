#include "config.h"
#include "screen.h"
#include "render_direct3d.h"
#include <windows.h>

#include <d3d9.h>
#include <d3dx9.h>

extern "C" HWND hWndMain;
extern "C" {
#include "colours.h"
#include "log.h"
}

typedef D3DXMATRIX* (WINAPI* D3DXMatrixLookAtLHFunc)(D3DXMATRIX*, CONST D3DXVECTOR3*, CONST D3DXVECTOR3*, CONST D3DXVECTOR3*);
typedef D3DXMATRIX* (WINAPI* D3DXMatrixPerspectiveFovLHFunc)(D3DXMATRIX*, FLOAT, FLOAT, FLOAT, FLOAT);
typedef D3DXMATRIX* (WINAPI* D3DXMatrixRotationYFunc)(D3DXMATRIX*, FLOAT);

// D3D declarations
static LPDIRECT3D9 d3d;    
static LPDIRECT3DDEVICE9 d3d_device;    
static LPDIRECT3DVERTEXBUFFER9 vertex_buffer = NULL;    
static LPDIRECT3DTEXTURE9 texture_buffer = NULL;    
static D3DPRESENT_PARAMETERS d3dpp;

struct CUSTOMVERTEX {FLOAT X, Y, Z; DWORD COLOR; FLOAT U, V;};
#define CUSTOMFVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

static float const pi = 3.1415926535897932384626433832795028841971693993751058209749445923078164062862f;
	
static float texturehorizclip = 0.0f;
static float texturevertclip = 0.0f; 

static HMODULE hD3DX = NULL;
static D3DXMatrixLookAtLHFunc D3DXMatrixLookAtLHPtr = NULL;
static D3DXMatrixPerspectiveFovLHFunc D3DXMatrixPerspectiveFovLHPtr = NULL;
static D3DXMatrixRotationYFunc D3DXMatrixRotationYPtr = NULL;

extern "C" void startupdirect3d(int screenwidth, int screenheight, BOOL windowed,
                                SCANLINEMODE scanlinemode, FILTER filter, int cropamount)
{	
	// look for the newest, oldest, then generic versions
	if ((hD3DX = LoadLibrary("d3dx9_42.dll")) != NULL || (hD3DX = LoadLibrary("d3dx9_24.dll")) != NULL || (hD3DX = LoadLibrary("d3dx9.dll")) != NULL)
	{
		D3DXMatrixLookAtLHPtr = (D3DXMatrixLookAtLHFunc) GetProcAddress(hD3DX, "D3DXMatrixLookAtLH");
		D3DXMatrixPerspectiveFovLHPtr = (D3DXMatrixPerspectiveFovLHFunc) GetProcAddress(hD3DX, "D3DXMatrixPerspectiveFovLH");
		D3DXMatrixRotationYPtr = (D3DXMatrixRotationYFunc) GetProcAddress(hD3DX, "D3DXMatrixRotationY");
		// unlikely failure to load function pointers
		if (D3DXMatrixLookAtLHPtr == NULL || D3DXMatrixPerspectiveFovLHPtr == NULL || D3DXMatrixRotationYPtr == NULL)
		{
			FreeLibrary(hD3DX);
			hD3DX = NULL;
			Log_print("Extended Direct3D functions disabled - load failure");
		}
	}
	else
	{
		Log_print("Extended Direct3D functions disabled - updated DirectX runtime needed");
	}

	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	initpresentparams(screenwidth, screenheight, windowed);

	// create the D3D device
	d3d->CreateDevice(D3DADAPTER_DEFAULT,
					  D3DDEVTYPE_HAL,
					  hWndMain,
					  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
					  &d3dpp,
					  &d3d_device);
	
	initdevice(scanlinemode, filter, cropamount);

    return;
}

// Function used when we need to change present params
void resetdevice(int screenwidth, int screenheight, BOOL windowed, 
                 SCANLINEMODE scanlinemode, FILTER filter, int cropamount)
{	
	texture_buffer->Release();    // close and release the texture
	initpresentparams(screenwidth, screenheight, windowed);
	d3d_device->Reset(&d3dpp);	
 	initdevice(scanlinemode, filter, cropamount);
	return;
}

void initpresentparams(int screenwidth, int screenheight, BOOL windowed)
{
   	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = windowed;  			
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hWndMain;
	d3dpp.BackBufferWidth = screenwidth;
	d3dpp.BackBufferHeight = screenheight;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	// Only need this in fullscreen mode.
	if (!windowed)	{
    		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	}

	return;
}

void initdevice(SCANLINEMODE scanlinemode, FILTER filter, int cropamount)
{
	UINT texturesize; 
	D3DTEXTUREFILTERTYPE filtertype;
    int bufferoffset = 48 + (cropamount * 2);
	
	// set the type of interpolation filtering selected by the user
	// only bilinear or none are available.
	if (filter == BILINEAR)
		filtertype = D3DTEXF_LINEAR;
	else
		filtertype = D3DTEXF_POINT;

    	d3d_device->SetRenderState(D3DRS_LIGHTING, FALSE);    // turn off the 3D lighting
    	d3d_device->SetRenderState(D3DRS_ZENABLE, TRUE);    // turn on the z-buffer
    	d3d_device->SetSamplerState( 0, D3DSAMP_MAGFILTER, filtertype);
    	d3d_device->SetFVF(CUSTOMFVF);

	// In Direct3D we paint the Atari screen onto a texture that overlays a vertex buffer polygon.
    // Although most newer graphics cards support rectangular textures of variable size,
	// for compatibility and performance reasons, it is better that the texture be square with
	// sides that are a power of two.  Since the size of the bitmap changes depending
	// on the scanline mode, we set that here.  We also set the clipping parameters here
	// that we'll use in the vertex buffer init to only map the drawn portion of the
    // texture to the vertex buffer.
 
	switch (scanlinemode)
	{
		case NONE:   // we are just mapping the standard 320x240 Atari screen bitmap.
			texturesize = 512;
			texturehorizclip = ((Screen_WIDTH - bufferoffset) / (float)texturesize);
			texturevertclip = (Screen_HEIGHT / (float)texturesize);
			break;
		case LOW:    // LOW res scanlines produces a 320x480 pixel bitmap	
			texturesize = 512;
			texturehorizclip = ((Screen_WIDTH - bufferoffset) / (float)texturesize);
			texturevertclip = ((Screen_HEIGHT * 2 + 1) / (float)texturesize);
			break;
		case MEDIUM: // MEDIUM res scanlines produces a 320x720 pixel bitmap
			texturesize = 1024;
			texturehorizclip = ((Screen_WIDTH - bufferoffset) / (float)texturesize);
			texturevertclip = ((Screen_HEIGHT * 3 + 2) / (float)texturesize);
			break;
		case HIGH:   // HIGH res canlines produces a 320x960 pixel bitmap
			texturesize = 1024;
			texturehorizclip = ((Screen_WIDTH - bufferoffset) / (float)texturesize);
			texturevertclip = ((Screen_HEIGHT * 4 + 3) / (float)texturesize);
			break;
			
	}

	// now lets go ahead and create a blank texture of the appropriate dimensions
	d3d_device->CreateTexture(texturesize, texturesize, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture_buffer, NULL );
	return;
}

// this function creates the 2D vertex buffer polygon and attaches the texture to it
void init_vertices(float x, float y, float z)
{
	// Create the vertices.
	// What we're actually doing here is specifying two right triangles
	// attached to one another to form a wireframe rectangle. We also specify
	// that our texture (the screen) should be mapped to extend to the edges
	// of this rectangle.  This is basically how we create a 2D surface with 
	// a 3D toolset.

	struct CUSTOMVERTEX vertex_def[] =
	{
		{-x, y, z, 0xffffffff, 0, 0,},
		{x, y, z, 0xffffffff, texturehorizclip, 0,},
		{-x, -y, -z, 0xffffffff, 0, texturevertclip,},
		{x, -y, -z, 0xffffffff, texturehorizclip, texturevertclip,},
	};

	d3d_device->CreateVertexBuffer(4*sizeof(CUSTOMVERTEX),
								0,
								CUSTOMFVF,
								D3DPOOL_MANAGED,
								&vertex_buffer,
								NULL);

	VOID* pVoid;    // a void pointer

	// lock vertex_buffer and load the vertices into it
	vertex_buffer->Lock(0, 0, (void**)&pVoid, 0);
	memcpy(pVoid, vertex_def, sizeof(vertex_def));	
	vertex_buffer->Unlock();

	return; 
}

// this is the function used to render each frame
void refreshv_direct3d(UBYTE *scr_ptr, FRAMEPARAMS *fp)
{
	// clear the contents of the vertex buffer polygon and make it black 
    d3d_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    d3d_device->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
  
	// If the caller has set fp->d3dRefresh to true it means that the vertex
	// buffer has changed (usually because the window or texture size has changed).
	// In these cases we will re-init the vertices prior to building the frame.
	if (fp->d3dRefresh) 
	{
    		if (hD3DX != NULL)
		{
			init_vertices(fp->d3dWidth, 0, fp->d3dHeight);
		}
		else
		{
			init_vertices(fp->d3dWidth, fp->d3dHeight, 0);
		}
		fp->d3dRefresh = FALSE;
	}
   
	// We can now unlock the backbuffer and start to draw our frame
    d3d_device->BeginScene();
	
    // refresh the texture buffer for the frame
	// this function call actually draws the contents of the Atari screen buffer
	// to the Direct3D backbuffer.
    refresh_frame(scr_ptr, fp);

	// set the texture
    d3d_device->SetTexture(0, texture_buffer);

    // select the vertex buffer to display
    d3d_device->SetStreamSource(0, vertex_buffer, 0, sizeof(CUSTOMVERTEX));

	//***********************************************************************
	// This next section supports the 3D screensaver and tilt functions by
	// using some simple matrix tranformations on the vertex buffer polygon.
	// Note: If you want to remove this 3D screensaver effect and operate
	// in a more purely 2D view, change the init_vertices statement above to
	// 	init_vertices(fp->d3dWidth, fp->d3dHeight, 0);
	// and remove all statements from here to the end of this
	// 3D Matrix Transformations section.
	//***********************************************************************
    if (hD3DX != NULL)
    {

	D3DXMATRIX matView;    
	
	static float index = pi;    // rotation amount
	static float cy = 2.41f;    // amount of zoom
	static float cz = 0.00001f; // amount of tilt 
	static float by = 0.0;

	if (fp->screensaver)  
	{
		fp->tiltlevel = TILTLEVEL0; // make sure tiltlevel is disabled		

		// this transformation creates and animates the 3D screensaver
		
		if (cy > 1.5f)
			cy -= 0.002f;     // zoom
		if (cz < 2.2)
			cz += 0.005f;	  // tilt

		index+=0.005f; // rotation index
	}
	else if (fp->tiltlevel != TILTLEVEL0)
	{
		index = pi;		

		switch (fp->tiltlevel)
		{
			case TILTLEVEL1:
				if (cz < 1.0)
					cz += 0.012f;	  // tilt
				if (by > -0.2f)
					by -= 0.003f;     // slide up
				break;
			case TILTLEVEL2:
				if (cz < 1.5)
					cz += 0.016f;	  // tilt
				if (cy > 2.3f)
					cy -= 0.004f;     // zoom
				break;
			case TILTLEVEL3:
				if (cz < 1.8)
					cz += 0.012f;	  // tilt
				if (cy > 2.1f)
					cy -= 0.008f;     // zoom
				if (by > -0.25f)
					by -= 0.002f;     // slide up
				break;
		}
	}
	else  
	{
		// this transformation creates a standard 2D view

		index = pi;

		cy = 2.41f;
		cz = 0.00001f;
		by = 0.0;
	}    	

 	// Create the transform matView
	D3DXMatrixLookAtLHPtr(&matView,
    			   &D3DXVECTOR3 (0.0f, cy, cz),        // tilt & zoom the camera
    			   &D3DXVECTOR3 (0.0f, by, 0.0f),    // raise & lower the camera
    			   &D3DXVECTOR3 (0.0f, 1.0, 0.0f));    // flip the camera	

	d3d_device->SetTransform(D3DTS_VIEW, &matView);    // set the view transform to matView 

	// set the projection transform
	D3DXMATRIX matProjection;    // the projection transform matrix
	D3DXMatrixPerspectiveFovLHPtr(&matProjection,
							   D3DXToRadian(45),    // the horizontal field of view
							   1.0f,   // aspect ratio
							   1.0f,   // the near view-plane
							   100.0f);    // the far view-plane
	d3d_device->SetTransform(D3DTS_PROJECTION, &matProjection); // set the projection

	// set the world transform 
	D3DXMATRIX matRotateY;    // a matrix to store the rotation for each triangle
	D3DXMatrixRotationYPtr(&matRotateY, index);    // the rotation matrix
	d3d_device->SetTransform(D3DTS_WORLD, &(matRotateY));    // set the world transform
    }

	//***********************************************************************
	// End 3D transformations
	//***********************************************************************

	// draw the fully rendered and transformed surface to the backbuffer
	d3d_device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

	// We now lock the backbuffer 
    d3d_device->EndScene(); 

	// Now tell Direct3D to flip the image to the screen.
    d3d_device->Present(NULL, NULL, NULL, NULL);

    return;
}

// shutdown the Direct3D engine
extern "C" void shutdowndirect3d(void)
{
	vertex_buffer->Release();    // close and release the vertex buffer
	texture_buffer->Release();    // close and release the texture
	d3d_device->Release();    // close and release the 3D device
	d3d->Release();    // close and release Direct3D

	if (hD3DX != NULL)
	{
		FreeLibrary(hD3DX);
		hD3DX = NULL;
	}

	return;
}

// Copy the raw screen buffer data to the texture buffer
void refresh_frame(UBYTE *scr_ptr, FRAMEPARAMS *fp)
{
	int i, j, k, cycles;
	COLORREF cr;
	int bmWidth = Screen_WIDTH - 48 - (fp->cropamount * 2);;
	int bmHeight = Screen_HEIGHT;
	
    D3DLOCKED_RECT d3dlr;
    texture_buffer->LockRect(0, &d3dlr, 0, 0);
	
	DWORD* pixels = (DWORD*)d3dlr.pBits;
	int scanlinelength = d3dlr.Pitch / 4;
	bmHeight *= fp->scanlinemode;

	// indent screen buffer if cropping
    scr_ptr += fp->cropamount;
	
	// write to the texture map, inserting scanlines as appropriate
	fp->scanlinemode == NONE ? cycles = 1 : cycles = fp->scanlinemode - 1;
	for (i = 0; i < bmHeight; i+=fp->scanlinemode) {
		for (j = 0; j < bmWidth; j++) {
			cr = Colours_table[*scr_ptr++];
			for (k = 0; k < cycles ; k++) {			
				pixels[(i + k) * scanlinelength + j] = cr;
			}
		}
		scr_ptr += 48 + (fp->cropamount * 2);;
	}

	texture_buffer->UnlockRect(0);
}

