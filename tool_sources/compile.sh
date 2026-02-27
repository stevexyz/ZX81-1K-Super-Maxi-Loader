#!/bin/bash
cc -o smloadgen smloadgen.c
i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe
#./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=smloader.p smloader.asm

