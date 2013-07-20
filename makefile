all:
	$(CC) -std=c99 -g -Wall -o example *.c sev/sev.a -lev
