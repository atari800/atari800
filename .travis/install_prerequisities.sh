#!/bin/sh

echo rvm_autoupdate_flag=0 >> ~/.rvmrc

case "$TRAVIS_OS_NAME" in
linux)
	sudo apt-get update
	sudo apt-get install -y \
		curl \
		libsdl1.2-dev \
		
	;;

osx)
	curl --get https://www.libsdl.org/release/SDL-1.2.15.dmg --output SDL.dmg
	mkdir -p ~/Library/Frameworks
	
	mountpoint=~/sdltmp
	mkdir "$mountpoint"

	hdiutil attach -mountpoint "$mountpoint" -readonly SDL.dmg
	cp -R "$mountpoint/SDL.framework" ~/Library/Frameworks
	hdiutil detach "$mountpoint"

	rmdir "$mountpoint"
	
	rm -rf src/macosx/SDL.Framework
	rm -rf src/macosx/SDL_image.Framework
	ln -s ~/Library/Frameworks/SDL.Framework src/Unix/MacOSX/SDL.Framework
	ln -s ~/Library/Frameworks/SDL_image.Framework src/Unix/MacOSX/SDL_image.Framework
	
	# we must install the macports version of the dependencies,
	# because the brew packages are not universal
	mkdir src/Unix/MacOSX/external
	for i in gmp/gmp-6.1.2_0+universal.darwin_16.i386-x86_64.tbz2 \
		mpfr/mpfr-3.1.5_0+universal.darwin_16.i386-x86_64.tbz2 \
		jpeg/jpeg-9b_0+universal.darwin_16.i386-x86_64.tbz2; do
		f=`basename $i`
		curl --get https://packages.macports.org/$i --output $f
		tar -C src/Unix/MacOSX/external --include="./opt/local" --strip-components=3 -xf $f
	done
	;;

*)
	exit 1
	;;
esac
