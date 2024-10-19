#!/bin/bash

# generate shader include files:
xxd -i -c 20 -n vertexShaderArr   atari800-shader.vert > ./gen-atari800-shader.vert.h
xxd -i -c 20 -n fragmentShaderArr atari800-shader.frag > ./gen-atari800-shader.frag.h

# those would be nicer, but exceed max string limit:
# awk 'BEGIN {print "GLchar* fragmentShader ="} {print "\"" $0 "\\n\""} END {print ";"}' ./atari800-shader.frag > ./gen-atari800-shader.frag.h
# awk 'BEGIN {print "GLchar* vertexShader ="}   {print "\"" $0 "\\n\""} END {print ";"}' ./atari800-shader.vert > ./gen-atari800-shader.vert.h
