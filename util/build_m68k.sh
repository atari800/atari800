#!/bin/bash
set -e

DIR_ATARI800_SRC="$(cd "$(dirname "$0")/.." && pwd)"
DIR_BUILD="${DIR_ATARI800_SRC}/build"
DIR_ATARI800_DEFAULT="${DIR_BUILD}/atari800"
DIR_ATARI800_FAST="${DIR_BUILD}/atari800-fast"
DIR_ATARI800_SDL="${DIR_BUILD}/atari800-sdl"
DIR_ATARI800_CF="${DIR_BUILD}/atari800-cf"

DIR_SYSROOT="$(m68k-atari-mintelf-gcc -print-sysroot)"

COMMON_FLAGS='--host=m68k-atari-mintelf --enable-veryslow --disable-nonlinear_mixing --disable-pbi_bb --disable-pbi_mio --with-readline=no
	--disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-monitorutf8 --disable-monitoransi
	--disable-eventrecording --disable-pokeyrec --disable-videorecording --disable-screenshots --disable-audiorecording'

rm -rf ${DIR_BUILD}
mkdir -p ${DIR_ATARI800_DEFAULT} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}

(
	cd ${DIR_ATARI800_SRC}
	make distclean || echo "Already cleaned"
)

(
	cd ${DIR_ATARI800_DEFAULT}
	export PATH=${DIR_SYSROOT}/usr/bin/m68020-60:$PATH
	${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=falcon --enable-falconcpuasm --disable-interpolatesound
	make V=1
)

(
	cd ${DIR_ATARI800_FAST}
	export PATH=${DIR_SYSROOT}/usr/bin/m68020-60:$PATH
	${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=falcon --enable-falconcpuasm --disable-interpolatesound --disable-newcycleexact
	make V=1
)

(
	cd ${DIR_ATARI800_SDL}
	export PATH=${DIR_SYSROOT}/usr/bin/m68020-60:$PATH
	${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=default --with-video=sdl --enable-falconcpuasm --disable-interpolatesound
	make V=1
)

(
	cd ${DIR_ATARI800_CF}
	export PATH=${DIR_SYSROOT}/usr/bin/m5475:$PATH
	${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=firebee --with-video=sdl --enable-pagedattrib --enable-cyclesperopcode
	make V=1
)

for d in ${DIR_ATARI800_DEFAULT} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}
do
	m68k-atari-mintelf-stack --fix=256k "$d/src/atari800"
	m68k-atari-mintelf-flags -S "$d/src/atari800"
	m68k-atari-mintelf-strip -s "$d/src/atari800"
done

cp ${DIR_ATARI800_DEFAULT}/src/atari800 ${DIR_BUILD}/atari800.gtp
cp ${DIR_ATARI800_FAST}/src/atari800 ${DIR_BUILD}/atarifst.gtp
cp ${DIR_ATARI800_SDL}/src/atari800 ${DIR_BUILD}/atarisdl.gtp
cp ${DIR_ATARI800_CF}/src/atari800 ${DIR_BUILD}/atari_cf.gtp
