#!/bin/bash

sjasmplus --nologo --msg=war --fullpath --outprefix=.  --sld=loader.sld --lst=loader.lst --lstlab --syntax=f --sym=loader.sym --raw=loader.p loader.asm

sjasmplus --nologo --msg=war --fullpath --outprefix=.  --sld=helloworld.sld --lst=helloworld.lst --lstlab --syntax=f --sym=helloworld.sym --raw=helloworld.bin helloworld.asm

cc -o smloadgen smloadgen.c
cc -o smlfilegen smlfilegen.c

i686-w64-mingw32-gcc smloadgen.c -o smloadgen.exe
i686-w64-mingw32-gcc smlfilegen.c -o smlfilegen.exe

