#!/bin/bash

source /workspaces/Development/http-proxy/proxy/tests/.test-config.sh

# Halt the proxy
# Test Setup
METHOD=${PROXY_HALT_CODE}
URL="/"
HOST="eregion.proxy"

(${CLIENT} ${CLIENT_FLAGS} ${METHOD} ${HOST} ${URL})

printf "[+] Proxy Halted\n"

exit 0
