
CC       = gcc
# Compiling Flags:
CFLAGS   = -c -g --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L

LINKER 	 = gcc
# Linking Flags:
LFLAGS 	 = -g --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L  -lpthread -pthread


