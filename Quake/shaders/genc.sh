#!/bin/sh

set -e

SCRIPTDIR=`dirname "$0"`
OUT="$SCRIPTDIR/shaders.c"
HEADER=shaders.h

ppflags="-E -P -x c"
cpp=cc

echo "#include \"$HEADER\"" >"$OUT"

while [ "$1" != "" ] ; do
    if [ "$1" == "-f" ] ; then
        shift
        ppflags="$ppflags $1"
    elif [ "$1" == "-c" ] ; then
        shift
        cpp="$1"
    else
        shaderpath=$1
        shaderfile=`basename "$shaderpath"`
        shadername=`echo "glsl_$shaderfile" | sed 's/[ -.]/_/g'`

        ( $cpp $ppflags $shaderpath && head -c 1 /dev/zero )\
            | sed 's/^.*@/#/'\
            | xxd -i /dev/stdin\
            | sed -e s/_dev_stdin/$shadername/ -e 's/unsigned char/char/'\
            >>"$OUT"
    fi

    shift
done

