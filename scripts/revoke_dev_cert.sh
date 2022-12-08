#!/bin/bash

# This script is used to upload the certificate and key to the server and 
# install them in the /etc/ssl directory of the server. This script is
# intended to be run on the server. It will prompt for the password to 
# decrypt the certificate and key.

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

if [ "$1" == "" ]; then
    echo "Usage: $0 <common name>"
    exit 1
fi

CN=$1

# Remove the certificate and local folder
CRT_FILE=${CERTS_DIR}/${CN}.crt
if [ ! -f ${CRT_FILE} ]; then
    echo "Certificate file ${CRT_FILE} not found"
else 
    rm ${CRT_FILE}
fi

# Remove the key and local folder
KEY_FILE=${KEYS_DIR}/${CN}.key
if [ ! -f ${KEY_FILE} ]; then
    echo "Key file ${KEY_FILE} not found"
else
    rm ${KEY_FILE}
fi

# Remove csr and local folder
CSR_FILE=${CERTS_DIR}/${CN}.csr
if [ ! -f ${CSR_FILE} ]; then
    echo "CSR file ${CSR_FILE} not found"
else
    rm ${CSR_FILE}
fi

# Remove from ca-certificates
if [ -f /usr/local/share/ca-certificates/${CN}.crt ]; then
    rm /usr/local/share/ca-certificates/${CN}.crt
    update-ca-certificates
fi

echo "Certificate and key removed from server"

exit 0
