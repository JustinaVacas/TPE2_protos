
CC       = gcc
# Compiling Flags:
CFLAGS   = -c -g --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L -pthread

LINKER 	 = gcc
# Linking Flags:
LFLAGS 	 = -g --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=2200809L  -lpthread -pthread

