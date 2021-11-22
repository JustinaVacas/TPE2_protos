include Makefile.inc

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all:$(SOURCES)
	$(CC) $(CFLAGS) -I./include $(SOURCES)
	@echo "Utils compilation complete."


%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@echo "Utils cleanup complete."

