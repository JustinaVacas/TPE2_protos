.PHONY=all test clean
COMPILER=gcc
CFLAGS = --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L
CFILES = *.c

all:
	pop3filter

test:
	for i in *_test;
	do;
	./$i;
	done;

pop3filter:
	$(COMPILER) $(CFLAGS) -I./include -o pop3filter ./main.c ./src/$(CFILES) 

clean:
	rm -f *.o pop3filter
	