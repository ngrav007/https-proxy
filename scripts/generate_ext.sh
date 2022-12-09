#!/bin/bash

# Generate a extension file for a dev certificate
# Usage: generate_ext_file.sh <common name>

source /workspaces/Development/https-proxy/scripts/config/.config

function generate_ext()
{
    # Get Common Name
    if [ -z $1 ]; then
        echo "Usage: generate_ext.sh <common name>"
        exit 1
    fi
    CN=$1

    # Check for sudo privileges
    if [[ $EUID -ne 0 ]]; then
        echo "[!] This script must be run as root" 1>&2
        exit 1
    fi

    # Ensure Root CA exists
    if [ ! -f "${ROOTCA_CRT}" ]; then
        echo "[!] Root CA certificate ${ROOT_CA_CRT} not found"
        exit 1
    fi

    # Ensure Root CA Key exists
    if [ ! -f "${ROOTCA_KEY}" ]; then
        echo "[!] Root CA key ${ROOT_CA_KEY} not found"
        exit 1
    fi

    # Extension file
    EXT="${LOCAL_EXTS}/${CN}.ext"

    # Generate extension file
    echo "authorityKeyIdentifier=keyid,issuer" > "${EXT}"
    echo "basicConstraints=CA:FALSE" >> "${EXT}"
    echo "keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment" >> "${EXT}"
    echo "subjectAltName = @alt_names" >> "${EXT}"
    echo "[alt_names]" >> "${EXT}"
    echo "DNS.1 = ${CN}" >> "${EXT}"
    echo "DNS.2 = *.${CN}" >> "${EXT}"
}

# Generate extension file
generate_ext "${1}"
