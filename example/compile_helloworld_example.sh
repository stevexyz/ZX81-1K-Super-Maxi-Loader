#!/bin/bash
./sjasmplus --nologo --msg=war --fullpath --outprefix=.  --syntax=f --raw=helloworld.bin helloworld.asm
../smloadgen helloworld.bin 0x4036 0x0123 0x4567 0x89AB 0xCDEF 0x0281 0x4000 0x1E
