all:	analyse

analyse:	Makefile analyse.c
	$(CC) -o analyse analyse.c -Wall -g -O3

