#!/bin/bash
set -e

DIR_ATARI800_SRC="$PWD/../atari800.git"
DIR_ATARI800_CLASSIC="atari800"
DIR_ATARI800_FAST="atari800-fast"
DIR_ATARI800_SDL="atari800-sdl"
DIR_ATARI800_CF="atari800-cf"

rm -rf ${DIR_ATARI800_CLASSIC} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}
mkdir -p ${DIR_ATARI800_CLASSIC} ${DIR_ATARI800_FAST} ${DIR_ATARI800_SDL} ${DIR_ATARI800_CF}

cd ${DIR_ATARI800_SRC}
git clean -f -x -d -f
./autogen.sh
cd -

cd ${DIR_ATARI800_CLASSIC}
${DIR_ATARI800_SRC}/configure --host=m68k-atari-mint --enable-veryslow --disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-eventrecording --disable-monitorutf8 --disable-monitoransi --disable-pbi_bb --disable-pbi_mio --disable-riodevice --with-readline=no \
	--target=falcon --enable-falconcpuasm \
	&& make V=1
cd -

cd ${DIR_ATARI800_FAST}
${DIR_ATARI800_SRC}/configure --host=m68k-atari-mint --enable-veryslow --disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-eventrecording --disable-monitorutf8 --disable-monitoransi --disable-pbi_bb --disable-pbi_mio --disable-riodevice --with-readline=no \
	--target=falcon --enable-falconcpuasm --disable-newcycleexact \
	&& make V=1
cd -

cd ${DIR_ATARI800_SDL}
PATH=$HOME/gnu-tools/m68000/m68k-atari-mint/bin/m68020-60:$PATH ${DIR_ATARI800_SRC}/configure --host=m68k-atari-mint --enable-veryslow --disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-eventrecording --disable-monitorutf8 --disable-monitoransi --disable-pbi_bb --disable-pbi_mio --disable-riodevice --with-readline=no \
	--target=default --enable-falconcpuasm \
	&& make V=1
cd -

cd ${DIR_ATARI800_CF}
PATH=$HOME/gnu-tools/m68000/m68k-atari-mint/bin/m5475:$PATH ${DIR_ATARI800_SRC}/configure --host=m68k-atari-mint --enable-veryslow --disable-monitorbreak --disable-monitorhints --disable-crashmenu --disable-monitorasm --disable-eventrecording --disable-monitorutf8 --disable-monitoransi --disable-pbi_bb --disable-pbi_mio --disable-riodevice --with-readline=no \
	--target=firebee --enable-pagedattrib --enable-cyclesperopcode \
	&& make V=1
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
