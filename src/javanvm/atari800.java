/*
 * atari800.java - Java NestedVM port of atari800
 *
 * Copyright (C) 2007-2008 Perry McFarlane
 * Copyright (C) 1998-2008 Atari800 development team (see DOC/CREDITS)
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
import org.ibex.nestedvm.Runtime;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.util.*;
import javax.sound.sampled.*;
import java.applet.*;

class AtariCanvas extends Canvas implements KeyListener {
	byte pixels[];
	MemoryImageSource mis;
	IndexColorModel icm;
	Image image;
	int atari_width;
	int atari_height;
	int atari_visible_width;
	int atari_left_margin;
	int width;
	int height;
	int scalew;
	int scaleh;
	int size;
	boolean windowClosed = false;
	Vector keyqueue;
	Hashtable kbhits;
	byte[][] paletteTable;
	byte[] temp;

	public void paint(Graphics g) {
		update(g);
	}

	public void update(Graphics g) {
		g.drawImage(image,0,0,width*scalew,height*scaleh,null);
	}

	public void init(){
		width = atari_visible_width;
		temp = new byte[width];
		height = atari_height;
		size = width*height;
		pixels = new byte[size];
		for(int i=0;i<size;i++){
			pixels[i]=0;
		}
		keyqueue = new Vector();
		addKeyListener(this);
		kbhits = new Hashtable();
	}

	/* Init the palette*/
	/* colortable is a runtime memory pointer*/
	public void initPalette(Runtime rt, int colortable){
		paletteTable = new byte[3][256];
		int entry=0;
		for(int i=0; i<256; i++){
			try {
				entry = rt.memRead(colortable+i*4);
			} catch(Exception e) {
				System.err.println(e);
			}
			paletteTable[0][i]=(byte)((entry>>>16)&0xff);
			paletteTable[1][i]=(byte)((entry>>>8)&0xff);
			paletteTable[2][i]=(byte)(entry&0xff);
		}
		icm = new IndexColorModel(8,256,paletteTable[0],paletteTable[1],paletteTable[2]);
		mis = new MemoryImageSource(width,height,icm,pixels,0,width);
		mis.setAnimated(true);
		mis.setFullBufferUpdates(true);
		image = createImage(mis);
	}

	public void displayScreen(Runtime rt, int atari_screen){
		int ao = atari_screen + atari_left_margin;
		int po = 0;
		for(int h=0; h<240;h++){
			try {
				rt.copyin(ao,temp,width);
				System.arraycopy(temp,0,pixels,po,width);
			} catch(Exception e) {
				System.err.println(e);
			}
			ao += atari_width;
			po += width;
		}
		mis.newPixels();
		repaint();
	}

	/* called when the user closes the window */
	public void setWindowClosed() {
		windowClosed = true;
	}

	// KeyListener methods:

	public void keyPressed(KeyEvent event) {
		char chr = event.getKeyChar();
		int key = event.getKeyCode();
		int loc = event.getKeyLocation();
		keyqueue.addElement(event);
		Integer[] val = new Integer[2];
		val[0] = new Integer(key);
		val[1] = new Integer(loc);
		kbhits.put(Arrays.asList(val), new Boolean(true));
		//System.err.println("keyPressed: "+key+" location: "+loc);
	}

	public void keyReleased(KeyEvent event) {
		char chr = event.getKeyChar();
		int key = event.getKeyCode();
		int loc = event.getKeyLocation();
		keyqueue.addElement(event);
		Integer[] val = new Integer[2];
		val[0] = new Integer(key);
		val[1] = new Integer(loc);
		kbhits.remove(Arrays.asList(val));
	}

	public void keyTyped(KeyEvent event) {
	}

	/* get a keyboard key state */
	int getKbhits(int key, int loc){
		Integer[] val = new Integer[2];
		val[0] = new Integer(key);
		val[1] = new Integer(loc);
		if  (kbhits.get(Arrays.asList(val)) != null ){
			return 1;
		}
		else{ 
			return 0;
		}
	}

	/* event points to an array of 4 values*/
	int pollKeyEvent(Runtime rt, int return_event){
		if (keyqueue.isEmpty()){
			return 0;
		}
		KeyEvent event = (KeyEvent)keyqueue.firstElement();
		keyqueue.removeElement(event);
		int type = event.getID();
		int key = event.getKeyCode();
		char uni = event.getKeyChar();
		int loc = event.getKeyLocation();
		try{
			/* write the data to the array pointed to by event*/
			rt.memWrite(return_event+0*4,type);
			rt.memWrite(return_event+1*4,key);
			rt.memWrite(return_event+2*4,(int)uni);
			rt.memWrite(return_event+3*4,loc);
		} catch(Exception e) {
			System.err.println(e);
		}
		return 1;
	}

	/* 1 if the Window was closed */
	int getWindowClosed(){
		return windowClosed ? 1 : 0;
	}
}

