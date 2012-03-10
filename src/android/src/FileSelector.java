/*
 * FileSelector.java - the file selector activity
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

import java.io.File;
import java.io.FileFilter;
import java.util.Comparator;
import java.util.Set;
import java.util.HashSet;

import android.app.ListActivity;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.os.Environment;
import android.view.View;
import android.widget.ListView;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.view.LayoutInflater;
import android.content.Context;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.ImageView;
import android.content.SharedPreferences;
import android.widget.AdapterView;
import android.content.DialogInterface;
import android.app.AlertDialog;
import android.util.Log;
import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.app.Dialog;
import android.widget.Toast;


public final class FileSelector extends ListActivity implements AdapterView.OnItemLongClickListener,
																View.OnClickListener
{
	public static final String ACTION_OPEN_FILE = "jubanka.intent.OPENFILE";
	public static final String ACTION_OPEN_PATH = "jubanka.intent.OPENPATH";

	private static final String TAG = "FileSelector";
	private static final String SAVED_PATH = "SavedPath";
	private static final String SAVED_POS = "SavedPos";

	private static final int DLG_MOUNT = 0;
	private static final int DLG_WARNING = 1;

	private IconArrayAdapter _ad = null;
	private File _curdir;
	private ListDirTask _task = null;
	private boolean _pathsel = false;
	private static String _mntfname = null;
	private static String _drive1fname = null;

	private final class IconArrayAdapter extends ArrayAdapter<String> {
		LayoutInflater _inf = null;

		public IconArrayAdapter(Context context, int textViewResourceId) {
			super(context, textViewResourceId);
			_inf = (LayoutInflater) getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
		}

		@Override
		public View getView(int pos, View view, ViewGroup par) {
			if (view == null)
				view = _inf.inflate(R.layout.file_selector_row, null);
			String itm = getItem(pos);
			((TextView) view.findViewById(R.id.fsel_text)).setText(itm);
			((ImageView) view.findViewById(R.id.fsel_image)).setImageResource(
				itm.endsWith("/") ? R.drawable.folder : 0 );

			return view;
		}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		ListView lv = getListView();
		_pathsel = getIntent().getAction().equals(ACTION_OPEN_PATH);

		if (_pathsel) {
			LayoutInflater inf = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			View v = inf.inflate(R.layout.file_selector_footer, null);
			setContentView(v);
			findViewById(R.id.fsel_ok).setOnClickListener(this);
			findViewById(R.id.fsel_cancel).setOnClickListener(this);
		} else
			lv.setOnItemLongClickListener(this);

		lv.setTextFilterEnabled(true);
		lv.setFastScrollEnabled(true);

		SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
		String oldpath = _pathsel ? (getIntent().getData() != null ? getIntent().getData().getPath() : null)
								  : prefs.getString(SAVED_PATH, null);
		listDirectory((oldpath != null) ? new File(oldpath) : Environment.getExternalStorageDirectory(),
					  _pathsel ? 0 : prefs.getInt(SAVED_POS, 0), getLastNonConfigurationInstance());
	}

	@Override
	protected void onPause() {
		if (!_pathsel) {
			SharedPreferences prefs = getPreferences(Context.MODE_PRIVATE);
			SharedPreferences.Editor edit = prefs.edit();
			edit.putString(SAVED_PATH, _curdir.getAbsolutePath());
			edit.putInt(SAVED_POS, getListView().getFirstVisiblePosition());
			edit.commit();
		}

		super.onPause();
	}

	@Override
	protected void onDestroy() {
		if (_task != null)	_task.cancel(true);		// you rotate, you lose
		_task = null;
		super.onDestroy();
	}

	@Override
	public Object onRetainNonConfigurationInstance() {
		String[] ret = null;
		if (_task == null && _ad != null && _ad.getCount() > 0) {
			int cnt = _ad.getCount();
			ret = new String[cnt];
			for (int i = 0; i < cnt; i++)
				ret[i] = _ad.getItem(i);
		}
		return (Object) ret;
	}

	@Override
	public void onClick(View v) {
		switch (v.getId()) {
		case R.id.fsel_ok:
			setResult(Activity.RESULT_OK, new Intent(MainActivity.ACTION_SET_ROMPATH,
						Uri.fromFile(_curdir)));
			finish();
			break;
		case R.id.fsel_cancel:
			setResult(Activity.RESULT_CANCELED);
			finish();
			break;
		}
	}

	@Override
	protected void onListItemClick(ListView l, View v, int pos, long id) {
		String fname = _ad.getItem(pos);
		if (fname.startsWith("../"))
			listDirectory(_curdir.getParentFile(), 0, null);
		else if (fname.endsWith("/"))
			listDirectory(new File(_curdir, fname), 0, null);
		else if (!_pathsel) {
			_drive1fname = null;
			setResult(Activity.RESULT_OK, new Intent(MainActivity.ACTION_INSERT_REBOOT,
					  Uri.fromFile(new File(_curdir, fname))));
			finish();
			return;
		}
	}

	@Override
	public boolean onItemLongClick(AdapterView<?> l, View v, final int pos, long id) {
		_mntfname = _ad.getItem(pos);
		if (_mntfname.endsWith("/"))
			return true;
		if (!NativeIsDisk(_curdir + "/" + _mntfname)) {
			showDialog(DLG_WARNING);
			return true;
		}
		showDialog(DLG_MOUNT);
		return true;
	}

	@Override
	protected Dialog onCreateDialog(int id) {
		Dialog d;

		switch (id) {
		case DLG_MOUNT:
			CharSequence[] items = new CharSequence[5];
			String[] drives = NativeGetDrvFnames();
			for (int i = 0; i < 4; i++)
				items[i] = new StringBuilder(drives[i]);
			items[4] = getString(R.string.unmountall);
			d = new AlertDialog.Builder(this)
						.setTitle(R.string.mountdisk)
						.setCancelable(true)
						.setItems(items, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								if (i < 4) {
									NativeRunAtariProgram(_curdir + "/" + _mntfname, i + 1, 0);
									if (i == 0)
										_drive1fname = _curdir + "/" + _mntfname;
									Toast.makeText(FileSelector.this,
											String.format(getString(R.string.mountinsertdisk), _mntfname, i + 1),
											Toast.LENGTH_SHORT)
										 .show();
								} else {
									NativeUnmountAll();
									_drive1fname = null;
								}
								_mntfname = null;
								dismissDialog(DLG_MOUNT);
							}
							})
						.create();
			break;

		case DLG_WARNING:
			d = new AlertDialog.Builder(FileSelector.this)
						.setTitle(R.string.warning)
						.setIcon(android.R.drawable.ic_dialog_alert)
						.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface d, int i) {
								dismissDialog(DLG_WARNING);
								showDialog(DLG_MOUNT);
							}
							})
						.setNegativeButton(R.string.no, null)
						.setMessage(String.format(getString(R.string.mountnodisk), _mntfname))
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
		case DLG_MOUNT:
			String[] drives = NativeGetDrvFnames();
			StringBuilder itemtxt;
			for (int i = 0; i < 4; i++) {
				itemtxt = (StringBuilder) ((AlertDialog) d).getListView().getAdapter().getItem(i);
				itemtxt.delete(0, itemtxt.length());
				itemtxt.append(drives[i]);
			}
			((AlertDialog) d).getListView().invalidateViews();
			break;
		}
	}

	@Override
	public void finish() {
		if (!_pathsel) {
			if (_drive1fname != null) {
				setResult(Activity.RESULT_OK, new Intent(MainActivity.ACTION_INSERT_ONLY,
												Uri.fromFile(new File(_drive1fname))));
			}
		}
		super.finish();
	}

	private void listDirectory(File dir, int pos, Object retain) {
		if (_ad != null)	_ad.clear();
		setTitle(getString(_pathsel ? R.string.fsel_opendir : R.string.fsel_openfile)
							+ " " + dir.getAbsolutePath());
		_curdir = dir;

		if (retain == null)
			_task = (ListDirTask) new ListDirTask(pos).execute(dir);
		else {
			_ad = new IconArrayAdapter(this, R.layout.file_selector_row);
			for (String str: (String[]) retain)
				_ad.add(str);

			setListAdapter(_ad);
			if (pos > _ad.getCount())	pos = _ad.getCount();
			setSelection(pos);
		}
	}

	private final class ListDirTask extends AsyncTask<File, Void, IconArrayAdapter>
	{
		ProgressDialog _pdlg;
		int _position;

		public ListDirTask(int pos) {
			_position = pos;
		}

		@Override
		protected void onPreExecute() {
			_pdlg = ProgressDialog.show(FileSelector.this, "", getString(R.string.loadingdir), true);
		}

		@Override
		protected void onPostExecute(IconArrayAdapter res) {
			_ad = res;
			setListAdapter(_ad);
			if (_position > res.getCount())	_position = res.getCount();
			setSelection(_position);

			try {
				_pdlg.dismiss();
			} catch (Exception e) {
				Log.d(TAG, "Leaked pdialog handle");
			}
			_task = null;
		}

		@Override
		protected IconArrayAdapter doInBackground(File... files) {
			IconArrayAdapter flst = new IconArrayAdapter(FileSelector.this, R.layout.file_selector_row);
			File dir = files[0];
			if (!dir.toString().equals("/"))
				flst.add("../");

			File[] lst = dir.listFiles(new FileFilter() {
				@Override
				public boolean accept(File file) {
					String f = file.getName().toLowerCase();
					int l = f.length();
					return file.isDirectory() || (l > 3 && EXTENSIONS.contains(f.substring(l - 3)));
				}
			});
			if (lst == null)	return flst;

			for (File f: lst) {
				if (f.isDirectory())
					flst.add(f.getName() + "/");
				else
					flst.add(f.getName());
			}

			flst.sort(new Comparator<String>() {
				@Override
				public int compare(String s1, String s2) {
					boolean s1b = s1.endsWith("/");
					boolean s2b = s2.endsWith("/");
					if (s1b && !s2b)	return -1;
					if (s2b && !s1b)	return 1;
					return s1.compareToIgnoreCase(s2);
				}
			});
			return flst;
		}
	}


	private native boolean NativeIsDisk(String img);
	private native void NativeRunAtariProgram(String img, int drive, int reboot);
	private native String[] NativeGetDrvFnames();
	private native void NativeUnmountAll();

	private static final Set<String> EXTENSIONS = new HashSet<String>(13);
	static {
		EXTENSIONS.add("atr");	EXTENSIONS.add("atz");	EXTENSIONS.add("xfd");
		EXTENSIONS.add("dcm");	EXTENSIONS.add("xfz");	EXTENSIONS.add("xex");
		EXTENSIONS.add("cas");	EXTENSIONS.add("rom");	EXTENSIONS.add("bin");
		EXTENSIONS.add("car");	EXTENSIONS.add("a8s");	EXTENSIONS.add("com");
		EXTENSIONS.add("exe");
	}
}
