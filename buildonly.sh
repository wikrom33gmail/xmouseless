#!/usr/bin/env bash

/usr/bin/gcc -Wall -g -o xmouseless  xmouseless.c -lX11 -lXtst -lpthread -lXext
