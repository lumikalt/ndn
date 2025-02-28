CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99
LDFLAGS = -lpthread
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
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
