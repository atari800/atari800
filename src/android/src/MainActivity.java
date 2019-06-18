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

package name.nick.jubanka.colleen;

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
import android.app.ActionBar;
import android.view.Window;
import android.view.WindowManager;
import android.os.Build;
import android.view.View;


public final class MainActivity extends Activity
{
	public static final String ACTION_INSERT_REBOOT = "name.nick.jubanka.colleen.FileSelector.INSERTREBOOT";
	public static final String ACTION_INSERT_ONLY   = "name.nick.jubanka.colleen.FileSelector.INSERTONLY";
	public static final String ACTION_SET_ROMPATH   = "name.nick.jubanka.colleen.FileSelector.SETROMPATH";

	private static final String TAG = "MainActivity";
	private static final int ACTIVITY_FSEL = 1;
	private static final int ACTIVITY_PREFS = 2;
	private static final int DLG_WELCOME = 0;
	private static final int DLG_PATHSETUP = 1;
	private static final int DLG_CHANGES = 2;
	private static final int DLG_BRWSCONFRM = 3;
	private static final int DLG_SELCARTTYPE = 4;

	public static String _pkgversion;
	public static String _coreversion;
	public ActionBarNull _aBar = null;
	private static boolean _initialized = false;
	private static String _curDiskFname = null;
	private A800view _view = null;
	private AudioThread _audio = null;
	private InputMethodManager _imng;
	private Settings _settings = null;
	private boolean _bootupconfig = false;
	private String _cartTypes[][] = null;

	public static class ActionBarNull {
		public ActionBarNull(Activity a)					{};
		public void hide(Activity a)						{};
		public void hide(Activity a, boolean p)				{};
		public void hide(Activity a, boolean p, boolean f)	{};
		public void show(Activity a) 						{};
		public boolean isShowing(Activity a)				{ return false; }
		public boolean isReal()								{ return false; }
		public void init(Activity a) 						{};
	}

	public static final class ActionBarHelp extends ActionBarNull {
		public ActionBarHelp(Activity a) {
			super(a);
		}

		@Override
		public void hide(Activity a) {
			hide(a, true);
		}

		@Override
		public void hide(Activity a, boolean p) {
			hide(a, p, false);
		}

		@Override
		public void hide(Activity a, boolean p, boolean f) {
			ActionBar ab = a.getActionBar();
			View v = ((MainActivity) a)._view;
			if ( !f && !ab.isShowing() &&
				(v.getSystemUiVisibility() & View.STATUS_BAR_HIDDEN) == View.STATUS_BAR_HIDDEN )
			   	return;

			if (Integer.parseInt(Build.VERSION.SDK) < Build.VERSION_CODES.JELLY_BEAN) {
				a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
				a.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
			}
			if (v != null) {
				int flags = View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN 	|
							View.SYSTEM_UI_FLAG_LAYOUT_STABLE 		|
							View.SYSTEM_UI_FLAG_FULLSCREEN			|
							View.STATUS_BAR_HIDDEN;
				if (p == true)
					flags |= View.SYSTEM_UI_FLAG_LOW_PROFILE;
				v.setSystemUiVisibility(flags);
			}
			ab.hide();
			((MainActivity) a).pauseEmulation(false);
		}

		@Override
		public void show(Activity a) {
			ActionBar ab = a.getActionBar();
			if (ab.isShowing())		return;

			((MainActivity) a).pauseEmulation(true);
			if (Integer.parseInt(Build.VERSION.SDK) < Build.VERSION_CODES.JELLY_BEAN) {
				a.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
				a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
			}
			View v = ((MainActivity) a)._view;
			if (v != null) v.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
												   View.SYSTEM_UI_FLAG_LAYOUT_STABLE	 |
												   View.STATUS_BAR_VISIBLE);
			ab.show();
		}

		@Override
		public boolean isShowing(Activity a) {
			return a.getActionBar().isShowing();
		}

		@Override
		public boolean isReal() {
			return true;
		}