public class atari800 extends Applet implements Runnable {
	AtariCanvas canvas;
	Frame frame;
	SourceDataLine line;
	byte[] soundBuffer;
	boolean isApplet;
	Thread thread;
	private volatile boolean threadSuspended;

	//Applet constructor:
	public atari800() {
		isApplet = true;
	}

	//Application constructor:
	public atari800(Frame f) {
		isApplet = false;
		frame = f;
	}

	private void initGraphics(int scalew, int scaleh, int atari_width, int atari_height, int atari_visible_width, int atari_left_margin){
		canvas = new AtariCanvas();
		canvas.atari_width = atari_width;
		canvas.atari_height = atari_height;
		canvas.atari_visible_width = atari_visible_width;
		canvas.atari_left_margin = atari_left_margin;
		canvas.init();
		canvas.setFocusTraversalKeysEnabled(false); //allow Tab key to work
		canvas.setFocusable(true);
		if (!isApplet) {
			frame.addWindowListener(new WindowAdapter() {
				public void windowsGainedFocus(WindowEvent e) {
					canvas.requestFocusInWindow();
				}
				public void windowClosing(WindowEvent e) {
					canvas.setWindowClosed();
				}
			});
		}
		canvas.requestFocusInWindow();
		canvas.setSize(new Dimension(canvas.width*scalew,canvas.height*scaleh));
		canvas.scalew = scalew;
		canvas.scaleh = scaleh;
		if (isApplet) {
			this.add(canvas);
		}
		else {
			frame.add(canvas);
			frame.setResizable(false);
			frame.pack();
			frame.setVisible(true);
		}
	}

	private void initSound(int sampleRate, int bitsPerSample, int channels, boolean isSigned, boolean bigEndian){
		AudioFormat format = new AudioFormat(sampleRate, bitsPerSample, channels, isSigned, bigEndian);
		DataLine.Info info = new DataLine.Info(SourceDataLine.class,format);

		try {
			line = (SourceDataLine) AudioSystem.getLine(info);
			line.open(format);
			line.start();
			soundBuffer = new byte[line.getBufferSize()];
		} catch(Exception e) {
			System.err.println(e);
		}
	}

	//Applet init
	public void init() {
		//System.out.println("init()");
		setLayout(null);
	}

	//Applet start
	public synchronized void start() {
		//System.out.println("start()");
		if (thread == null) {
			thread = new Thread(this);
			thread.start();
		}
		threadSuspended = false;
		thread.interrupt();
	}

	//Applet stop
	public synchronized void stop() {
		//System.out.println("stop()");
		threadSuspended = true;
	}

	//Applet destroy
	public void destroy() {
		//System.out.println("destroy()");
		if (line!=null) line.close();
		if (canvas!=null) this.remove(canvas);
		Thread dead = thread;
		thread = null;
		dead.interrupt();
	}

	//Applet run thread
	public void run() {
		//System.out.println("run()");
		String args = getParameter("args");
		this.main2(args.split("\\s"));
	}

	public void paint(Graphics g) {
		//System.out.println("paint");
		if (canvas!=null) canvas.paint(g);
	}

	public static void main(String[] args) {
		Frame f = new Frame();
		final atari800 app = new atari800(f);
		f.setTitle("atari800");
		app.main2(args);
	}

