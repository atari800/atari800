%define name	atari800
%define ver	1.2.3
%define rel	1
%define copy	GPL
%define ich friedel <friedel@nomaden.org>
%define group	Console/Emulators
%define realname atari800-%{ver}
%define src atari800-%{ver}.tar.gz
%define targets ncurses svgalib x11 sdl
## If you change the targets, you'll have to change the files list at the
## bottom of this file as well
%define maintarget sdl
Summary:	An emulator for 8-bit Atari personal computers.
Name:		%{name}
Version:	%{ver}
Release:	%{rel}
Copyright:	%{copy}
Packager: %{ich}
URL: http://atari800.sourceforge.net/
Group:	%{group}
Source: http://prdownloads.sourceforge.net/atari800/%{src}
BuildRoot: /var/tmp/%{name}-root
#Patch: %{name}-%{ver}.patch
%description
Atari800 is an emulator for the 800, 800XL, 130XE and 5200 models of
the Atari personal computer. It can be used under SVGAlib or X11. It
features excellent compatibility with full sound support, artifacting
emulation, and more.

Authors:
David Firth
and many others (see CREDITS for a full list)

%prep
rm -rf %{realname}

%setup -n %{realname}/src
./autogen.sh || echo "Autogen put out its usual complaint, ignored!"
#%patch -p1

%build
for target in %{targets}
do
	./configure --target=$target --disable-VERY_SLOW \
	--disable-NO_CYCLE_EXACT \
	--enable-CRASH_MENU --enable-MONITOR_BREAK \
	--enable-MONITOR_HINTS --enable-MONITOR_ASSEMBLER \
	--enable-COMPILED_PALETTE --enable-SNAILMETER \
	--enable-USE_CURSORBLOCK --enable-LINUX_JOYSTICK \
	--enable-SOUND  --disable-NO_VOL_ONLY --disable-NO_CONSOL_SOUND \
	--disable-SERIO_SOUND --disable-NOSNDINTER --enable-CLIP \
	--disable-STEREO --enable-BUFFERED_LOG --enable-SET_LED \
	--disable-NO_LED_ON_SCREEN --disable-SVGA_SPEEDUP --disable-JOYMOUSE
	make
	mv atari800 atari800-$target
	make clean
done
touch atari800

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/share/man/man1
make install PREFIX=$RPM_BUILD_ROOT/usr
for target in %{targets}
do
	install atari800-$target $RPM_BUILD_ROOT/usr/bin
done
(
	cd $RPM_BUILD_ROOT/usr/bin
	ln -sf atari800-%{maintarget} atari800
)

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%doc ../DOC ../README.1ST ../COPYING
%attr(4755,root,root) /usr/bin/atari800-svgalib
/usr/bin/atari800
/usr/bin/atari800-x11
/usr/bin/atari800-ncurses
/usr/bin/atari800-sdl
/usr/share/man/man1/atari800.1.bz2

%changelog
* Tue Jul 9 2002 Petr Stehlik <pstehlik@sophics.cz>
Main target is SDL. Manual is installed to /usr/share/man (FHS).
* Mon Jul 8 2002 Petr Stehlik <pstehlik@sophics.cz>
Atari800 now installs to /usr/bin
* Fri Dec 21 2001 Friedrich Delgado Friedrichs <friedel@nomaden.org>
Pathname correction (tarfile now unpacks to Atari800-<version>, instead
of Atari800).
* Sun Dec 2 2001 Friedrich Delgado Friedrichs <friedel@nomaden.org>
First (working) version
