#!/bin/bash

cc -o smloadgen smloadgen.c
cc -o smlfilegen smlfilegen.c

i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe
i686-w64-mingw32-gcc smlfilegen.c -o smlfilegen.exe

./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=smloader.p smloader.asm

./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=helloworld.bin helloworld.asm