		@Override
		public void init(Activity a) {
			a.getActionBar().setBackgroundDrawable(a.getResources().getDrawable(R.drawable.actionbar_bg));
		}
	}


	static {
		System.loadLibrary("atari800");
		_coreversion = NativeInit();
	}
	
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (Integer.parseInt(Build.VERSION.SDK) >= Build.VERSION_CODES.HONEYCOMB)
			_aBar = new ActionBarHelp(this);
		else
			_aBar = new ActionBarNull(this);

		_view = new A800view(this);
		setContentView(_view);
		_view.setKeepScreenOn(true);

		_aBar.init(this);
		_aBar.hide(this);

		_imng = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);

		PreferenceManager.setDefaultValues(this, R.xml.preferences, true);
		Object obj = getLastNonConfigurationInstance();
		_settings = new Settings(PreferenceManager.getDefaultSharedPreferences(this), this, obj);
		_pkgversion = getPInfo().versionName;

		if (!_initialized) {
			_settings.putBoolean("plandef", false);
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
			_bootupconfig = true;
			pauseEmulation(true);
			showDialog(DLG_WELCOME);
			return;
		}

		if (Integer.parseInt(instver) != getPInfo().versionCode) {
			_bootupconfig = true;
			pauseEmulation(true);
			showDialog(DLG_CHANGES);
			return;
		}
		Toast.makeText(this,
					   _aBar.isReal() ? R.string.actionbarhelptoast : R.string.noactionbarhelptoast,
					   Toast.LENGTH_SHORT).show();
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
								_bootupconfig = false;
								pauseEmulation(false);
								dismissDialog(DLG_PATHSETUP);
							}
							})
						.create();
			break;

		case DLG_CHANGES:
			t = new TextView(this);
			int[] vs = getResources().getIntArray(R.array.changes_versions);
			int instver = getPInfo().versionCode;
			for (int i = 0; i < vs.length; i++)
				if (vs[i] == instver) {
					t.setText(Html.fromHtml(getResources().getStringArray(R.array.changes_strings)[i]));
					break;
				}
			t.setTextAppearance(this, android.R.style.TextAppearance_Small_Inverse);
			t.setBackgroundResource(android.R.color.background_light);
			t.setMovementMethod(LinkMovementMethod.getInstance());
			s = new ScrollView(this);
			s.addView(t);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.atariupdate)
						.setView(s)
						.setInverseBackgroundForced(true)
						.setCancelable(false)
						.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								_settings.putInt("version", getPInfo().versionCode);
								_bootupconfig = false;
								pauseEmulation(false);
								dismissDialog(DLG_CHANGES);
								Toast.makeText(MainActivity.this, _aBar.isReal() ?
														R.string.actionbarhelptoast :
														R.string.noactionbarhelptoast,
											   Toast.LENGTH_SHORT).show();
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
			p = getPackageManager().getPackageInfo("name.nick.jubanka.colleen", 0);
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
		if (pause) {
			if (_audio != null)	_audio.pause(pause);
			if (_view != null)	_view.pause(pause);
		} else if (!_bootupconfig) {
			if (_view != null)	_view.pause(pause);
			if (_audio != null)	_audio.pause(pause);
		}
	}

	@Override
	public void onPause() {
		_imng.hideSoftInputFromWindow(_view.getWindowToken(), 0);
		pauseEmulation(true);
		super.onPause();
	}

	@Override
	public void onResume() {
		_aBar.hide(this, true, true);
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

	// Menu stuff
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inf = getMenuInflater();
		inf.inflate(R.menu.menu, menu);
		return true;
	}

	@Override
	public void onOptionsMenuClosed(Menu m) {
		_aBar.hide(this);
		pauseEmulation(false);
	}

	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		if (!_aBar.isReal())
			pauseEmulation(true);	// menu is always shown on > honeycomb
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
			_aBar.hide(this, false);
			return true;
		case R.id.menu_open:
			startActivityForResult(new Intent(FileSelector.ACTION_OPEN_FILE, null, this, FileSelector.class),
								   ACTIVITY_FSEL);
			return true;
		case R.id.menu_nextdisk:
			insertNextDisk();
			_aBar.hide(this);
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
		_aBar.hide(this);

		switch (reqc) {
		case ACTIVITY_FSEL:
			if (data.getAction().equals(ACTION_SET_ROMPATH)) {
				if (resc == RESULT_OK) {
					String p = data.getData().getPath();
					_settings.putString("rompath", p);
					_settings.simulateChanged("rompath");
				}
				_bootupconfig = false;
				pauseEmulation(false);
				break;
			}

			if (resc != RESULT_OK) {
				break;
			}

			_curDiskFname = data.getData().getPath();
			if (data.getAction().equals(ACTION_INSERT_REBOOT)) {
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
			paddle, plandef, browser, forceAT
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
						   Boolean.parseBoolean(_newvalues.get(PreferenceName.plandef)) );

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
	private static native void NativePrefGfx(int aspect, boolean bilinear, int artifact,
											 int frameskip, boolean collisions, int crophoriz, int cropvert, int portpad, int covlhold);
	private static native boolean NativePrefMachine(int machine, boolean ntsc);
	private static native void NativePrefEmulation(boolean basic, boolean speed, boolean disk,
												   boolean sector, boolean browser);
	private static native void NativePrefSoftjoy(boolean softjoy, int up, int down, int left, int right,
												 int fire, int derotkeys, String[] actions);
	private static native void NativePrefJoy(boolean visible, int size, int opacity, boolean righth,
											 int deadband, int midx, boolean anchor, int anchorx, int anchory,
											 int grace, boolean paddle, boolean plandef);
	private static native void NativePrefSound(int mixrate, int mixbufsizems, boolean sound16bit, boolean hqpokey,
											   boolean disableOSL);
	private static native boolean NativeSetROMPath(String path);
	private static native String NativeGetJoypos();
	private static native String NativeGetURL();
	private static native void NativeClearDevB();
}
