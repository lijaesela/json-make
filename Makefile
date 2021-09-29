CC = clang
FLAGS = -Wall -Wextra -std=c11 -pedantic
LIBS = -ljson-c

json-make: json-make.c
	$(CC) $(FLAGS) -o json-make json-make.c $(LIBS)

run: json-make
	cp -f json-make example/
	make -C example
