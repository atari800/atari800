/*
 * Preferences.java - Preference activity for the emulator
 *
 * Copyright (C) 2014 Kostas Nakos
 * Copyright (C) 2014 Atari800 development team (see DOC/CREDITS)
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

import java.io.File;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;

import android.preference.PreferenceActivity;
import android.os.Bundle;
import android.preference.Preference;
import android.util.Log;
import android.content.SharedPreferences;
import android.preference.Preference.OnPreferenceClickListener;
import android.content.Intent;
import android.net.Uri;
import android.app.AlertDialog;
import android.webkit.WebView;
import android.app.Dialog;
import android.content.DialogInterface;
import android.preference.EditTextPreference;
import android.widget.Toast;
import android.content.res.Resources;
import android.preference.CheckBoxPreference;

@SuppressWarnings("deprecation")
public final class Preferences extends PreferenceActivity implements Preference.OnPreferenceChangeListener
{
	private static final String TAG = "Preferences";
	private static final String[] PREF_KEYS = { "up", "down", "left", "right", "fire",
												"actiona", "actionb", "actionc" };
	private static final String PD_RESNAME = "pd2012";
	private static final int ACTIVITY_IMPORT_ROMS = 4;
	private static final int DLG_ABOUT = 1;
	private static final int DLG_RESET = 2;
	private static final int DLG_OVRWR = 3;
	private SharedPreferences _sp;
	private String _svstfname = null;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		KeymapPreference kp;

		super.onCreate(savedInstanceState);

		addPreferencesFromResource(R.xml.preferences);
		_sp = getPreferenceManager().getSharedPreferences();

		for (String s: PREF_KEYS) {
			kp = (KeymapPreference) findPreference(s);
			kp.setOnPreferenceChangeListener(this);
			kp.updateSum();
		}

		/* Import ROM BIOS: open SAF file picker with multi-select */
		findPreference("importroms").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
					.addCategory(Intent.CATEGORY_OPENABLE)
					.setType("*/*")
					.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
					.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION),
					ACTIVITY_IMPORT_ROMS);
				return true;
			}
		});

		findPreference("about").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				showDialog(DLG_ABOUT);
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

		findPreference("resetactions").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				showDialog(DLG_RESET);
				return true;
			}
		});

		findPreference("savestate").setOnPreferenceChangeListener(this);
		findPreference("loadstate").setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference p) {
				File savesDir = getDir("saves", MODE_PRIVATE);
				final String[] files = savesDir.list(new java.io.FilenameFilter() {
					@Override
					public boolean accept(File dir, String name) {
						return name.endsWith(".a8s");
					}
				});
				if (files == null || files.length == 0) {
					Toast.makeText(Preferences.this, R.string.loadstatenone, Toast.LENGTH_SHORT).show();
					return true;
				}
				new AlertDialog.Builder(Preferences.this)
					.setTitle(R.string.loadstatedlg)
					.setItems(files, new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface d, int which) {
							String path = new File(savesDir, files[which]).getAbsolutePath();
							if (NativeLoadState(path))
								Toast.makeText(Preferences.this,
									String.format(getString(R.string.loadstateok), files[which]),
									Toast.LENGTH_SHORT).show();
							else
								Toast.makeText(Preferences.this, R.string.loadstateerror,
									Toast.LENGTH_SHORT).show();
						}
					})
					.show();
				return true;
			}
		});

		if (_sp.getBoolean("a800fns", false) == true)
			findPreference("a800fns").setSummary(getString(R.string.pref_a800fns_sum_ena));
		findPreference("a800fns").setOnPreferenceChangeListener(this);

		Preference p = findPreference("launchpd");
		if (p != null) {
			p.setOnPreferenceClickListener(new OnPreferenceClickListener() {
				@Override
				public boolean onPreferenceClick(Preference p) {
					Resources res = Preferences.this.getResources();
					int id = res.getIdentifier(PD_RESNAME, "raw", Preferences.this.getPackageName());
					if (id != 0) {
						InputStream is = res.openRawResource(id);
						byte pddata[];
						try {
							pddata = new byte[is.available()];
							is.read(pddata);
							is.close();
						} catch (IOException e) { 
							Log.d(TAG, "IO exception while reading resouce");
							return true;
						}
						if (! NativeBootPD(pddata, pddata.length))
							Toast.makeText(Preferences.this, R.string.pdbooterror, Toast.LENGTH_LONG).show();
						else {
							((CheckBoxPreference) Preferences.this.findPreference("plandef")).setChecked(true);
							Toast.makeText(Preferences.this, R.string.pdreminder, Toast.LENGTH_LONG).show();
							Preferences.this.finish();
						}
					} else {
						Log.d(TAG, "PD2012 resource not found");
					}
					return true;
				}
			});
			if (getResources().getIdentifier(PD_RESNAME, "raw", getPackageName()) == 0)
				p.setEnabled(false);
		}

		findPreference("forceAT").setEnabled(NativeOSLSound() || ((CheckBoxPreference) findPreference("forceAT")).isChecked());
	}

	private boolean saveState(boolean force) {
		String path = getDir("saves", MODE_PRIVATE).getAbsolutePath();

		if (!force && new File(path, _svstfname).exists())
			return false;

		if (!NativeSaveState(path + '/' + _svstfname)) {
			Toast.makeText(this, R.string.savestateerror, Toast.LENGTH_LONG).show();
			return true;
		}

		Toast.makeText(this, R.string.savestateok, Toast.LENGTH_LONG).show();
		return true;
	}

	@Override
	public boolean onPreferenceChange(Preference p, Object v) {
		if (p.getKey().equals("savestate")) {
			_svstfname = (String) v + ".a8s";
			if (!saveState(false))
				showDialog(DLG_OVRWR);
			return true;
		} else if (p.getKey().equals("a800fns")) {
			if ((Boolean) v)
				p.setSummary(getString(R.string.pref_a800fns_sum_ena));
			else
				p.setSummary(getString(R.string.pref_a800fns_sum_dis));
			return true;
		} else {
			int k = (Integer) v;
			KeymapPreference pref;

			Log.d(TAG, "Change " + k);
			for (String key: PREF_KEYS) {
				if (key.equals(p.getKey()))	continue;
				pref = (KeymapPreference) findPreference(key);
				if (k >= 0) {	// check mappings
					if (pref.getKeymap() == k)
						return false;
				} else {		// swap mappings
					if (pref.getKeymap() == -k) {
						pref.setKeymap( ((KeymapPreference) p).getKeymap() );
						return true;
					}
				}
			}
			return true;
		}
	}

	@Override
	protected void onActivityResult(int reqc, int resc, Intent data) {
		Log.d(TAG, "onActivityResult: reqc=" + reqc + " resc=" + resc + " data=" + data);
		if (resc != RESULT_OK || data == null) {
			return;
		}

		switch (reqc) {
		case ACTIVITY_IMPORT_ROMS:
		{
			File romsDir = getDir("roms", MODE_PRIVATE);
			ArrayList<Uri> uris = new ArrayList<Uri>();
			if (data.getData() != null)
				uris.add(data.getData());
			if (data.getClipData() != null) {
				for (int i = 0; i < data.getClipData().getItemCount(); i++)
					uris.add(data.getClipData().getItemAt(i).getUri());
			}
			int count = 0;
			for (Uri uri : uris) {
				try {
					InputStream in = getContentResolver().openInputStream(uri);
					if (in == null) continue;
					String fname = uri.getLastPathSegment();
					if (fname == null) fname = "rom_" + count;
					fname = Uri.decode(fname);
					int colonIdx = fname.lastIndexOf(':');
					if (colonIdx >= 0) fname = fname.substring(colonIdx + 1);
					fname = fname.replace('/', '_').replace('\\', '_');
					File outFile = new File(romsDir, fname);
					FileOutputStream out = new FileOutputStream(outFile);
					byte[] buf = new byte[65536];
					int n;
					while ((n = in.read(buf)) >= 0)
						out.write(buf, 0, n);
					in.close();
					out.close();
					count++;
				} catch (Exception e) {
					Log.d(TAG, "Import ROM error: " + e.getMessage());
				}
			}
			if (count > 0) {
				String rp = romsDir.getAbsolutePath();
				SharedPreferences.Editor e = _sp.edit();
				e.putString("rompath", rp);
				e.commit();
				Toast.makeText(this, String.format("Imported %d ROM file(s)", count), Toast.LENGTH_SHORT).show();
			}
			break;
		}
		}
	}

	@Override
	protected Dialog onCreateDialog(int id) {
		Dialog d;

		switch (id) {
		case DLG_ABOUT:
			WebView v = new WebView(this);
			v.loadData(String.format(getString(R.string.aboutmsg),
					   MainActivity._pkgversion, MainActivity._coreversion), "text/html", "utf-8");
			v.setVerticalScrollBarEnabled(true);
			d = new AlertDialog.Builder(this)
					.setTitle(R.string.about)
					.setIcon(R.drawable.icon)
					.setView(v)
					.setInverseBackgroundForced(true)
					.setPositiveButton(R.string.ok, null)
					.create();
			break;

		case DLG_RESET:
			d = new AlertDialog.Builder(this)
					.setTitle(R.string.warning)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.pref_warnresetactions)
					.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface d, int i) {
							for (String str: new String[] {"actiona", "actionb", "actionc"})
								((KeymapPreference) findPreference(str)).setDefaultKeymap();
						}
						})
					.setNegativeButton(R.string.cancel, null)
					.create();
			break;

		case DLG_OVRWR:
			d = new AlertDialog.Builder(this)
					.setTitle(R.string.warning)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(String.format(getString(R.string.savestateoverwrite), _svstfname))
					.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface d, int i) {
							saveState(true);
						}
						})
					.setNegativeButton(R.string.cancel, null)
					.create();
			break;

		default:
			d = null;
		}

		return d;
	}

	private native boolean NativeSaveState(String fname);
	private native boolean NativeLoadState(String fname);
	private native boolean NativeBootPD(byte data[], int len);
	private native boolean NativeOSLSound();
}
