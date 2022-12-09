#!/bin/bash

# This script is used to display the certificate information for a given
# certificate. 

source /workspaces/Development/https-proxy/scripts/config/.config

# Get Common Name
if [ -z $1 ]; then
    echo "Usage: display_cert.sh <common name>"
    exit 1
fi

# Files
CRT="${LOCAL_CERTS}/${1}.crt"
PEM="${LOCAL_CERTS}/${1}.pem"

# Ensure certificate exists
if [ ! -f "${CRT}" ]; then
    if [ -f "${PEM}" ]; then
        CRT="${PEM}"
    else
        echo "[!] Certificate ${CRT} not found"
        exit 1
    fi
fi

# Display certificate information
echo "[*] Displaying certificate information for ${CRT}"
echo " -------------------------------------------------- "
openssl x509 -in ${CRT} -text -noout
echo " -------------------------------------------------- "
