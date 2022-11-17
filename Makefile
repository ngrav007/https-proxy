CC = gcc

TARGET = proxy

BLDDIR = ./build
SRCDIR = ./src
INCDIR = ./include
BINDIR = ./bin

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
INCS = $(wildcard $(INCDIR)/*.h)

CFLAGS = -g -Wall -Wextra -I$(INCDIR) -fdiagnostics-color=always # -Werror
LDFLAGS = -g

.PHONY: all clean

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(INCS)
	$(CC) $(CFLAGS) -o $@ -c $< 

clean:
	rm -f $(BLDDIR)/*.o $(BINDIR)/$(TARGET) vgcore.*
