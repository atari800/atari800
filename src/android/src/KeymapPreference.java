/*
 * KeymapPreference.java - Even simpler preference for mapping keys
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

import android.preference.DialogPreference;
import android.content.Context;
import android.view.KeyCharacterMap;
import android.view.View;
import android.view.KeyEvent;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.Log;
import android.content.DialogInterface;
import android.os.Bundle;
import android.app.Dialog;
import android.app.AlertDialog;
import android.util.SparseArray;
import android.widget.TextView;
import android.R.style;
import android.util.TypedValue;
import static android.view.KeyEvent.*;
import static name.nick.jubanka.atari800.A800view.*;


public final class KeymapPreference extends DialogPreference
{
	private static final String TAG = "KeyPreference";
	private static final int DEFKEY = 'a';
	private KeyCharacterMap _keymap;
	int _def;

	public KeymapPreference(Context c, AttributeSet a) {
		super(c, a);
		_keymap = KeyCharacterMap.load(KeyCharacterMap.BUILT_IN_KEYBOARD);
	}

	@Override
	protected Object onGetDefaultValue(TypedArray a, int i) {
		_def = a.getInt(i, DEFKEY);
		return _def;
	}

	@Override
	protected void onSetInitialValue(boolean restore, Object def) {
		if (restore == false)
			persistInt((Integer) def);
	}

	@Override
	public void onBindView(View v) {
		updateSum();
		super.onBindView(v);
	}

	@Override
	protected void showDialog(Bundle state) {
		super.showDialog(state);
		Dialog d = getDialog();
		d.takeKeyEvents(true);
		((AlertDialog) d).getButton(DialogInterface.BUTTON_POSITIVE).setVisibility(View.GONE);
	}

	@Override
	protected View onCreateDialogView() {
		View v = new SnoopTextView(getContext());
		return v;
	}

	private class SnoopTextView extends TextView
	{
		public SnoopTextView(Context c) {
			super(c);
			setText(R.string.pref_keymapmsg);
			setTextAppearance(c, android.R.style.TextAppearance_Medium);
			int pad = (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
													  (float) 10,
													  c.getResources().getDisplayMetrics());
			setPadding(pad, pad, pad, pad);
			setFocusable(true);
			setFocusableInTouchMode(true);
			requestFocus();
		}

		@Override
		public boolean onKeyDown(int kc, KeyEvent ev) {
			Log.d(TAG, "key" + kc);

			for (int res: RESKEYS)
				if (res == kc)
					return false;

			final int k = xlatKey(kc);

			if (k == 0)
				return false;

			if (k == getPersistedInt(-1)) {
				getDialog().dismiss();
				return true;
			}

			if (callChangeListener(new Integer(k)) == false) {
				new AlertDialog.Builder(getContext())
					.setTitle(R.string.warning)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.pref_keymapdupmsg)
					.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface d, int i) {
							callChangeListener(new Integer(-k));
							setKeymap(k);
							d.dismiss();
							getDialog().dismiss();
						}
						})
					.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface d, int i) {
							getDialog().dismiss();
						}
						})
					.show();
				return true;
			}

			setKeymap(k);
			getDialog().dismiss();
			return true;
		}

		@Override
		public boolean onCheckIsTextEditor() { return true; }
	}

	public void updateSum() {
		setSummary("Current: " + ((getPersistedInt(-1) == -1) ?
								  getKeyname(_def) : getKeyname(getPersistedInt(-1))));
	}

	private int xlatKey(int kc) {
		int k;

		Integer xlat = XLATKEYS.get(kc);
		if (xlat != null)
			k = xlat.intValue();
		else
			k = _keymap.get(kc, 0);
		return k;
	}

	private void setKeymap(int k) {
		persistInt(k);
		updateSum();
	}

	private String getKeyname(int k) {
		String name = null;
		name = KEYNAMES.get(k);
		if (name != null)				return name;
		if (k > 31 && k < 127)			return Character.toString((char) k);
		return "ASCII " + k;
	}

	// Real programmers *hate* data entry ;-)
	private static final SparseArray<String> KEYNAMES = new SparseArray<String>();
	static {
		KEYNAMES.put(' ',					"Space");
		KEYNAMES.put(KEY_DOWN,				"Down arrow");
		KEYNAMES.put(KEY_LEFT,				"Left arrow");
		KEYNAMES.put(KEY_RIGHT,				"Right arrow");
		KEYNAMES.put(KEY_UP,				"Up arrow");
		KEYNAMES.put(KEY_ENTER,				"DPAD Enter");
		KEYNAMES.put(KEY_BACKSPACE,			"Del");
	}
	private static final int[] RESKEYS = {
		KEYCODE_SHIFT_LEFT, KEYCODE_SHIFT_RIGHT, KEYCODE_VOLUME_UP, KEYCODE_VOLUME_DOWN,
		KEYCODE_MENU, KEYCODE_SEARCH, KEYCODE_BACK, KEYCODE_HOME, KEYCODE_POWER,
		KEYCODE_CALL, KEYCODE_ENDCALL
	};
	private static final SparseArray<Integer> XLATKEYS = new SparseArray<Integer>();
	static {
		XLATKEYS.put(KEYCODE_DPAD_UP,		KEY_UP);
		XLATKEYS.put(KEYCODE_DPAD_DOWN,		KEY_DOWN);
		XLATKEYS.put(KEYCODE_DPAD_LEFT,		KEY_LEFT);
		XLATKEYS.put(KEYCODE_DPAD_RIGHT,	KEY_RIGHT);
		XLATKEYS.put(KEYCODE_DPAD_CENTER,	KEY_ENTER);
		XLATKEYS.put(KEYCODE_DEL,			KEY_BACKSPACE);
	}
}
