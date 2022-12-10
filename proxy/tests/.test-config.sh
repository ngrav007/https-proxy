#!/bin/bash

WORKDIR="/workspaces/Development/https-proxy/proxy"
TESTDIR="${WORKDIR}/tests"
BINDIR="${WORKDIR}/bin"
REPORT_DIR="${WORKDIR}/reports"
OUTPUT_DIR="${WORKDIR}/output"

# Config file for http-proxy tests
PROXY="${BINDIR}/proxy"
CLIENT="${BINDIR}/http-client"
SERVER="${BINDIR}/http-server"
TLS_CLIENT="${BINDIR}/tls-client"

PROXY_PORT="9055"
PROXY_HOST="localhost"
SERVER_PORT="9066"
TIMER="time"
TIMER_FLAGS="-p -o"

FETCH="fetch"
CACHE="cache"
OUTPUT_EXT="output"
REPORT_EXT="report"
STDERR_EXT="stderr"
STDOUT_EXT="stdout"

PROXY_FLAGS="${PROXY_PORT}"
SERVER_FLAGS="${SERVER_PORT}"
CLIENT_FLAGS="${PROXY_HOST} ${PROXY_PORT}"

# # Check for root
# if [ $(id -u) -ne 0 ]; then
#     echo "You must be root to run this script"
#     exit 1
# fi
