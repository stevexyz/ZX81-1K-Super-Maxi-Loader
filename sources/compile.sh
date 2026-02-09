#!/bin/bash

sjasmplus/sjasmplus --nologo --msg=war --fullpath --outprefix=.  --sld=loader.sld --lst=loader.lst --lstlab --syntax=f --sym=loader.sym --raw=loader.p loader.asm

sjasmplus/sjasmplus --nologo --msg=war --fullpath --outprefix=.  --sld=loaded.sld --lst=loaded.lst --lstlab --syntax=f --sym=loaded.sym --raw=loaded.bin loaded.asm

cc -o smloadgen smloadgen.c
i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe

./smloadgen loaded.bin 0x4036 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x1E
