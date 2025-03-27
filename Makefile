CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
LDFLAGS =
SRC = $(wildcard *.c) $(wildcard protocols/*.c)
OBJ = $(SRC:.c=.o)
EXEC = ndn

all: $(EXEC)
OBJDIR = obj
OBJ = $(patsubst %.c, $(OBJDIR)/%.o, $(SRC))

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)   # Ensure subdirectories exist
	$(CC) $(CFLAGS) -c $< -o $@

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

debug: CFLAGS += -g
debug: clean $(EXEC)

valgrind: debug
	valgrind --leak-check=full \
		--track-origins=yes \
		--show-reachable=yes \
		./$(EXEC) $(filter-out $@,$(MAKECMDGOALS))

.PHONY: all clean valgrind

# Prevent make from interpreting the command line arguments as make targets
%:
	@:
