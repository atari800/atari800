/*
 * Preferences.java - Preference activity for the emulator
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

import java.io.File;

import android.preference.PreferenceActivity;
import android.os.Bundle;
import android.preference.Preference;
import android.util.Log;
import android.content.SharedPreferences;
import android.preference.Preference.OnPreferenceClickListener;
import android.net.Uri;
import android.content.Intent;
import android.app.AlertDialog;
import android.webkit.WebView;


public final class Preferences extends PreferenceActivity implements Preference.OnPreferenceChangeListener
{
	private static final String TAG = "Preferences";
	private static final String[] PREF_KEYS = { "up", "down", "left", "right", "fire" };
	private static final int ACTIVITY_FSEL = 1;
	private int[] prefValues = new int[5];
	private SharedPreferences _sp;
	private AlertDialog _aboutdlg = null;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		addPreferencesFromResource(R.xml.preferences);
		_sp = getPreferenceManager().getSharedPreferences();

		for (String s: PREF_KEYS)
			findPreference(s).setOnPreferenceChangeListener(this);

		findPreference("rompath").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				String val = _sp.getString("rompath", null);
				Uri u = (val == null) ? null : Uri.fromFile(new File(val));
				startActivityForResult(new Intent(FileSelector.ACTION_OPEN_PATH, u,
									   Preferences.this, FileSelector.class), ACTIVITY_FSEL);
				return true;
			}
		});

		findPreference("about").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				aboutDialog();
				return true;
			}
		});

		findPreference("help").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				startActivity(new Intent(Intent.ACTION_VIEW,
							  Uri.parse("http://pocketatari.atari.org/android/index.html#manual")));
				return true;
			}
		});
	}

	@Override
	public boolean onPreferenceChange(Preference pref, Object v) {
		int k = (Integer) v;
		Log.d(TAG, "Change" + k);

		if (k >= 0) {	// check mappings
			for (String key: PREF_KEYS)
				if (_sp.getInt(key, -1) == k)
					return false;
			return true;
		} else {		// swap mappings
			k = -k;
			for (String key: PREF_KEYS)
				if (_sp.getInt(key, -1) == k) {
					SharedPreferences.Editor e = _sp.edit();
					e.putInt(key, _sp.getInt(pref.getKey(), -1));
					e.commit();
					((KeymapPreference) findPreference(key)).updateSum();
				}
			return true;
		}
	}

	@Override
	protected void onActivityResult(int reqc, int resc, Intent data) {
		switch (reqc) {
		case ACTIVITY_FSEL:
			if (resc != RESULT_OK) break;
			SharedPreferences.Editor e = _sp.edit();
			e.putString("rompath", data.getData().getPath());
			e.commit();
		}
	}

	private void aboutDialog() {
		WebView v = new WebView(this);
		v.loadData(String.format(getString(R.string.aboutmsg),
					MainActivity._pkgversion, MainActivity._coreversion), "text/html", "utf-8");
		v.setVerticalScrollBarEnabled(true);
		_aboutdlg = new AlertDialog.Builder(this)
				.setTitle(R.string.about)
				.setIcon(R.drawable.icon)
				.setView(v)
				.setInverseBackgroundForced(true)
				.setPositiveButton(R.string.ok, null)
				.show();
	}
}
