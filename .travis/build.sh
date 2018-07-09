#!/bin/sh

#
# actual build script
# most of the steps are ported from the atari800.spec file
#
BUILDROOT="${PWD}/.travis/tmp"
OUT="${PWD}/.travis/out"

mkdir -p "${BUILDROOT}"
mkdir -p "${OUT}"

unset CC CXX

P=atari800

prefix=/usr
bindir=$prefix/bin
datadir=$prefix/share
icondir=$datadir/icons/hicolor
docdir=$datadir/doc/${P}

common_opts="--prefix=$prefix"

VERSION=`sed -n -e 's/^AC_INIT(Atari800, \([0-9][\.0-9]*\).*$/\1/p' src/configure.ac`

isrelease=false
ATAG=${VERSION}${archive_tag}
tag=`git tag --points-at ${TRAVIS_COMMIT}`
case $tag in
	ATARI800_*)
		isrelease=true
		;;
	*)
		ATAG=${VERSION}${archive_tag}-${SHORT_ID}
		;;
esac

NO_CONFIGURE=1 ./autogen.sh

targets="sdl"
maintarget="sdl"

options_sdl="--with-video=sdl --with-sound=sdl"
options_ncurses="--with-video=ncurses --with-sound=oss"
options_x11="--target=x11 --with-sound=oss"

case "$TRAVIS_OS_NAME" in
linux)
	for target in $targets
	do
		./configure $common_opts `eval echo \\\$options_${target}`
		make
		mv src/${P} src/${P}-${target}
		make clean
	done
	touch atari800

	make DESTDIR="${BUILDROOT}" install || exit 1
	for target in $targets
	do
	   install -m 755 src/${P}-${target} "${BUILDROOT}${bindir}"
	done
	pushd "${BUILDROOT}${bindir}"
	ln -sf ${P}-${maintarget} ${P}
	popd

	install debian/changelog "${BUILDROOT}$docdir/changelog.Debian"
	install DOC/ChangeLog "${BUILDROOT}$docdir/ChangeLog"
	install DOC/NEWS "${BUILDROOT}$docdir/NEWS"
	install DOC/CREDITS "${BUILDROOT}$docdir/CREDITS"
	install DOC/README "${BUILDROOT}$docdir/README"
	install DOC/USAGE "${BUILDROOT}$docdir/USAGE"
	install DOC/PORTING "${BUILDROOT}$docdir/PORTING"
	gzip -9nv "${BUILDROOT}$docdir"/*
	# These are too small to gzip
	install DOC/FAQ "${BUILDROOT}$docdir/FAQ"
	install DOC/BUGS "${BUILDROOT}$docdir/BUGS"
	install DOC/TODO "${BUILDROOT}$docdir/TODO"
	install debian/README.Debian "${BUILDROOT}$docdir/README.Debian"
	install debian/${P}.cfg "${BUILDROOT}$docdir/${P}.cfg"
	# and don't gzip the copyright statement
	install debian/copyright "${BUILDROOT}$docdir/copyright"
	# install and gzip man page
	install -d "${BUILDROOT}man/man1"
	install -m 644 src/${P}.man "${BUILDROOT}$datadir/man/man1/${P}.1"
	gzip -9nv "${BUILDROOT}man/man1"/*.1
	# install menu file
	install -d "${BUILDROOT}$datadir/menu"
	install -m 644 debian/menu "${BUILDROOT}$datadir/menu/${P}"
	
	ARCHIVE="${PROJECT_LOWER}-${ATAG}.tar.xz"
	(
	cd "${BUILDROOT}"
	tar cvfJ "${OUT}/${ARCHIVE}" .
	)
	;;

osx)
	DMG="${PROJECT_LOWER}-${VERSION}${archive_tag}.dmg"
	ARCHIVE="${PROJECT_LOWER}-${ATAG}.dmg"

	pushd src/macosx
	xcodebuild -derivedDataPath "$OUT" -project atari800.xcodeproj -configuration Release -scheme Packaging 
	popd
	
	mv "$OUT/Build/Products/Release/$DMG" "$OUT/$ARCHIVE" || exit 1

	;;

esac

export ARCHIVE
export isrelease

if $isrelease; then
	make dist
	for ext in gz bz2 xz lz; do
		ARCHIVE="${PROJECT_LOWER}-${VERSION}.tar.${ext}"
		test -f "${ARCHIVE}" && mv "${ARCHIVE}" "$OUT"
	done
fi
