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

package name.nick.jubanka.colleen;

import android.preference.DialogPreference;
import android.content.Context;
import android.view.KeyCharacterMap;
import android.view.View;
import android.view.ViewGroup;
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
import android.widget.EditText;
import android.R.style;
import android.util.TypedValue;
import android.view.LayoutInflater;
import static android.view.KeyEvent.*;
import static name.nick.jubanka.colleen.A800view.*;


public final class KeymapPreference extends DialogPreference
{
	private static final String TAG = "KeyPreference";

	private static final int DEFKEY = 'a';
	private static final String DEFKEYEXT = "-1,-1";
	private static final int EXTSTR_ACTION = 0;
	private static final int EXTSTR_KEY = 1;

	private KeyCharacterMap _keymap;
	private int _def;
	private String _defext = null;
	private boolean _extended = false;

	public KeymapPreference(Context c, AttributeSet a) {
		super(c, a);
		_keymap = KeyCharacterMap.load(KeyCharacterMap.BUILT_IN_KEYBOARD);

		_extended = Boolean.parseBoolean( c.obtainStyledAttributes(a, R.styleable.KeymapPreference)
										   .getString(R.styleable.KeymapPreference_ext) );

		setNegativeButtonText(R.string.cancel);
		setDialogTitle(getTitle());
	}

	@Override
	protected Object onGetDefaultValue(TypedArray a, int i) {
		try {
			_def = a.getInt(i, DEFKEY);
			return _def;
		} catch (NumberFormatException e) {
			_defext = a.getString(i);
			if (_defext == null)
				_defext = DEFKEYEXT;
			return _defext;
		}
	}

	@Override
	protected void onSetInitialValue(boolean restore, Object def) {
		if (!restore)
			if (!_extended)
				persistInt((Integer) def);
			else
				persistString((String) def);
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

	private final class SnoopTextView extends EditText
	{
		public SnoopTextView(Context c) {
			super(c);
			setText( (!_extended) ? R.string.pref_keymapmsg : R.string.pref_keymapmsg1);
			setCursorVisible(false);
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
			Log.d(TAG, "key " + kc);

			for (int res: RESKEYS)
				if (res == kc)
					return false;

			final int k = xlatKey(kc);

			if (k == 0)
				return false;

			if (k == getKeymap() && !_extended) {
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
							if (!_extended) {
								Dialog d1 = getDialog();
								if (d1 != null)
									d1.dismiss();
							} else
								showExtDialog();
						}
						})
					.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface d, int i) {
							Dialog d1 = getDialog();
							if (d1 != null)
								d1.dismiss();
						}
						})
					.show();
				return true;
			}

			setKeymap(k);
			if (!_extended)
				getDialog().dismiss();
			else
				showExtDialog();
			return true;
		}
	}

	private void showExtDialog()
	{
		LayoutInflater inf = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		new AlertDialog.Builder(getContext())
			.setTitle(getTitle())
			.setView(inf.inflate(R.layout.extended_keymap, null))
			.setCancelable(false)
			.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface d, int i) {
					CharSequence txt = ((TextView) ((Dialog) d).findViewById(R.id.keyinput)).getText();
					if (txt == null || txt.length() != 1) {
						getDialog().dismiss();
						return;
					}
					persistString( buildExtPref(parseExtPref(EXTSTR_ACTION), (int) txt.charAt(0)) );
					updateSum();
					d.dismiss();
					getDialog().dismiss();
				}
				})
			.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface d, int i) {
					getDialog().dismiss();
				}
				})
			.show();
	}

	public void updateSum() {
		StringBuilder str = new StringBuilder();
		str.append( getContext()
					.getString(_extended ? R.string.pref_keymap_controller : R.string.pref_keymap_current) );
		str.append(" ");
		str.append( getKeyname(getKeymap()) );
		if (_extended) {
			str.append(" ");
			str.append( getContext().getString(R.string.pref_keymap_mappedto) );
			str.append(" ");
			str.append( getKeyname(parseExtPref(EXTSTR_KEY)) );
		}
		setSummary(str);
	}

	private int xlatKey(int kc) {
		int k;

		Integer xlat = A800view.XLATKEYS.get(kc);
		if (xlat != null)
			k = xlat.intValue();
		else
			k = _keymap.get(kc, 0);
		return k;
	}

	public void setKeymap(int k) {
		if (!_extended)
			persistInt(k);
		else
			persistString( buildExtPref(k, parseExtPref(EXTSTR_KEY)) );
		updateSum();
	}

	public void setDefaultKeymap() {
		if (!_extended)		return;
		persistString(DEFKEYEXT);
		updateSum();
	}

	public int getKeymap() {
		if (!_extended)
			return getPersistedInt(-128) == -128 ? _def : getPersistedInt(-1);
		else
			return parseExtPref(EXTSTR_ACTION);
	}

	private int parseExtPref(int part) {
		String str = getPersistedString(null);
		return Integer.parseInt( ((str != null) ? str : _defext).split(",")[part] );
	}

	private String buildExtPref(int k1, int k2) {
		return Integer.toString(k1) + "," + Integer.toString(k2);
	}

	private String getKeyname(int k) {
		String name = null;
		name = KEYNAMES.get(k);
		if (name != null)				return name;
		if (k > 31 && k < 127)			return Character.toString((char) k);
		return "ASCII " + k;
	}

	// Real programmers *hate* data entry ;-)
	private static final SparseArray<String> KEYNAMES = new SparseArray<String>(13);
	static {
		KEYNAMES.put(-1,			"None");
		KEYNAMES.put(' ',			"Space");
		KEYNAMES.put(KEY_DOWN,		"Down arrow");
		KEYNAMES.put(KEY_LEFT,		"Left arrow");
		KEYNAMES.put(KEY_RIGHT,		"Right arrow");
		KEYNAMES.put(KEY_UP,		"Up arrow");
		KEYNAMES.put(KEY_ENTER,		"Enter");
		KEYNAMES.put(KEY_BACKSPACE,	"Del");
		KEYNAMES.put(KEY_BT_X,		"Button X");
		KEYNAMES.put(KEY_BT_Y,		"Button Y");
		KEYNAMES.put(KEY_BT_L1,		"Button L1");
		KEYNAMES.put(KEY_BT_R1,		"Button R1");
		KEYNAMES.put(KEY_BREAK,		"DPAD Enter");
	}
	private static final int[] RESKEYS = {
		KEYCODE_SHIFT_LEFT, KEYCODE_SHIFT_RIGHT, KEYCODE_VOLUME_UP, KEYCODE_VOLUME_DOWN,
		KEYCODE_MENU, KEYCODE_SEARCH, KEYCODE_BACK, KEYCODE_HOME, KEYCODE_POWER,
		KEYCODE_CALL, KEYCODE_ENDCALL
	};
}
