CC=gcc

callisto: callisto.c
	$(CC) callisto.c -o callisto -Wall -Wextra -pedantic -std=c11
