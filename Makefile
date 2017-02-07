CC=gcc
COPT=  -I/usr/local/include -I/usr/local/include/freetype2 -g -Wall -fsanitize=bounds -fsanitize-undefined-trap-on-error -fstack-protector-all -fno-omit-frame-pointer
LOPT= -L/usr/local/lib -lhpdfs -lz -lfreetype


all:	analyse

analyse:	Makefile analyse.c
	$(CC) -o analyse analyse.c $(COPT) $(LOPT)

