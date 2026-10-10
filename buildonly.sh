#!/usr/bin/env bash

/usr/bin/gcc -Wall -g -O2 -o xmouseless  xmouseless.c -lX11 -lXtst -lpthread -lXext
