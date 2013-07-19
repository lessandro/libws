all:
	$(CC) -Wall -o example example.c ws.c base64.c sha1.c sev/sev.a -lev
