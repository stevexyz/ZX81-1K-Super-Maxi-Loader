#!/bin/bash

#./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=smloader.p smloader.asm

cc -o smloadgen smloadgen.c
i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe

cc -o p2wav p2wav.c
i686-w64-mingw32-gcc p2wav.c -o p2wav.exe

cc -o wav2stereo wav2stereo.c
i686-w64-mingw32-gcc wav2stereo.c -o wav2stereo.exe

