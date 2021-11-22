include Makefile.inc

TARGET := pop3filter
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
rm       = rm -rf


all: comp link

comp:$(SOURCES)
	$(CC) $(CFLAGS) -I./include -I../utils/include $(SOURCES)
	@echo "pop3filter Compilation complete."

link:$(OBJECTS)
	$(LINKER) $(LFLAGS) $(OBJECTS) ../utils/*.o -o $(TARGET).out
	@echo "pop3filter Linking complete."

%.o : %.c
	$(CC) $(CFLAGS) -I./include -c $< -o $@

%.out : %.o
	$(LINKER) $(LFLAGS) $< -o $@

clean:
	@$(rm) $(OBJECTS)
	@$(rm) $(TARGET).out
	@echo "pop3filter Cleanup complete."
	