#!/bin/bash

#./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=smloader.p smloader.asm

cc -O2 -o smloadgen smloadgen.c
i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe

cc -o p2wav p2wav.c
i686-w64-mingw32-gcc p2wav.c -o p2wav.exe

cc -O2 -o smlwavpp smlwavpp.c -lm
i686-w64-mingw32-gcc smlwavpp.c -o smlwavpp.exe -lm

