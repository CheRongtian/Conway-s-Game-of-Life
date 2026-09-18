#!/bin/bash
clang Improved.c -o Improved $(sdl2-config --cflags --libs) -O2
./Improved