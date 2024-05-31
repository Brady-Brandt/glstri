CC = clang
all: main.c
	$(CC) -O3 main.c -o glstri

remove: glstri
	rm glstri
