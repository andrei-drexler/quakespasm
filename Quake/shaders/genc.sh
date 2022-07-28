#!/bin/sh

SHADERPATH=$1
SHADERFILE=`basename "$SHADERPATH"`
SHADERNAME=`echo "glsl_$SHADERFILE" | sed 's/[ -.]/_/g'`
SHADER_C=`dirname "$SHADERPATH"`/"$SHADERFILE.c"

head -c 1 /dev/zero\
    | cat "$SHADERPATH" /dev/stdin\
    | xxd -i /dev/stdin\
    | sed -e s/_dev_stdin/$SHADERNAME/ -e '1i #include "shaders.h"'\
    >"$SHADER_C"
