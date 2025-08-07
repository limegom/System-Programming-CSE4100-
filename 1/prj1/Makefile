CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
OBJS = main.o list.o hash.o bitmap.o debug.o hex_dump.o

testlib: $(OBJS)
	$(CC) $(CFLAGS) -o testlib $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) testlib
