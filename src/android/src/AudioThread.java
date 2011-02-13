/*
 * AudioThread.java - pushes audio to android
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

import android.media.AudioTrack;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.util.Log;


public final class AudioThread extends Thread
{
	private static final String TAG = "A800AudioThread";
	private static final int FRAMERATE = 50;

	private AudioTrack _at;
	private int _bufsize;
	private byte[] _buffer;
	private boolean _quit;
	private int _chunk;
	private boolean _initok;
	private boolean _pause;

	public AudioThread(int rate, int bytes, int bufsizems) {
		int format = bytes == 1 ? AudioFormat.ENCODING_PCM_8BIT : AudioFormat.ENCODING_PCM_16BIT;
		int minbuf = AudioTrack.getMinBufferSize(rate, AudioFormat.CHANNEL_OUT_MONO, format);
		int hardmin = (int) ( ((float) rate * bytes) / ((float) (1000.0f / bufsizems)) );
		_chunk = rate * bytes / FRAMERATE;
		_bufsize = (hardmin > minbuf) ? hardmin : minbuf;
		_bufsize = ((_bufsize + _chunk - 1) / _chunk * _chunk + 3) / 4 * 4;
		_at = new AudioTrack(AudioManager.STREAM_MUSIC, rate, AudioFormat.CHANNEL_CONFIGURATION_MONO,
							 format, _bufsize, AudioTrack.MODE_STREAM);
		_buffer = new byte[_bufsize];
		Log.d( TAG, String.format(
				  "Mixing audio at %dHz, %dbit, buffer size %d (%d ms) [requested=%d(%dms), minbuf=%d(%dms)]",
				  rate, 8 * bytes, _bufsize, (int) (((float) _bufsize)/((float) rate * bytes) * 1000),
				  hardmin, (int) (((float) hardmin)/((float) rate * bytes) * 1000),
				  minbuf, (int) (((float) minbuf)/((float) rate * bytes) * 1000)
				  ) );
		_quit = false;
		_pause = false;
		NativeSoundInit(_bufsize);
		this.setDaemon(true);
		if (_at.getState() != AudioTrack.STATE_INITIALIZED) {
			Log.e(TAG, "Cannot initialize audio");
			_initok = false;
		} else
			_initok = true;
	}

	public void pause(boolean p) {
		synchronized(this) {
			_pause = p;
		}
		if (p) {
			_at.pause();
			Log.d(TAG, "Audio paused");
		} else {
			_at.play();
			Log.d(TAG, "Audio resumed");
		}
	}

	public void run() {
		int offset = 0;
		int len, w, chunk;
		boolean pause;

		if (!_initok)	return;
		_at.play();
		chunk = _chunk / 2;

		try {
		while (!_quit) {
			synchronized(this) {
				pause = _pause;
			}
			if (pause) {
				sleep(10);
				continue;
			}

			len = _bufsize - offset;
			if (len > chunk)
				len = chunk;
			else if (len <= 0) {
				len = chunk;
				offset = 0;
			}
			NativeSoundUpdate(offset, len);
			w = 0;
			while (w < len)
				w += _at.write(_buffer, offset + w, len - w);
			offset += w;
			//Log.d(TAG, String.format("Pumped %d bytes", w));
		}
		} catch (InterruptedException ex) {
		}

		NativeSoundExit();
		_at.stop();
	}

	public void interrupt() {
		Log.d(TAG, "Audio thread exit via interrupt");
		_quit = true;
	}

	// Native function declarations
	private native void NativeSoundInit(int size);
	private native void NativeSoundUpdate(int offset, int length);
	private native void NativeSoundExit();
}
