/*
 * MainActivity.java - activity entry point for atari800
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

import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.io.File;
import java.util.Map;
import java.util.EnumMap;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MenuInflater;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;
import android.content.Intent;
import android.util.Log;
import android.widget.Toast;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.app.Dialog;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.widget.TextView;
import android.R.style;
import android.widget.ScrollView;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.text.Html;
import android.text.method.LinkMovementMethod;


public final class MainActivity extends Activity
{
	public static final String ACTION_INSERT_REBOOT = "name.nick.jubanka.atari800.FileSelector.INSERTREBOOT";
	public static final String ACTION_INSERT_ONLY   = "name.nick.jubanka.atari800.FileSelector.INSERTONLY";
	public static final String ACTION_SET_ROMPATH   = "name.nick.jubanka.atari800.FileSelector.SETROMPATH";

	private static final String TAG = "MainActivity";
	private static final int ACTIVITY_FSEL = 1;
	private static final int ACTIVITY_PREFS = 2;
	private static final int DLG_WELCOME = 0;
	private static final int DLG_PATHSETUP = 1;

	public static String _pkgversion;
	public static String _coreversion;
	private static boolean _initialized = false;
	private A800view _view = null;
	private AudioThread _audio = null;
	private InputMethodManager _imng;
	private String _curDiskFname = null;
	private Settings _settings = null;
	private boolean _bootupconfig = false;

	static {
        System.loadLibrary("atari800");
		_coreversion = NativeInit();
	}
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		_view = new A800view(this);
		setContentView(_view);
		_view.setKeepScreenOn(true);

		_imng = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

		PreferenceManager.setDefaultValues(this, R.xml.preferences, true);
		_settings = new Settings(PreferenceManager.getDefaultSharedPreferences(this), this);
		_pkgversion = getPInfo().versionName;
		if (!_initialized) {
			_settings.fetchApplySettings();
			_initialized = true;
			bootupMsgs();
		} else {
			_settings.fetch();
			_settings.commit();
			soundInit(false);
		}
    }

	private void bootupMsgs() {
		String instver = _settings.get(false, "version");
		if (instver == null || instver.equals("false")) {
			_bootupconfig = true;
			pauseEmulation(true);
			showDialog(DLG_WELCOME);
			return;
		}

		String rompath = _settings.get(false, "rompath");
		if (rompath == null || rompath.equals("false")) {
			pauseEmulation(true);
			_bootupconfig = true;
			showDialog(DLG_PATHSETUP);
		}
	}

	@Override
	protected Dialog onCreateDialog(int id) {
		Dialog d;
		TextView t;
		ScrollView s;

		switch (id) {
		case DLG_WELCOME:
			t = new TextView(this);
			t.setText(R.string.welcomenote);
			t.setTextAppearance(this, android.R.style.TextAppearance_Small_Inverse);
			t.setBackgroundResource(android.R.color.background_light);
			s = new ScrollView(this);
			s.addView(t);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.welcome)
						.setView(s)
						.setInverseBackgroundForced(true)
						.setCancelable(false)
						.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								dismissDialog(DLG_WELCOME);
								_settings.putInt("version", getPInfo().versionCode);
								showDialog(DLG_PATHSETUP);
							}
							})
						.create();
			break;

		case DLG_PATHSETUP:
			t = new TextView(this);
			t.setText(Html.fromHtml(getString(R.string.pathsetupmsg)));
			t.setTextAppearance(this, android.R.style.TextAppearance_Small_Inverse);
			t.setBackgroundResource(android.R.color.background_light);
			t.setMovementMethod(LinkMovementMethod.getInstance());
			s = new ScrollView(this);
			s.addView(t);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.pathsetup)
						.setView(s)
						.setInverseBackgroundForced(true)
						.setCancelable(false)
						.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								dismissDialog(DLG_PATHSETUP);
								startActivityForResult(new Intent(FileSelector.ACTION_OPEN_PATH,
										null, MainActivity.this, FileSelector.class), ACTIVITY_FSEL);
							}
							})
						.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								dismissDialog(DLG_PATHSETUP);
								finish();
							}
							})
						.create();
			break;

		default:
			d = null;
		}
		return d;
	}

	private PackageInfo getPInfo() {
		PackageInfo p;
		try {
			p = getPackageManager().getPackageInfo("name.nick.jubanka.atari800", 0);
		} catch (Exception e) {
			Log.d(TAG, "Package not found");
			p = null;
		}
		return p;
	}

	private void soundInit(boolean n) {
		if (Boolean.parseBoolean(_settings.get(n, "sound"))) {
			if (_audio != null)	_audio.interrupt();
			_audio = new AudioThread(Integer.parseInt(_settings.get(n, "mixrate")),
									 Boolean.parseBoolean(_settings.get(n, "sound16bit")) ?  2 : 1,
									 Integer.parseInt(_settings.get(n, "mixbufsize")) * 10);
			_audio.start();
		} else {
			if (_audio != null)	_audio.interrupt();
			_audio = null;
		}
	}

	public void pauseEmulation(boolean pause) {
		_audio.pause(pause);
		_view.pause(pause);
	}

	@Override
	public void onPause() {
		_imng.hideSoftInputFromWindow(_view.getWindowToken(), 0);
		pauseEmulation(true);
		super.onPause();
	}

	@Override
	public void onResume() {
		if (!_bootupconfig)	pauseEmulation(false);
		super.onResume();
	}

	@Override 
	public void onDestroy() {
		if(_audio != null) _audio.interrupt();
		super.onDestroy();
		if (isFinishing()) {
			Log.d(TAG, "Exiting with finishing flag up");
			NativeExit();
			android.os.Process.killProcess(android.os.Process.myPid());
		}
	}

	// Menu stuff
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inf = getMenuInflater();
		inf.inflate(R.menu.menu, menu);
		return true;
	}

	@Override
	public void onOptionsMenuClosed(Menu m) {
		pauseEmulation(false);
	}

	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		pauseEmulation(true);
		_imng.hideSoftInputFromWindow(_view.getWindowToken(), 0);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.menu_quit:
			finish();
			return true;
		case R.id.menu_softkbd:
			_imng.showSoftInput(_view, InputMethodManager.SHOW_FORCED);
			return true;
		case R.id.menu_open:
			startActivityForResult(new Intent(FileSelector.ACTION_OPEN_FILE, null, this, FileSelector.class),
								   ACTIVITY_FSEL);
			return true;
		case R.id.menu_nextdisk:
			insertNextDisk();
			return true;
		case R.id.menu_preferences:
			startActivityForResult(new Intent(this, Preferences.class), ACTIVITY_PREFS);
			return true;
		default:
			return super.onOptionsItemSelected(item);
		}
	}

	@Override
	protected void onActivityResult(int reqc, int resc, Intent data) {
		switch (reqc) {
		case ACTIVITY_FSEL:
			if (resc != RESULT_OK) {
				if (_bootupconfig)	finish();
				break;
			}
			if (data.getAction().equals(ACTION_SET_ROMPATH)) {
				String p = data.getData().getPath();
				_settings.putString("rompath", p);
				_settings.simulateChanged("rompath");
				_bootupconfig = false;
				pauseEmulation(false);
				break;
			}
			_curDiskFname = data.getData().getPath();
			NativeRunAtariProgram(_curDiskFname,
					(data.getAction().equals(ACTION_INSERT_REBOOT)) ? 1 : 0);
			Toast.makeText(this, String.format(getString(
					(data.getAction().equals(ACTION_INSERT_REBOOT)) ?
												R.string.diskboot : R.string.mountnextdisk),
								_curDiskFname.substring(_curDiskFname.lastIndexOf("/") + 1)),
						   Toast.LENGTH_SHORT)
				 .show();
			break;
		case ACTIVITY_PREFS:
			_settings.fetchApplySettings();
			break;
		}
	}

	private void insertNextDisk() {
		final String[] pats = { "[\\s(,_]+s\\d" };
		Pattern p;
		Matcher m;
		File f;

		if (_curDiskFname == null)
			return;
		String path  = _curDiskFname.substring(0, _curDiskFname.lastIndexOf("/") + 1);
		String fname = _curDiskFname.substring(_curDiskFname.lastIndexOf("/") + 1,
											   _curDiskFname.lastIndexOf("."));
		String ext   = _curDiskFname.substring(_curDiskFname.lastIndexOf("."));
		Log.d(TAG, "Enter **" + path + "**" + fname + "**" + ext + "**");

		for (String s: pats) {
			p = Pattern.compile(s);
			m = p.matcher(fname);
			if (m.find()) {
				Log.d(TAG, "Match **" + m.group() + "**");
				char side = (char)(fname.charAt(m.end() - 1) + 1);
				char end = side;
				do {
					f = new File(path + fname.substring(0, m.end() - 1) + side +
								 fname.substring(m.end()) + ext);
				Log.d(TAG, "Trying loop " + f.getName());
					if (f.exists()) {
						_curDiskFname = f.getPath();
						NativeRunAtariProgram(_curDiskFname, 0);
						Toast.makeText(this,
									   String.format(getString(R.string.mountnextdisk), f.getName()),
									   Toast.LENGTH_SHORT)
							 .show();
						return;
					}
					if (side != '9')
						side++;
					else
						side = '0';
				} while (side != end);
			}
		}
		Toast.makeText(this, R.string.mountnonextdisk, Toast.LENGTH_SHORT).show();
	}

	private native void NativeRunAtariProgram(String img, int reboot);
	private native void NativeExit();
	private static native String NativeInit();

	// ----------------- Preferences -------------------

	private final static class Settings
	{
		enum PreferenceName {
			aspect, bilinear, artifact, frameskip, collisions, machine, basic, speed,
			disk, sector, softjoy, up, down, left, right, fire, joyvisible, joysize,
			joyopacity, joyrighth, joydeadband, joymidx, sound, mixrate, sound16bit,
			hqpokey, mixbufsize, version, rompath, anchor, anchorstr
		};
		private SharedPreferences _sharedprefs;
		private Map<PreferenceName, String> _values, _newvalues;
		private Context _context;

		public Settings(SharedPreferences s, Context c) {
			_sharedprefs = s;
			_context = c;
			_values = new EnumMap<PreferenceName, String>(PreferenceName.class);
			_newvalues = new EnumMap<PreferenceName, String>(PreferenceName.class);
		}

		public void fetch() {
			String v = null;
			for (PreferenceName n: PreferenceName.values()) {
				// nice, efficient coerce to string follows
				try { v = Boolean.toString(_sharedprefs.getBoolean(n.toString(), false)); } catch(Exception e1) {
				try { v = Integer.toString(_sharedprefs.getInt(n.toString(), -1)); } catch(Exception e2) {
				try { v = _sharedprefs.getString(n.toString(), null); } catch(Exception e3) {
				throw new ClassCastException(); }}};
				_newvalues.put(n,  v);
			}
		}

		private void apply() {
			NativePrefGfx( Integer.parseInt(_newvalues.get(PreferenceName.aspect)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.bilinear)),
						   Integer.parseInt(_newvalues.get(PreferenceName.artifact)),
						   Integer.parseInt(_newvalues.get(PreferenceName.frameskip)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.collisions)) );

			if (changed(PreferenceName.machine)) {
				if (!NativePrefMachine(Integer.parseInt(_newvalues.get(PreferenceName.machine)))) {
					Log.d(TAG, "OS rom not found");
					Toast.makeText(_context, R.string.noromfound, Toast.LENGTH_LONG).show();
					revertString(PreferenceName.machine);
					NativePrefMachine(Integer.parseInt(_newvalues.get(PreferenceName.machine)));
				}
			}

			NativePrefEmulation( Boolean.parseBoolean(_newvalues.get(PreferenceName.basic)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.speed)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.disk)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.sector)) );

			NativePrefSoftjoy( Boolean.parseBoolean(_newvalues.get(PreferenceName.softjoy)),
							   Integer.parseInt(_newvalues.get(PreferenceName.up)),
							   Integer.parseInt(_newvalues.get(PreferenceName.down)),
							   Integer.parseInt(_newvalues.get(PreferenceName.left)),
							   Integer.parseInt(_newvalues.get(PreferenceName.right)),
							   Integer.parseInt(_newvalues.get(PreferenceName.fire)) );

			int x = 0, y = 0;
			if (Boolean.parseBoolean(_newvalues.get(PreferenceName.anchor))) {
				String astr = _newvalues.get(PreferenceName.anchorstr);
				if (astr == null || astr.equals("false")) {
					astr = NativeGetJoypos();
					putString("anchorstr", astr);
					_newvalues.put(PreferenceName.anchorstr, astr);
				}
				String[] tok = astr.split(" ");
				x = Integer.parseInt(tok[0]);
				y = Integer.parseInt(tok[1]);
			} else
				putString("anchorstr", "false");
			NativePrefOvl( Boolean.parseBoolean(_newvalues.get(PreferenceName.joyvisible)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joysize)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joyopacity)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.joyrighth)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joydeadband)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joymidx)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.anchor)), x, y );

			if ( changed(PreferenceName.mixrate) || changed(PreferenceName.sound16bit) ||
				 changed(PreferenceName.hqpokey) )
				NativePrefSound( Integer.parseInt(_newvalues.get(PreferenceName.mixrate)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.sound16bit)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.hqpokey)) );
			if ( changed(PreferenceName.sound) || changed(PreferenceName.mixrate) ||
				 changed(PreferenceName.sound16bit) || changed(PreferenceName.mixbufsize) )
				((MainActivity) _context).soundInit(true);

			if (changed(PreferenceName.rompath))
				if (!NativeSetROMPath(_newvalues.get(PreferenceName.rompath)))
					Toast.makeText(_context, R.string.rompatherror, Toast.LENGTH_LONG).show();
		}

		public void simulateChanged(String key) {
			for (PreferenceName n: PreferenceName.values())
				_newvalues.put(n, _values.get(n));
			_values.put(PreferenceName.valueOf(key), null);
			apply();
			commit();
		}

		public String get(boolean newval, String what) {
			return (newval ? _newvalues : _values).get(PreferenceName.valueOf(what));
		}

		public void putInt(String key, int val) {
			_values.put(PreferenceName.valueOf(key), Integer.toString(val));
			SharedPreferences.Editor e = _sharedprefs.edit();
			e.putInt(key, val);
			e.commit();
		}

		public void putString(String key, String val) {
			_values.put(PreferenceName.valueOf(key), val);
			SharedPreferences.Editor e = _sharedprefs.edit();
			e.putString(key, val);
			e.commit();
		}

		private void revertString(PreferenceName p) {
			String oldval = _values.get(p);
			_newvalues.put(p, oldval);
			SharedPreferences.Editor e = _sharedprefs.edit();
			e.putString(p.toString(), oldval);
			e.commit();
		}

		public void fetchApplySettings() {
			fetch();
			apply();
			commit();
		}

		private boolean changed(PreferenceName p) {
			String s1 = _values.get(p);
			String s2 = _newvalues.get(p);
			if (s1 == null || s2 == null)	return true;
			return !s1.equals(s2);
		}

		public void commit() {
			for (PreferenceName n: PreferenceName.values()) {
				_values.put(n, _newvalues.get(n));
				_newvalues.put(n, null);
			}
		}

		public void print() {
			for (PreferenceName n: PreferenceName.values())
				Log.d(TAG, n.toString() + "=" + _values.get(n));
		}
	}
	private static native void NativePrefGfx(int aspect, boolean bilinear, int artifact,
											 int frameskip, boolean collisions);
	private static native boolean NativePrefMachine(int machine);
	private static native void NativePrefEmulation(boolean basic, boolean speed, boolean disk,
												   boolean sector);
	private static native void NativePrefSoftjoy(boolean softjoy, int up, int down, int left,
												 int right, int fire);
	private static native void NativePrefOvl(boolean visible, int size, int opacity, boolean righth,
										int deadband, int midx, boolean anchor, int anchorx, int anchory);
	private static native void NativePrefSound(int mixrate, boolean sound16bit, boolean hqpokey);
	private static native boolean NativeSetROMPath(String path);
	private static native String NativeGetJoypos();
}
