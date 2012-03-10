/*
 * SliderPreference.java - Simple custom preference dialog with a slider
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
import android.util.AttributeSet;
import android.content.res.TypedArray;
import android.widget.SeekBar;
import android.view.LayoutInflater;
import android.widget.TextView;
import android.view.View;
import android.content.DialogInterface;
import android.util.Log;


public final class SliderPreference extends DialogPreference implements SeekBar.OnSeekBarChangeListener
{
	private int _min, _max, _def;
	private String _suffix;
	private TextView _txtSetting = null;
	private int _sliderValue;

	public SliderPreference(Context context, AttributeSet attrs) {
		super(context, attrs);
		
		TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.SliderPreference);
		_min = a.getInt(R.styleable.SliderPreference_min, 10);
		_max = a.getInt(R.styleable.SliderPreference_max, 99);
		_suffix = a.getString(R.styleable.SliderPreference_suffix);
		if (_suffix == null)	_suffix = "%";
		a.recycle();

		setPositiveButtonText(R.string.ok);
		setNegativeButtonText(R.string.cancel);
		setDialogTitle(getTitle());
	}

	@Override
	protected Object onGetDefaultValue(TypedArray a, int i) {
		_def = a.getInt(i, 10);
		return _def;
	}

	@Override
	protected void onSetInitialValue(boolean restore, Object def) {
		if (restore == false)
			persistInt((Integer) def);
	}

	@Override
	protected View onCreateDialogView() {
		LayoutInflater inf = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		View v = inf.inflate(R.layout.slider_dialog, null);
		_txtSetting = (TextView) v.findViewById(R.id.setting);
		SeekBar s = (SeekBar) v.findViewById(R.id.slider);
		s.setOnSeekBarChangeListener(this);
		s.setMax(_max - _min);
		s.setProgress(getPersistedInt(_def) - _min);
		onProgressChanged(s, getPersistedInt(_def) - _min, false);
		return v;
	}

	@Override
	public void onClick(DialogInterface d, int w) {
		if (w == DialogInterface.BUTTON_POSITIVE && callChangeListener(_sliderValue))
			persistInt(_sliderValue);
		_txtSetting = null;
	}

	@Override
	public void onProgressChanged(SeekBar s, int p, boolean u) {
		_sliderValue = p + _min;
		_txtSetting.setText(_sliderValue + _suffix);
	}

	@Override public void onStartTrackingTouch(SeekBar seekBar) {}
	@Override public void onStopTrackingTouch(SeekBar seekBar) {}
}
