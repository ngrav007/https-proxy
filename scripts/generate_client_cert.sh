#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

GENERATE_CERT=/workspaces/Development/https-proxy/scripts/generate_cert.sh

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

${GENERATE_CERT} ${CLIENT_CN}
