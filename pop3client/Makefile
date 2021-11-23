include Makefile.inc

BINARY := pop3client
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all: comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) -I./include -I../utils/include $(SOURCES)
	@echo "pop3client Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ../utils/*.o -o $(BINARY).out
	@echo "pop3client Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(BINARY).out
	@echo "pop3client Cleanup complete."
	