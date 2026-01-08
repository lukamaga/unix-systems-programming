# Makefile
# make
# make run
# make clean

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS :=

BIN := pipeline_shell
SRC := pipeline_shell.c

.PHONY: all clean run

all: fmt $(BIN)

fmt:
	@tail -c1 $(SRC) 2>/dev/null | od -An -t x1 | grep -q '0a' || \
		printf '\n' >> $(SRC)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
