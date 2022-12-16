#!/bin/bash

PROXY_HALT_CODE="__halt__"

WORKDIR="/workspaces/Development/http-proxy/proxy"
BINDIR="${WORKDIR}/bin"
TESTDIR="${WORKDIR}/tests"
REPORT_DIR="${WORKDIR}/reports"
OUTPUT_DIR="${WORKDIR}/output"

if [ ! -d ${WORKDIR} ]; then
    echo "Error: ${WORKDIR} does not exist"
    exit 1
fi

if [ ! -d ${BINDIR} ]; then
    echo "Error: ${BINDIR} does not exist"
    exit 1
fi

if [ ! -d ${TESTDIR} ]; then
    echo "Error: ${TESTDIR} does not exist"
    exit 1
fi

if [ ! -d ${REPORT_DIR} ]; then
    echo "Error: ${REPORT_DIR} does not exist"
    exit 1
fi

if [ ! -d ${OUTPUT_DIR} ]; then
    echo "Error: ${OUTPUT_DIR} does not exist"
    exit 1
fi

# Executables
PROXY="${BINDIR}/proxy"
CLIENT="${BINDIR}/http-client"
SERVER="${BINDIR}/http-server"
TLS_CLIENT="${BINDIR}/tls-client"
TIMER="/usr/bin/time"

if [ ! -x ${PROXY} ]; then
    echo "Error: ${PROXY} does not exist or is not executable"
    exit 1
fi

if [ ! -x ${CLIENT} ]; then
    echo "Error: ${CLIENT} does not exist or is not executable"
    exit 1
fi

if [ ! -x ${SERVER} ]; then
    echo "Error: ${SERVER} does not exist or is not executable"
    exit 1
fi

if [ ! -x ${TLS_CLIENT} ]; then
    echo "Error: ${TLS_CLIENT} does not exist or is not executable"
    exit 1
fi

if [ ! -x ${TIMER} ]; then
    echo "Error: ${TIMER} does not exist or is not executable"
    exit 1
fi

# File Extensions
FETCH='fetch'
CACHE='cache'
REPORT_EXT='report'
STDERR_EXT='stderr'
STDOUT_EXT='stdout'
OUTPUT_EXT='output'

# Test Config
PROXY_PORT='9055'
PROXY_HOST='localhost'
PROXY_ADDR="${PROXY_HOST}:${PROXY_PORT}"
SERVER_PORT='9066'
TIMER_FLAGS='-p -o'
PROXY_FLAGS=${PROXY_PORT}
SERVER_FLAGS=${SERVER_PORT}
CLIENT_FLAGS="${PROXY_HOST} ${PROXY_PORT}"
TLS_CLIENT_FLAGS="${PROXY_HOST} ${PROXY_PORT}"

# Print out configuration
printf "[>] Test Configuration ------------------------------------------- +\n"
printf "  Proxy .................... ${PROXY} ${PROXY_FLAGS}\n"
printf "  FileServer ............... ${SERVER} ${SERVER_FLAGS}\n"
printf "  Client ................... ${CLIENT} ${CLIENT_FLAGS}\n"
printf "  TLS Client ............... ${TLS_CLIENT} ${TLS_CLIENT_FLAGS}\n"
printf "  Timer .................... ${TIMER} ${TIMER_FLAGS}\n"
printf "[<] -------------------------------------------------------------- -\n\n"


# Path: https-proxy/proxy/tests/.test-config.sh
