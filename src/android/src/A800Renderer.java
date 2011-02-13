/*
 * A800Renderer.java - opengl graphics frontend to android
 *
 * Copyright (C) 2010 Kostas Nakos
 * Copyright (C) 2010 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

package name.nick.jubanka.atari800;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import android.opengl.GLSurfaceView;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Region;
import android.util.Log;
import android.widget.Toast;
import android.content.Context;

public final class A800Renderer implements GLSurfaceView.Renderer
{
	private static final String TAG = "A800Renderer";
	private final int OVL_TEXW = 128;
	private final int OVL_TEXH = 64;
	private int[] _pix;
	private Toast _crashtoast;

	@Override
	public void onSurfaceCreated(GL10 gl, EGLConfig config) {
		_pix = new int[OVL_TEXW * OVL_TEXH];
		generateOverlays();
		NativeGetOverlays();
	}

	@Override
	public void onSurfaceChanged(GL10 gl, int w, int h) {
		gl.glViewport(0, 0, w, h);
		NativeResize(w, h);
	}

	@Override
	public void onDrawFrame(GL10 gl) {
		if (NativeRunFrame())
			_crashtoast.show();
	}

	public void prepareToast(Context c) {
		_crashtoast = Toast.makeText(c, R.string.cimcrash, Toast.LENGTH_LONG);
	}

	private void generateOverlays() {
		RectF  r;

		Paint fill = new Paint(0);
		fill.setStyle(Paint.Style.FILL);
		fill.setColor(0xFF0054A8);
		Paint stroke = new Paint(0);
		stroke.setStyle(Paint.Style.STROKE);
		stroke.setStrokeWidth(1);
		stroke.setColor(0xFF001976);

		Bitmap bmp = Bitmap.createBitmap(OVL_TEXW, OVL_TEXH, Bitmap.Config.ARGB_8888);
		bmp.eraseColor(0);
		Canvas can = new Canvas(bmp);

		// Joystick area
		r = new RectF(0, 0, 63, 63);
		can.clipRect(0, 0, 64, 64, Region.Op.REPLACE);
		can.drawRoundRect(r, 6, 6, fill);
		can.drawRoundRect(r, 6, 6, stroke);

		// Fire/Joy point
		r = new RectF(67, 3, 77, 13);
		can.clipRect(64, 0, 79, 15, Region.Op.REPLACE);
		can.drawOval(r, fill);

		// El texto
		can.clipRect(64, 16, 128, 64, Region.Op.REPLACE);
		Paint t = new Paint(Paint.ANTI_ALIAS_FLAG);
		t.setColor(0xFF001976);
		t.setTextSize(10);
		can.drawColor(0x00ffffff);
		can.drawText("START", 65, 24, t);
		can.drawText("SELECT", 65, 34, t);
		can.drawText("OPTION", 65, 44, t);
		can.drawText("RESET", 65, 54, t);
		can.drawText("HELP", 65, 64, t);

		bmp.getPixels(_pix, 0, OVL_TEXW, 0, 0, OVL_TEXW, OVL_TEXH);
		bmp.recycle();
	}

	// Native function declarations
	private native void NativeGetOverlays();
	private native boolean NativeRunFrame();
	private native void NativeResize(int w, int h);
}
