#!/bin/bash

# This script is used to upload the certificate and key to the server and 
# install them in the /etc/ssl directory of the server. This script is
# intended to be run on the server. It will prompt for the password to 
# decrypt the certificate and key.

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

if [ "$1" == "" ]; then
    echo "[*] Usage: $0 <common name>"
    exit 1
fi

CN=$1

# Files
CRT="${LOCAL_CERTS}/${CN}.crt"
CSR="${LOCAL_CSRS}/${CN}.csr"
KEY="${LOCAL_KEYS}/${CN}.key"
EXT="${LOCAL_EXTS}/${CN}.ext"

# Remove the certificate
if [ -f ${CRT} ]; then
    echo "[+] Removing certificate file ${CRT}"
    rm ${CRT}
fi

# Remove the key
if [ -f ${KEY} ]; then
    echo "[+] Removing key file ${KEY}"
    rm ${KEY}
fi

# Remove csr and local folder
if [ -f ${CSR} ]; then
    echo "[+] Removing CSR file ${CSR}"
    rm ${CSR}
fi

# Extension for certificate
if [ -f ${EXT} ]; then
    echo "[+] Removing extension file ${EXT}"
    rm ${EXT}
fi

echo "[*] '${CN}' certificate and key successfully removed"

exit 0
