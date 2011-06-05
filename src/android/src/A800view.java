/*
 * A800view.java - atari screen view
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

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.KeyEvent;
import android.util.Log;
import android.view.KeyCharacterMap;
import android.os.Build;
import android.widget.Toast;
import android.view.View;
import android.util.SparseArray;
import static android.view.KeyEvent.*;


public final class A800view extends GLSurfaceView
{
	public static final int KEY_SHIFT     = 256;
	public static final int KEY_CONTROL   = 257;
	public static final int KEY_BACKSPACE = 255;
	public static final int KEY_UP        = 254;
	public static final int KEY_DOWN      = 253;
	public static final int KEY_LEFT      = 252;
	public static final int KEY_RIGHT     = 251;
	public static final int KEY_FIRE      = 250;
	public static final int KEY_ENTER     = 249;
	public static final int KEY_ESCAPE    = 248;
	public static final int KEY_CENTER    = 247;
	public static final int KEY_BT_X      = 246;
	public static final int KEY_BT_Y      = 245;
	public static final int KEY_BT_L1     = 244;
	public static final int KEY_BT_R1     = 243;
	public static final int KEY_BREAK     = 242;
	// keycodes from newer sdks
	public static final int KC_BUTTON_X   = 307;
	public static final int KC_BUTTON_Y   = 308;
	public static final int KC_BUTTON_L1  = 310;
	public static final int KC_BUTTON_R1  = 311;
	

	private static final String TAG = "A800View";
	private A800Renderer _renderer;
	private KeyCharacterMap _keymap;
	private int _key, _meta, _hit;
	private TouchFactory _touchHandler = null;
	private Toast _toastquit;
	private Integer _xkey;

	public A800view(Context context) {
		super(context);

		_renderer = new A800Renderer();
		setRenderer(_renderer);
		_renderer.prepareToast(context);

		setFocusable(true);
		setFocusableInTouchMode(true);
		requestFocus();

		_keymap = KeyCharacterMap.load(KeyCharacterMap.BUILT_IN_KEYBOARD);

		if (Integer.parseInt(Build.VERSION.SDK) < Build.VERSION_CODES.ECLAIR)
			_touchHandler = new SingleTouch();
		else
			_touchHandler = new MultiTouch();

		_toastquit = Toast.makeText(context, R.string.pressback, Toast.LENGTH_SHORT);
	}

	public void pause(boolean p) {
		setRenderMode(p ? GLSurfaceView.RENDERMODE_WHEN_DIRTY :
						  GLSurfaceView.RENDERMODE_CONTINUOUSLY);
	}

	// Touch input
	@Override
	public boolean onTouchEvent(final MotionEvent ev) {
		return _touchHandler.onTouchEvent(ev);
	}

	abstract static class TouchFactory {
		public abstract boolean onTouchEvent(MotionEvent ev);
	};

	private static final class SingleTouch extends TouchFactory {
		private int _x1, _y1, _s1;
		private int _action, _actioncode;


		@Override
		public boolean onTouchEvent(final MotionEvent ev) {
			_action = ev.getAction();
			_actioncode = _action & MotionEvent.ACTION_MASK;
			_x1 = (int) ev.getX();
			_y1 = (int) ev.getY();
			_s1 = 1;
			if (_actioncode == MotionEvent.ACTION_UP)
				_s1 = 0;

			NativeTouch(_x1, _y1, _s1, -1000, -1000, 0);
			return true;
		}
	}

	private static final class MultiTouch extends TouchFactory {
		private int _x1, _y1, _s1, _x2, _y2, _s2;
		private int _action, _actioncode, _ptrcnt;

		@Override
		public boolean onTouchEvent(final MotionEvent ev) {
			_action = ev.getAction();
			_actioncode = _action & MotionEvent.ACTION_MASK;
			_ptrcnt = ev.getPointerCount();
			_x1 = (int) ev.getX(0);
			_y1 = (int) ev.getY(0);
			_s1 = 1;
			if (_ptrcnt > 1) {
				_x2 = (int) ev.getX(1);
				_y2 = (int) ev.getY(1);
				_s2 = 1;
			} else {
				_x2 = -1000;
				_y2 = -1000;
				_s2 = 0;
			}
			if (_actioncode == MotionEvent.ACTION_UP) {
				_s1 = _s2 = 0;
			} else if (_actioncode == MotionEvent.ACTION_POINTER_UP) {
				if ( (_action >> MotionEvent.ACTION_POINTER_ID_SHIFT) == 0)
					_s1 = 0;
				else
					_s2 = 0;
			}

			NativeTouch(_x1, _y1, _s1, _x2, _y2, _s2);
			return true;
		}
	}

	// Key input
	@Override
	public boolean onKeyDown(int kc, final KeyEvent ev) {
		return doKey(kc, ev);
	}

	@Override
	public boolean onKeyUp(int kc, final KeyEvent ev) {
		return doKey(kc, ev);
	}

	private boolean doKey(int kc, final KeyEvent ev) {
		_hit =(ev.getAction() == ACTION_DOWN) ? 1 : 0;

		if (kc == KEYCODE_BACK && _hit == 1) {
			if (_toastquit.getView().getWindowVisibility() == View.VISIBLE) {
				_toastquit.cancel();
				((MainActivity) getContext()).finish();
			} else
				_toastquit.show();
			return true;
		}

		_xkey = XLATKEYS.get(kc);
		if (_xkey != null)
			_key = _xkey.intValue();
		else {
			_meta = ev.getMetaState();
			if ((_meta & KeyEvent.META_SHIFT_RIGHT_ON) == KeyEvent.META_SHIFT_RIGHT_ON)
				_meta &= ~(KeyEvent.META_SHIFT_RIGHT_ON | KeyEvent.META_SHIFT_ON);
			_key = _keymap.get(kc, _meta);
			if (_key == 0)
				return false;
		}

		//Log.d(TAG, String.format("key %d %d -> %d", ev.getAction(), kc, _key));

		NativeKey(_key, _hit);

		return true;
	}

	private native static void NativeTouch(int x1, int y1, int s1, int x2, int y2, int s2);
	private native void NativeKey(int keycode, int status);

	public static final SparseArray<Integer> XLATKEYS = new SparseArray<Integer>(14);
	static {
		XLATKEYS.put(KEYCODE_DPAD_UP,		KEY_UP);
		XLATKEYS.put(KEYCODE_DPAD_DOWN,		KEY_DOWN);
		XLATKEYS.put(KEYCODE_DPAD_LEFT,		KEY_LEFT);
		XLATKEYS.put(KEYCODE_DPAD_RIGHT,	KEY_RIGHT);
		XLATKEYS.put(KEYCODE_DPAD_CENTER,	KEY_BREAK);
		XLATKEYS.put(KEYCODE_SEARCH,		KEY_FIRE);
		XLATKEYS.put(KEYCODE_SHIFT_LEFT,	KEY_SHIFT);
		XLATKEYS.put(KEYCODE_SHIFT_RIGHT,	KEY_CONTROL);
		XLATKEYS.put(KEYCODE_DEL,			KEY_BACKSPACE);
		XLATKEYS.put(KEYCODE_ENTER,			KEY_ENTER);
		XLATKEYS.put(KC_BUTTON_X,			KEY_BT_X);
		XLATKEYS.put(KC_BUTTON_Y,			KEY_BT_Y);
		XLATKEYS.put(KC_BUTTON_L1,			KEY_BT_L1);
		XLATKEYS.put(KC_BUTTON_R1,			KEY_BT_R1);
	}
}
