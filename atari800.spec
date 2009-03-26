%define name	atari800
%define ver	2.1.0
%define rel	1
%define copy	GPL
%define ich Petr Stehlik <pstehlik@sophics.cz>
%define group	Console/Emulators
%define realname atari800-%{ver}
%define src atari800-%{ver}.tar.gz
%define targets ncurses x11 sdl
## If you change the targets, you'll have to change the files list at the
## bottom of this file as well
%define maintarget sdl
Summary:	An emulator of 8-bit Atari personal computers.
Name:		%{name}
Version:	%{ver}
Release:	%{rel}
License:	%{copy}
Packager: %{ich}
URL: http://atari800.sourceforge.net/
Group:	%{group}
Source: http://prdownloads.sourceforge.net/atari800/%{src}
BuildRoot: /var/tmp/%{name}-root
#Patch: %{name}-%{ver}.patch
%description
Atari800 is an emulator for the 800, 800XL, 130XE and 5200 models of
the Atari personal computer. It can be used on console, FrameBuffer or X11.
It features excellent compatibility, HIFI sound support, artifacting
emulation, precise cycle-exact ANTIC/GTIA emulation and more.

Authors:
David Firth
and Atari800 Development Team (see CREDITS for a full list)

%prep
rm -rf %{realname}

%setup -n %{realname}/src
./autogen.sh || echo "Autogen put out its usual complaint, ignored!"
#%patch -p1

%build
for target in %{targets}
do
	./configure --prefix=/usr --target=$target
	make
	mv atari800 atari800-$target
	make clean
done
touch atari800

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
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
/usr/bin/atari800
/usr/bin/atari800-x11
/usr/bin/atari800-ncurses
/usr/bin/atari800-sdl
/usr/share/man/man1/atari800.1.gz
/usr/share/doc/atari800/COPYING
/usr/share/doc/atari800/README.1ST
/usr/share/doc/atari800/README
/usr/share/doc/atari800/INSTALL
/usr/share/doc/atari800/USAGE
/usr/share/doc/atari800/NEWS

%changelog
* Fri Mar 27 2009 Petr Stehlik <pstehlik@sophics.cz>
New upstream release.
* Wen Jul 11 2007 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS file.
* Sat Apr 08 2006 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS file.
* Mon Jan 02 2006 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Long list of changes in the NEWS file.
* Fri Dec 30 2005 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Long list of changes in the NEWS file.
* Sat Apr 30 2005 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS.
* Thu Dec 30 2004 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS.
* Mon Dec 27 2004 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS.
* Sun Aug 08 2004 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Changes documented in the NEWS.
* Sat Dec 20 2003 Petr Stehlik <pstehlik@sophics.cz>
Version increased.
* Thu Sep 04 2003 Petr Stehlik <pstehlik@sophics.cz>
Version increased. Configure options removed.
SVGAlib target dropped.
configure uses --prefix and make install uses DESTDIR now.
More documentation installed.
* Mon Feb 10 2003 Petr Stehlik <pstehlik@sophics.cz>
Version increased. STEREO enabled by default. Description updated.
* Mon Dec 2 2002 Petr Stehlik <pstehlik@sophics.cz>
Version increased.
* Wed Aug 7 2002 Petr Stehlik <pstehlik@sophics.cz>
Version increased, packager changed, slight change to description.
* Tue Jul 9 2002 Petr Stehlik <pstehlik@sophics.cz>
Main target is SDL. Manual is installed to /usr/share/man (FHS).
* Mon Jul 8 2002 Petr Stehlik <pstehlik@sophics.cz>
Atari800 now installs to /usr/bin
* Fri Dec 21 2001 Friedrich Delgado Friedrichs <friedel@nomaden.org>
Pathname correction (tarfile now unpacks to Atari800-<version>, instead
of Atari800).
* Sun Dec 2 2001 Friedrich Delgado Friedrichs <friedel@nomaden.org>
First (working) version
