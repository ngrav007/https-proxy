CC = gcc

PROXY = proxy
CLIENT = http-client
BLDDIR = ./build
SRCDIR = ./src
INCDIR = ./include
BINDIR = ./bin
CLIDIR = ./client

SERVER_MAIN = $(BLDDIR)/main.o
CLIENT_MAIN = $(BLDDIR)/http-client.o
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
INCS = $(wildcard $(INCDIR)/*.h)
CLIOBJS = $(subst $(SERVER_MAIN), $(CLIENT_MAIN), $(OBJS))

CFLAGS = -g -Wall -Wextra -I$(INCDIR) -fdiagnostics-color=always # -Werror
LDFLAGS = -g 

.PHONY: all clean

all: $(BINDIR)/$(PROXY) $(BINDIR)/$(CLIENT)

$(BINDIR)/$(PROXY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(INCS)
	$(CC) $(CFLAGS) -o $@ -c $< 

$(BINDIR)/$(CLIENT): $(CLIOBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BLDDIR)/$(CLIENT).o: $(CLIDIR)/$(CLIENT).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(BLDDIR)/*.o $(BINDIR)/$(PROXY) vgcore.* core.*
