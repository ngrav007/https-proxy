#!/bin/bash

BINDIR=./bin

# Config file for http-proxy tests
PROXY=$BINDIR/http-proxy
CLIENT=$BINDIR/http-client
SERVER=$BINDIR/http-server

PROXY_PORT=9055
PROXY_HOST=localhost
SERVER_PORT=9066
TIMER=/usr/bin/time
TIMER_FLAGS="-p -o"
REPORT_DIR=$(dirname $0)/reports

FETCH="fetch"
CACHE="cache"
OUTPUT_EXT="output"
REPORT_EXT="report"
STDERR_EXT="stderr"
STDOUT_EXT="stdout"

PROXY_FLAGS="$PROXY_PORT"
SERVER_FLAGS="$SERVER_PORT"
CLIENT_FLAGS="$PROXY_HOST $PROXY_PORT"
