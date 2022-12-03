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
OUTDIR = ./output
TESTDIR = ./tests
REPORTDIR = ./reports

PROXY_MAIN = $(BLDDIR)/http-proxy.o
CLIENT_MAIN = $(BLDDIR)/http-client.o
SERVER_MAIN = $(BLDDIR)/http-server.o 

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
INCS = $(wildcard $(INCDIR)/*.h)

CLIOBJS = $(subst $(PROXY_MAIN), $(CLIENT_MAIN), $(OBJS))
SEROBJS = $(subst $(CLIENT_MAIN), $(SERVER_MAIN), $(CLIOBJS))

CFLAGS = -g -Wall -Wextra -I$(INCDIR) -fdiagnostics-color=always -I/workspaces/Development/lib/openssl/include/openssl  # -Werror
LDFLAGS = -g -I/workspaces/Development/lib/openssl/include/openssl -L/workspaces/Development/openssl -L/workspaces/Development/lib/openssl/lib 
LDLIBS = -lssl -lcrypto

.PHONY: all clean

all: $(BINDIR)/$(PROXY) $(BINDIR)/$(CLIENT) $(BINDIR)/$(SERVER)

$(BINDIR)/$(PROXY): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

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
	rm -f $(BLDDIR)/*.o $(BINDIR)/* vgcore.* core.* $(OUTDIR)/* $(TESTDIR)/$(REPORTDIR)/*
