#! /bin/sh

autoheader
autoconf

echo
echo "Now you need to run the configure script.  The configure script requires a"
echo "\"--target=TARGET\" option, and it may also take various \"--enable-FEATURE\""
echo "options."
echo
echo "Run \"./configure --help\" to see all available options."
echo "Run \"./configure --help=short\" just to see the Atari800 FEATURE options."
echo "Run \"./configure\" without a parameter to see the valid targets."
echo
