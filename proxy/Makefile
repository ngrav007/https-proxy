CC = gcc

PROXY = proxy
CLIENT = http-client
SERVER = http-server
TLSCLI = tls-client
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
TLSCLI_MAIN = $(BLDDIR)/tls-client.o 

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(BLDDIR)/%.o, $(SRCS))
INCS = $(wildcard $(INCDIR)/*.h)

CLIOBJS = $(subst $(PROXY_MAIN), $(CLIENT_MAIN), $(OBJS))
SEROBJS = $(subst $(CLIENT_MAIN), $(SERVER_MAIN), $(CLIOBJS))
TLSOBJS = $(subst $(SERVER_MAIN), $(TLSCLI_MAIN), $(SEROBJS))

CFLAGS = -g -Wall -Wextra -fdiagnostics-color=always -I$(INCDIR) -I/opt/homebrew/opt/openssl@3/include  # -Werror
LDFLAGS = -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

.PHONY: all clean

all: $(BINDIR)/$(PROXY) $(BINDIR)/$(CLIENT) $(BINDIR)/$(SERVER) $(BINDIR)/$(TLSCLI)

$(BINDIR)/$(PROXY): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BLDDIR)/%.o: $(SRCDIR)/%.c $(INCS)
	$(CC) $(CFLAGS) -o $@ -c $< 

# Client
$(BINDIR)/$(CLIENT): $(CLIOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BLDDIR)/$(CLIENT).o: $(CLIDIR)/$(CLIENT).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

# TLS/SSL Client
$(BINDIR)/$(TLSCLI): $(TLSOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BLDDIR)/$(TLSCLI).o: $(CLIDIR)/$(TLSCLI).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

# File Server
$(BINDIR)/$(SERVER): $(SERVER_MAIN) $(SEROBJS)
	$(CC) $(CFLAGS) -o $@ $^  $(LDFLAGS)

$(BLDDIR)/$(SERVER).o: $(SERDIR)/$(SERVER).c $(INCS) ./build/http.o
	$(CC) $(CFLAGS) -o $@ -c $<

filesystem:
	mkdir -p $(BLDDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(OUTDIR)
	mkdir -p $(REPORTDIR)
	

clean:
	rm -f $(BLDDIR)/*.o $(BINDIR)/* vgcore.* core.* $(OUTDIR)/*

clean_reports:
	rm -f $(REPORTDIR)/*

clean_output:
	rm -f $(OUTDIR)/*
