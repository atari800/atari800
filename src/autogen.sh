#! /bin/sh

autoheader
autoconf

echo
echo "Now run ./configure --target (see below) and then type make"
echo

./configure --target help
