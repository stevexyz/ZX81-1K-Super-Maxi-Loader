#!/bin/bash
tcc -o smloadgen smloadgen.c
tcc -o p2wav p2wav.c
tcc -o smlwavpp smlwavpp.c -lm

