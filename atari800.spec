# generic defines used by all distributions.
#
%define ver			4.2.0
# When adding a target, add an $options_<target_name> variable to the %build
# section, and add %{_bindir}/%{name}-<target_name> to the files list in the
# %files section.
%define targets			sdl
%define maintarget		sdl

%define	myrelease		1
%define mybuild			1
%define _rel			%{myrelease}.%{mybuild}

# define the package groups. If they all followed the same naming convention,
# these would be the same. They don't, and so they aren't :(
#
%define	suse_group		System/Emulators/Other
%define	mandriva_group		Console/Emulators
%define	fedora_group		Console/Emulators

# defaults
#
%define	group			Console/Emulators
%define	rel			%{_rel}

%define	my_suse			0
%define	my_mandriva		0
%define	my_fedora		0
%define	my_centos		0


%if 0%{?suse_version:1}%{?sles_version:1}
%define	my_suse			1
%endif

# if present, use %distversion to find out which Mandriva version is being built
#
%if 0%{?distversion:1}
%if 0%{?!mandriva_version:1}
%define	mandriva_version	%(echo $[%{distversion}/10])
%endif

%endif

%if 0%{?mandriva_version:1}
%define	my_mandriva		1
%define my_vendor		mandriva
%endif

# if present, decode %dist to find out which OS package is being built on
#
%if 0%{?dist:1}

# Centos or Fedora
#
%define	my_which_os		%(i=%{dist} ; if [ "${i::3}" == ".fc" ] ; then echo "1" ; else echo "0" ; fi )

%if %{my_which_os}

%if 0%{?!fedora_version:1}
%define fedora_version		%(i=%{dist} ; echo "${i:3}" )
%endif

%else

%if 0%{?!centos_version:1}
%define centos_version		%(i=%{dist} ; echo "${i:3}00" )
%endif

%endif

%endif

%if 0%{?fedora_version:1}
%define	my_fedora		1
%define my_vendor		fedora
%endif

%if 0%{?centos_version:1}
%define	my_centos		1
%define my_vendor		centos
%endif


%if %{my_suse}

%if %{suse_version}
%define	rel			%{myrelease}.suse%(echo $[%suse_version/10]).%{mybuild}
%else
%define	rel			%{myrelease}.sles%{sles_version}.%{mybuild}
%endif

%define	group			%{suse_group}

%endif


# building on a Mandriva/Mandrake Linux system.
#
# this should create a release that conforms to the Mandriva naming conventions.
#
%if %{my_mandriva}

%define rel			%{myrelease}.mdv%{mandriva_version}.%{mybuild}

%define group			%{mandriva_group}

%endif


# building on a Fedora Core Linux system.
#
# this should create a release that conforms to the Fedora naming conventions.
#
%if %{my_fedora}

%if 0%{?!fedora_version:1}
%define	fedora_version		%(i="%dist" ; echo "${i:3}")
%endif

%if 0%{?!dist:1}
%define	dist			.fc%{fedora_version}
%endif

%define	rel			%{myrelease}%{dist}.%{mybuild}
%define	group			%{fedora_group}

%endif


# building on a Centos Linux system.
#
# this should create a release that conforms to the Centos naming conventions.
#
%if %{my_centos}

%if 0%{?!centos_version:1}
%define	centos_version		%(i="%dist" ; echo "${i:3}")
%endif

%if 0%{?!dist:1}
%define	dist			.el%{centos_version}
%endif

%define	rel			%{myrelease}%{dist}.%{mybuild}
%define	group			%{fedora_group}

%endif


%if %{my_suse}
Requires:			SDL >= 1.2.10
BuildRequires:			SDL-devel >= 1.2.10
%endif

%if %{my_mandriva}
Requires:			libSDL >= 1.2.10
BuildRequires:			libSDL-devel >= 1.2.10
%endif

%if %{my_fedora}
Requires:			SDL >= 1.2.10
BuildRequires:			SDL-devel >= 1.2.10
%endif


# Now for the meat of the spec file
#
Name:			atari800
Version:		%{ver}
Summary:		An emulator of 8-bit Atari personal computers
License:		GPLv2
URL:			http://atari800.github.io/
Source:			http://prdownloads.sourceforge.net/atari800/%{name}-%{version}.tar.gz
Group:			%{group}
Release:		%{rel}
BuildRoot:		%{_tmppath}/%{name}-%{version}-%{release}-buildroot
BuildRequires:		zlib-devel
BuildRequires:		libpng-devel
BuildRequires:		readline-devel


%description
Atari800 is an emulator for Atari 8-bit computer systems including the 400,
800, 1200XL, 600XL, 800XL, 65XE, 130XE, 800XE and the XE Game System, and also
for the Atari 5200 SuperSystem console. It can be used on console, FrameBuffer
or X11. It features excellent compatibility, HIFI sound support, artifacting
emulation, precise cycle-exact ANTIC/GTIA emulation and more.

Authors:
David Firth
and Atari800 Development Team (see CREDITS for a full list)


%prep
%setup -q -n %{name}-%{version}


%build
options_sdl="--with-video=sdl --with-sound=sdl"
#options_ncurses="--with-video=ncurses --with-sound=oss"
#options_x11="--target=x11 --with-sound=oss"

for target in %{targets}
do
	%configure `eval echo \\\$options_${target}`
	%{__make} %{?jobs:-j%jobs}
	mv src/%{name} src/%{name}-${target}
	%{__make} clean
done
touch atari800


%install
cd src
mkdir -p %{buildroot}/%{_bindir}
mkdir -p %{buildroot}/%{_mandir}/man1
for target in %{targets}
do
	install -m 755 %{name}-$target %{buildroot}/%{_bindir}
done
(
	cd %{buildroot}/%{_bindir}
	ln -sf %{name}-%{maintarget} %{name}
)

mv %{name}.man %{name}.1

install -m 644 %{name}.1 %{buildroot}/%{_mandir}/man1/


%files
%defattr(-,root,root)
%{_bindir}/%{name}
%{_bindir}/%{name}-%{maintarget}
%{_mandir}/man1/%{name}.1.*
%doc COPYING
%doc README.TXT
%doc DOC/BUGS
%doc DOC/ChangeLog
%doc DOC/CREDITS
%doc DOC/FAQ
%doc DOC/INSTALL
%doc DOC/NEWS
%doc DOC/PORTING
%doc DOC/README
%doc DOC/TODO
%doc DOC/USAGE


%clean
%{__rm} -rf %{buildroot}
%{__rm} -rf %{_builddir}/%{name}-%{version}-%{release}-buildroot
