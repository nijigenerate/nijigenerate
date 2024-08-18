#!/bin/sh
set -e

dub run gitver -- --prefix $2 --file source/$1/ver.d.new --mod $1.ver --appname $1
cmp source/$1/ver.d source/$1/ver.d.new &>/dev/null && rm source/$1/ver.d.new && exit 0 || true
mv -f source/$1/ver.d.new source/$1/ver.d