	// used by both Application and Applet
	public void main2(String[] args) {
		//Place holder for command line arguments
		String[] appArgs = new String[args.length +1];
		try {
			final Thread thisThread = Thread.currentThread();
			//Application name
			appArgs[0] = "atari800";
			//Fill in the rest of the command line arguments
			for(int i=0;i<args.length;i++) appArgs[i+1] = args[i];

			//Make a Runtime instantiation
			final Runtime rt;

			String className = "atari800_runtime";
			/*TO use the interpreter:*/
			/*
			if(className.endsWith(".mips")){
				rt = new org.ibex.nestedvm.Interpreter(className);
			} else {*/
				Class c = Class.forName(className);
				if(!Runtime.class.isAssignableFrom(c)) { 
					System.err.println(className + " isn't a MIPS compiled class");
					System.exit(1); 
				}
				rt = (Runtime) c.newInstance();
			//}
			rt.setCallJavaCB(new Runtime.CallJavaCB() {
				public int call(int a, int b, int c, int d) {
					switch(a) {
						case 1: 
							/*static void JAVANVM_DisplayScreen(void *as){
								_call_java(1, (int)as, 0, 0);
							}*/
							canvas.displayScreen(rt,b);
							return 0;
						case 2:
							/*static void JAVANVM_InitPalette(void *ct){
								_call_java(2, (int)ct, 0, 0);
							}*/
							canvas.initPalette(rt, b);
							return 0;
						case 3:
							/*static int JAVANVM_Kbhits(int key, int loc){
								return _call_java(3, key, loc, 0);
							}*/
							return canvas.getKbhits(b, c);
						case 4:
							/*static int JAVANVM_PollKeyEvent(void *event){
								return _call_java(4, (int)event, 0, 0);
							}*/
							return canvas.pollKeyEvent(rt, b);
						case 5:
							/*static int JAVANVM_GetWindowClosed(void){
								return _call_java(5, 0, 0, 0);
							}*/
							return canvas.getWindowClosed();
						case 6:
							/*static int JAVANVM_Sleep(int millis){
								return _call_java(6, millis, 0, 0);
							}*/
							try {
								Thread.sleep((long)b);
							} catch(Exception e) {}
							return 0;
						case 7:
							/*static int JAVANVM_InitGraphics(void *config){
								return _call_java(7, (int)config, 0, 0);
							}*/
							int scaleh = 2;
							int scalew = 2;
							int atari_width = 384;
							int atari_height = 240;
							int atari_visible_width = 336;
							int atari_left_margin = 24;
							try {
								scalew = rt.memRead(b+4*0);
								scaleh = rt.memRead(b+4*1);
								atari_width = rt.memRead(b+4*2);
								atari_height = rt.memRead(b+4*3);
								atari_visible_width = rt.memRead(b+4*4);
								atari_left_margin = rt.memRead(b+4*5);
							} catch(Exception e) {
								System.err.println(e);
							}
							initGraphics(scaleh,scalew,atari_width,atari_height,atari_visible_width,atari_left_margin);
							return 0;
						case 8:
							/*static int JAVANVM_InitSound(void *config){
								return _call_java(8, (int)config, 0, 0);
							}*/
							int sampleRate = 44100;
							int bitsPerSample = 16;
							int channels = 2;
							boolean isSigned = true;
							boolean bigEndian = true;
							try {
								sampleRate = rt.memRead(b+4*0);
								bitsPerSample = rt.memRead(b+4*1);
								channels = rt.memRead(b+4*2);
								isSigned = (rt.memRead(b+4*3) != 0);
								bigEndian = (rt.memRead(b+4*4) != 0);
							} catch(Exception e) {
								System.err.println(e);
							}

							initSound(sampleRate, bitsPerSample, channels, isSigned, bigEndian);
							return line.getBufferSize();
						case 9:
							/*static int JAVANVM_SoundAvailable(void){
								return _call_java(9, 0, 0, 0);
							}*/
							return line.available();
						case 10:
							/*static int JAVANVM_SoundWrite(void *buffer,int len){
								return _call_java(10, (int)buffer, len, 0);
							}*/
							try {
								rt.copyin(b,soundBuffer,c);
							} catch(Exception e) {
								System.err.println(e);
							}
							line.write(soundBuffer,0,c);
							return 0;
						case 11:
							/*static int JAVANVM_SoundPause(void){
								return _call_java(11, 0, 0, 0);
							}*/
							line.stop();
							return 0;
						case 12:
							/*static int JAVANVM_SoundContinue(void){
								return _call_java(12, 0, 0, 0);
							}*/
							line.start();
							return 0;
						case 13:
							/*static int JAVANVM_CheckThreadStatus(void){
								return _call_java(13, 0, 0, 0);
							}*/
							if (isApplet && threadSuspended) {
								if (line!=null) line.stop();
								try {
									synchronized(this) {
										while (threadSuspended && thread == thisThread) {
										   	wait();
										}
									}
								} catch (InterruptedException e) {}
								if (thread != thisThread) {
									return 1;
								}
								if (line!=null) line.start();
							}
							return 0;
						default:
							return 0;
					}
				 }
			});
			if (isApplet) {
			}
			//Run the emulator:
			if (!isApplet) {
				System.exit(rt.run(appArgs));
			} else {
				rt.run(appArgs);
			}
		} catch(Exception e) {
			System.err.println(e);
		}
	}
}

/*
vim:ts=4:sw=4:
*/
