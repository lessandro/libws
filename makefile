all:
	$(CC) -Wall -o example example.c ws.c sev/sev.a -lev
