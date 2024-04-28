#!/bin/bash
set -e

DIR_ATARI800_SRC="$PWD/../atari800.git"
DIR_ATARI800_CLASSIC="atari800"
DIR_ATARI800_FAST="atari800-fast"
DIR_ATARI800_SDL="atari800-sdl"
DIR_ATARI800_CF="atari800-cf"

DIR_SYSROOT="$(m68k-atari-mint-gcc -print-sysroot)"

COMMON_FLAGS='--host=m68k-atari-mint --enable-veryslow --disable-nonlinear_mixing --disable-pbi_bb --disable-pbi_mio --with-readline=no
	--disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-monitorutf8 --disable-monitoransi
	--disable-eventrecording --disable-pokeyrec --disable-videorecording --disable-screenshots --disable-audiorecording'

rm -rf ${DIR_ATARI800_CLASSIC} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}
mkdir -p ${DIR_ATARI800_CLASSIC} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}

cd ${DIR_ATARI800_SRC}
make distclean || echo "Already cleaned"
cd -

cd ${DIR_ATARI800_CLASSIC}
${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=falcon --enable-falconcpuasm --disable-interpolatesound && make V=1
cd -

cd ${DIR_ATARI800_FAST}
${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=falcon --enable-falconcpuasm --disable-interpolatesound --disable-newcycleexact && make V=1
cd -

cd ${DIR_ATARI800_SDL}
PATH=${DIR_SYSROOT}/usr/bin/m68020-60:$PATH ${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=default --enable-falconcpuasm --disable-interpolatesound && make V=1
cd -

cd ${DIR_ATARI800_CF}
PATH=${DIR_SYSROOT}/usr/bin/m5475:$PATH ${DIR_ATARI800_SRC}/configure $COMMON_FLAGS --target=firebee --enable-pagedattrib --enable-cyclesperopcode && make V=1
cd -

for d in ${DIR_ATARI800_CLASSIC} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}
do
	m68k-atari-mint-stack --fix=256k "$d/src/atari800"
	m68k-atari-mint-flags -S "$d/src/atari800"
	m68k-atari-mint-strip -s "$d/src/atari800"
done

cp ${DIR_ATARI800_CLASSIC}/src/atari800 atari800.gtp
cp ${DIR_ATARI800_FAST}/src/atari800 atarifst.gtp
cp ${DIR_ATARI800_SDL}/src/atari800 atarisdl.gtp
cp ${DIR_ATARI800_CF}/src/atari800 atari_cf.gtp
