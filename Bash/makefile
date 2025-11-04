# The compiler that we will be using
CC=gcc
# Any C program flags
CFLAGS=-g -Wall -Werror
# Any include directories
INCLUDE=./include

# Execute one test at a time in a separate process.
# This will allow the program to move forard, even if there is a segfault.
all: ./*.c
	$(CC) $(CFLAGS) ./*.c -I$(INCLUDE) -o ./bin/shell
	./bin/shell

run:
	./bin/shell


# Removes the binary files automatically
clean:
	rm ./bin/shell
