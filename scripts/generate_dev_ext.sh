#!/bin/bash

# Generate a extension file for a dev certificate
# Usage: generate_ext_file.sh <common name>

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Ensure Root CA exists
if [ ! -f "${ROOT_CA_CRT}" ]; then
    echo "[!] Root CA certificate ${ROOT_CA_CRT} not found"
    exit 1
fi

# Ensure Root CA Key exists
if [ ! -f "${ROOT_CA_KEY}" ]; then
    echo "[!] Root CA key ${ROOT_CA_KEY} not found"
    exit 1
fi

# Get Common Name
if [ -z "$1" ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi
# CN=$1
EXT_FILE=$1

# Extension file
# EXT_FILE=${EXT_DIR}/${CN}.ext
# if [ -f ${EXT_FILE} ]; then
#     echo "[!] Extension file ${EXT_FILE} already exists"
#     rm ${EXT_FILE} # TODO - should we always remove?
# fi

# Generate extension file
echo "authorityKeyIdentifier=keyid,issuer" > ${EXT_FILE}
echo "basicConstraints=CA:FALSE" >> ${EXT_FILE}
echo "keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment" >> ${EXT_FILE}
echo "subjectAltName = @alt_names" >> ${EXT_FILE}
echo "[alt_names]" >> ${EXT_FILE}
echo "DNS.1 = ${CN}" >> ${EXT_FILE}
echo "DNS.2 = *.${CN}" >> ${EXT_FILE}
