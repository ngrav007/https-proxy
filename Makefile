CC = gcc

PROXY = proxy
CLIENT = http-client
SERVER = http-server
BLDDIR = ./build
SRCDIR = ./src
INCDIR = ./include
BINDIR = ./bin
CLIDIR = ./client
SERDIR = ./server

PROXY_MAIN = $(BLDDIR)/http-proxy.o
CLIENT_MAIN = $(BLDDIR)/http-client.o
SERVER_MAIN = $(BLDDIR)/http-server.o 

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
INCS = $(wildcard $(INCDIR)/*.h)

CLIOBJS = $(subst $(PROXY_MAIN), $(CLIENT_MAIN), $(OBJS))
SEROBJS = $(subst $(CLIENT_MAIN), $(SERVER_MAIN), $(CLIOBJS))

CFLAGS = -g -Wall -Wextra -I$(INCDIR) -fdiagnostics-color=always # -Werror
LDFLAGS = -g 

.PHONY: all clean

all: $(BINDIR)/$(PROXY) $(BINDIR)/$(CLIENT) $(BINDIR)/$(SERVER)

$(BINDIR)/$(PROXY): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(INCS)
	$(CC) $(CFLAGS) -o $@ -c $< 

$(BINDIR)/$(CLIENT): $(CLIOBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BLDDIR)/$(CLIENT).o: $(CLIDIR)/$(CLIENT).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

$(BINDIR)/$(SERVER): $(SERVER_MAIN) $(SEROBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BLDDIR)/$(SERVER).o: $(SERDIR)/$(SERVER).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f $(BLDDIR)/*.o $(BINDIR)/* vgcore.* core.*
