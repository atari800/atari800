/*
 * MainActivity.java - activity entry point for atari800
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

package cz.pstehlik.colleen;

import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.io.File;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.util.Map;
import java.util.EnumMap;

import android.app.Activity;
import android.os.Bundle;
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
import android.os.Build;
import android.view.WindowManager;
import android.view.WindowInsets;
import android.view.View;
import android.view.Gravity;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;


public final class MainActivity extends Activity
{
	private static final String TAG = "MainActivity";
	private static final int ACTIVITY_FSEL = 1;
	private static final int ACTIVITY_PREFS = 2;
	private static final int DLG_WELCOME = 0;
	private static final int DLG_PATHSETUP = 1;
	private static final int DLG_BRWSCONFRM = 2;
	private static final int DLG_SELCARTTYPE = 3;

	public static String _pkgversion;
	public static String _coreversion;
	private static boolean _initialized = false;
	private static String _curDiskFname = null;
	private A800view _view = null;
	private AudioThread _audio = null;
	private InputMethodManager _imng;
	private Settings _settings = null;
	private String _cartTypes[][] = null;
	private static File _romsDir = null;
	static File _savesDir = null;

	static {
		System.loadLibrary("atari800");
		_coreversion = NativeInit();
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		_view = new A800view(this);

		FrameLayout root = new FrameLayout(this);

		root.addView(_view, new FrameLayout.LayoutParams(
			FrameLayout.LayoutParams.MATCH_PARENT,
			FrameLayout.LayoutParams.MATCH_PARENT));

		LinearLayout topBar = new LinearLayout(this);
		topBar.setOrientation(LinearLayout.HORIZONTAL);
		topBar.setBackgroundColor(0x00000000);
		topBar.setGravity(Gravity.LEFT | Gravity.CENTER_VERTICAL);

		ImageButton btnOpen = new ImageButton(this);
		btnOpen.setImageResource(R.drawable.ic_folder_open);
		btnOpen.setBackgroundColor(0x00000000);
		btnOpen.setPadding(24, 4, 24, 4);
		btnOpen.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
					.addCategory(Intent.CATEGORY_OPENABLE).setType("*/*")
					.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION), ACTIVITY_FSEL);
			}
		});
		topBar.addView(btnOpen);

		ImageButton btnKbd = new ImageButton(this);
		btnKbd.setImageResource(R.drawable.ic_keyboard);
		btnKbd.setBackgroundColor(0x00000000);
		btnKbd.setPadding(24, 4, 24, 4);
		btnKbd.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				_imng.showSoftInput(_view, InputMethodManager.SHOW_FORCED);
			}
		});
		topBar.addView(btnKbd);

		ImageButton btnPrefs = new ImageButton(this);
		btnPrefs.setImageResource(R.drawable.ic_settings);
		btnPrefs.setBackgroundColor(0x00000000);
		btnPrefs.setPadding(24, 4, 24, 4);
		btnPrefs.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				startActivityForResult(new Intent(MainActivity.this, Preferences.class), ACTIVITY_PREFS);
			}
		});
		topBar.addView(btnPrefs);

		root.addView(topBar, new FrameLayout.LayoutParams(
			FrameLayout.LayoutParams.WRAP_CONTENT,
			FrameLayout.LayoutParams.WRAP_CONTENT,
			Gravity.LEFT | Gravity.TOP));

		setContentView(root);
		_view.setKeepScreenOn(true);

		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
		_view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.STATUS_BAR_HIDDEN);

		root.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
			@Override
			public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
				int topInset = insets.getSystemWindowInsetTop();
				topBar.setPadding(0, topInset, 0, 0);
				NativeSetTopInset(topInset);
				return insets;
			}
		});

		_imng = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

		PreferenceManager.setDefaultValues(this, R.xml.preferences, true);
		Object obj = getLastNonConfigurationInstance();
		_settings = new Settings(PreferenceManager.getDefaultSharedPreferences(this), this, obj);
		_pkgversion = getPInfo().versionName;
		_romsDir = getDir("roms", MODE_PRIVATE);
		_savesDir = getDir("saves", MODE_PRIVATE);

		if (!_initialized) {
			_settings.putBoolean("koalapad", false);
			_settings.fetchApplySettings();
			_initialized = true;
			bootupMsgs();
		} else {
			_settings.fetch();
			if (obj != null)	_settings.testApply();
			_settings.commit();
			soundInit(false);
		}
	}

	@Override
	public Object onRetainNonConfigurationInstance() {
		return (Object) _settings.serialize();
	}

	private void bootupMsgs() {
		String instver = _settings.get(false, "version");
		if (instver == null || instver.equals("false")) {
			showDialog(DLG_WELCOME);
			return;
		}
		Toast.makeText(this, R.string.noactionbarhelptoast, Toast.LENGTH_SHORT).show();
	}

	public void message(int msg) {
		switch (msg) {
		case A800Renderer.REQ_BROWSER:
			runOnUiThread(new Runnable() {
				@Override
				public void run() {
					showDialog(DLG_BRWSCONFRM);
				}
			});
			break;
		};
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
								String rp = _romsDir.getAbsolutePath();
								_settings.putString("rompath", rp);
								_settings.simulateChanged("rompath");
								pauseEmulation(false);
							}
							})
						.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								pauseEmulation(false);
								dismissDialog(DLG_PATHSETUP);
							}
							})
						.create();
			break;

		case DLG_BRWSCONFRM:
			String url = NativeGetURL();
			if (url.length() == 0) {
				d = null;
				break;
			}
			if (! validateURL(url)) {
				Log.d(TAG, "Browser request denied for improper url " + url);
				d = null;
				NativeClearDevB();
				Toast.makeText(this, R.string.browserreqdenied, Toast.LENGTH_SHORT).show();
				break;
			}
			pauseEmulation(true);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.warning)
						.setIcon(android.R.drawable.ic_dialog_alert)
						.setCancelable(false)
						.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								String u = NativeGetURL().trim();
								Log.d(TAG, "Spawning browser for " + u);
								pauseEmulation(false);
								try {
									startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(u)));
								} catch (Exception e1) {
									Log.d(TAG, "Exception, trying with lower case");
									try {
										startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(u.toLowerCase())));
									} catch (Exception e2) {
										Log.d(TAG, "Exception, failed, giving up");
									}
								}
								NativeClearDevB();
							}
							})
						.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								pauseEmulation(false);
								NativeClearDevB();
							}
							})
						.setMessage("")
						.create();
			break;

		case DLG_SELCARTTYPE:
			if (_cartTypes == null || _cartTypes.length == 0) {
				Log.d(TAG, "0 cart types passed");
				d = null;
				break;
			}
			pauseEmulation(true);
			String itm[] = new String[_cartTypes.length];
			for (int i = 0; i < _cartTypes.length; itm[i] = _cartTypes[i][1], i++);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.selectcarttype)
						.setCancelable(false)
						.setItems(itm, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								NativeBootCartType(Integer.parseInt(_cartTypes[i][0]));
								pauseEmulation(false);
								removeDialog(DLG_SELCARTTYPE);
							}
							})
						.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								pauseEmulation(false);
								removeDialog(DLG_SELCARTTYPE);
							}
							})
						.create();
			break;

		default:
			d = null;
		}

		return d;
	}

	@Override
	protected void onPrepareDialog(int id, Dialog d) {
		switch (id) {
		case DLG_BRWSCONFRM:
			((AlertDialog) d).setMessage(String.format(getString(R.string.confirmurl), NativeGetURL().trim()));
			break;
		}
	}

	private boolean validateURL(String u) {
		if (u.trim().toLowerCase().startsWith("http://"))
			return true;
		return false;
	}

	private PackageInfo getPInfo() {
		PackageInfo p;
		try {
			p = getPackageManager().getPackageInfo("cz.pstehlik.colleen", 0);
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
									 Integer.parseInt(_settings.get(n, "mixbufsize")) * 10,
									 Boolean.parseBoolean(_settings.get(n, "ntsc")));
			_audio.start();
		} else {
			if (_audio != null)	_audio.interrupt();
			_audio = null;
		}
	}

	public void pauseEmulation(boolean pause) {
		if (_audio != null)	_audio.pause(pause);
		if (_view != null)	_view.pause(pause);
	}

	@Override
	public void onPause() {
		_imng.hideSoftInputFromWindow(_view.getWindowToken(), 0);
		pauseEmulation(true);
		super.onPause();
	}

	@Override
	public void onResume() {
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
		getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
		_view.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN |
				View.STATUS_BAR_HIDDEN);
		pauseEmulation(false);
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

	@Override
	protected void onActivityResult(int reqc, int resc, Intent data) {

		switch (reqc) {
		case ACTIVITY_FSEL:
			/* Handle SAF ACTION_OPEN_DOCUMENT result (content URI) */
			if (resc == RESULT_OK && data != null
				&& Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT && data.getData() != null
				&& data.getData().getScheme() != null && data.getData().getScheme().equals("content")) {
				String copyPath = copyContentUriToCache(data.getData());
				if (copyPath != null) {
					_curDiskFname = copyPath;
					int r = NativeRunAtariProgram(_curDiskFname, 1, 1);
					if (r == -2)
						showDialog(DLG_SELCARTTYPE);
					else
						Toast.makeText(this, String.format(getString(r < 0 ? R.string.errorboot : R.string.diskboot),
											_curDiskFname.substring(_curDiskFname.lastIndexOf("/") + 1)),
									   Toast.LENGTH_SHORT)
							 .show();
				}
				break;
			}
			pauseEmulation(false);
			break;

		case ACTIVITY_PREFS:
			if (resc == RESULT_FIRST_USER) {
				finish();
				break;
			}
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
						int r = NativeRunAtariProgram(_curDiskFname, 1, 0);
						Toast.makeText(this,
									   String.format(getString(
											   r >= 0 ? R.string.mountnextdisk : R.string.mountnextdiskerror),
										   f.getName()),
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

	private native int NativeRunAtariProgram(String img, int drive, int reboot);
	private native void NativeBootCartType(int kb);
	private native void NativeExit();
	private static native String NativeInit();

	// ----------------- Preferences -------------------

	private final static class Settings
	{
		enum PreferenceName {
			aspect, bilinear, artifact, frameskip, collisions, machine, basic, speed,
			disk, sector, softjoy, up, down, left, right, fire, joyvisible, joysize,
			joyopacity, joyrighth, joydeadband, joymidx, sound, mixrate, sound16bit,
			hqpokey, mixbufsize, version, rompath, anchor, anchorstr, joygrace,
			crophoriz, cropvert, portpad, covlhold, derotkeys, actiona, actionb, actionc, ntsc,
			paddle, koalapad, browser, forceAT, a800fns
		};
		private SharedPreferences _sharedprefs;
		private Map<PreferenceName, String> _values, _newvalues;
		private Context _context;

		@SuppressWarnings("unchecked")
		public Settings(SharedPreferences s, Context c, Object retain) {
			_sharedprefs = s;
			_context = c;
			if (retain == null)
				_values = new EnumMap<PreferenceName, String>(PreferenceName.class);
			else
				_values = (EnumMap<PreferenceName, String>) retain;
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
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.collisions)),
						   Integer.parseInt(_newvalues.get(PreferenceName.crophoriz)),
						   Integer.parseInt(_newvalues.get(PreferenceName.cropvert)),
						   Integer.parseInt(_newvalues.get(PreferenceName.portpad)),
						   Integer.parseInt(_newvalues.get(PreferenceName.covlhold)) );

			if ( changed(PreferenceName.machine) || changed(PreferenceName.ntsc) ) {
				if ( !NativePrefMachine(Integer.parseInt(_newvalues.get(PreferenceName.machine)),
										Boolean.parseBoolean(_newvalues.get(PreferenceName.ntsc))) ) {
					Log.d(TAG, "OS rom not found");
					if ( _values.get(PreferenceName.machine) != null &&
						!_values.get(PreferenceName.machine).equals("false") ) {
						Toast.makeText(_context, R.string.noromfoundrevert, Toast.LENGTH_LONG).show();
						revertString(PreferenceName.machine);
						NativePrefMachine(Integer.parseInt(_newvalues.get(PreferenceName.machine)),
										  Boolean.parseBoolean(_newvalues.get(PreferenceName.ntsc)));
					} else {
						Toast.makeText(_context, R.string.noromfound, Toast.LENGTH_LONG).show();
					}
				}
			}

			NativePrefEmulation( Boolean.parseBoolean(_newvalues.get(PreferenceName.basic)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.speed)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.disk)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.sector)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.browser)) );

			NativePrefSoftjoy( Boolean.parseBoolean(_newvalues.get(PreferenceName.softjoy)),
							   Integer.parseInt(_newvalues.get(PreferenceName.up)),
							   Integer.parseInt(_newvalues.get(PreferenceName.down)),
							   Integer.parseInt(_newvalues.get(PreferenceName.left)),
							   Integer.parseInt(_newvalues.get(PreferenceName.right)),
							   Integer.parseInt(_newvalues.get(PreferenceName.fire)),
							   Integer.parseInt(_newvalues.get(PreferenceName.derotkeys)),
				   			   new String[] { _newvalues.get(PreferenceName.actiona),
											  _newvalues.get(PreferenceName.actionb),
											  _newvalues.get(PreferenceName.actionc) } );

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
			NativePrefJoy( Boolean.parseBoolean(_newvalues.get(PreferenceName.joyvisible)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joysize)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joyopacity)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.joyrighth)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joydeadband)),
						   Integer.parseInt(_newvalues.get(PreferenceName.joymidx)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.anchor)),
						   x, y,
						   Integer.parseInt(_newvalues.get(PreferenceName.joygrace)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.paddle)),
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.koalapad)) );

			if ( changed(PreferenceName.mixrate) || changed(PreferenceName.sound16bit) ||
				 changed(PreferenceName.hqpokey) || changed(PreferenceName.mixbufsize) ||
				 changed(PreferenceName.forceAT) )
				NativePrefSound( Integer.parseInt(_newvalues.get(PreferenceName.mixrate)),
								 Integer.parseInt(_newvalues.get(PreferenceName.mixbufsize)) * 10,
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.sound16bit)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.hqpokey)),
								 Boolean.parseBoolean(_newvalues.get(PreferenceName.forceAT)) );
			if ( changed(PreferenceName.sound) || changed(PreferenceName.mixrate) ||
				 changed(PreferenceName.sound16bit) || changed(PreferenceName.mixbufsize) ||
				 changed(PreferenceName.forceAT) )
				((MainActivity) _context).soundInit(true);

			NativePrefKbd( Boolean.parseBoolean(_newvalues.get(PreferenceName.a800fns)) );

			if (changed(PreferenceName.rompath)) {
				String rp = _newvalues.get(PreferenceName.rompath);
				if (rp == null || rp.equals("false"))
					rp = _romsDir.getAbsolutePath();
				if (!NativeSetROMPath(rp))
					Toast.makeText(_context, R.string.rompatherror, Toast.LENGTH_LONG).show();
			}
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

		public void putBoolean(String key, boolean val) {
			_values.put(PreferenceName.valueOf(key), Boolean.toString(val));
			SharedPreferences.Editor e = _sharedprefs.edit();
			e.putBoolean(key, val);
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

		public void testApply() {
			boolean changed = false;
			for (PreferenceName n: PreferenceName.values())
				changed |= changed(n);
			if (changed)
				apply();
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

		public Map<PreferenceName, String> serialize() {
			return _values;
		}
	}

	private String copyContentUriToCache(Uri uri) {
		try {
			InputStream in = getContentResolver().openInputStream(uri);
			if (in == null) {
				return null;
			}
			String fname = uri.getLastPathSegment();
			if (fname == null) fname = "temp";
			/* Decode URL encoding: primary%3ADownload%2Fgame.atr -> primary:Download/game.atr */
			fname = Uri.decode(fname);
			/* Strip volume prefix like "primary:" */
			int colonIdx = fname.lastIndexOf(':');
			if (colonIdx >= 0) fname = fname.substring(colonIdx + 1);
			/* Replace any remaining path separators to avoid nested dir creation */
			fname = fname.replace('/', '_').replace('\\', '_');
			File outFile = new File(getCacheDir(), fname);
			FileOutputStream out = new FileOutputStream(outFile);
			byte[] buf = new byte[65536];
			int n;
			while ((n = in.read(buf)) >= 0)
				out.write(buf, 0, n);
			in.close();
			out.close();
			return outFile.getAbsolutePath();
		} catch (Exception e) {
			return null;
		}
	}

	private static native void NativePrefGfx(int aspect, boolean bilinear, int artifact,
											 int frameskip, boolean collisions, int crophoriz, int cropvert, int portpad, int covlhold);
	private static native boolean NativePrefMachine(int machine, boolean ntsc);
	private static native void NativePrefEmulation(boolean basic, boolean speed, boolean disk,
												   boolean sector, boolean browser);
	private static native void NativePrefSoftjoy(boolean softjoy, int up, int down, int left, int right,
												 int fire, int derotkeys, String[] actions);
	private static native void NativePrefJoy(boolean visible, int size, int opacity, boolean righth,
											 int deadband, int midx, boolean anchor, int anchorx, int anchory,
											 int grace, boolean paddle, boolean koalapad);
	private static native void NativePrefSound(int mixrate, int mixbufsizems, boolean sound16bit, boolean hqpokey,
											   boolean disableOSL);
	private static native void NativePrefKbd(boolean a800fns);
	private static native boolean NativeSetROMPath(String path);
	private static native String NativeGetJoypos();
	private static native String NativeGetURL();
	private static native void NativeClearDevB();
	private static native void NativeSetTopInset(int topInset);
}
