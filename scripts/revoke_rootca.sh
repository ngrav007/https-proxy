#!/bin/bash

# This script is used to upload the certificate and key to the server and 
# install them in the /etc/ssl directory of the server. This script is
# intended to be run on the server. It will prompt for the password to 
# decrypt the certificate and key.

source /workspaces/Development/http-proxy/scripts/config/.config

GENERATE_KEY="/workspaces/Development/http-proxy/scripts/generate_key.sh"
GENERATE_EXT="/workspaces/Development/http-proxy/scripts/generate_ext.sh"  

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi


# Remove the root ca certificate
if [ -f ${ROOTCA_CRT} ]; then
    echo "[+] Removing certificate file ${ROOTCA_CRT}"
    rm ${ROOTCA_CRT}
fi

# Remove root ca key    
if [ -f ${ROOTCA_KEY} ]; then
    echo "[+] Removing key file ${ROOTCA_KEY}"
    rm ${ROOTCA_KEY}
fi

# Remove Password file
if [ -f ${ROOTCA_PASSWD} ]; then
    echo "[+] Removing password file ${ROOTCA_PASSWD}"
    rm ${ROOTCA_PASSWD}
fi

# Remove from ca-certificates
if [ -f /usr/local/share/ca-certificates/${CN}.crt ]; then
    echo "[+] Removing ${CN}.crt certificate from ca-certificates"
    rm /usr/local/share/ca-certificates/${CN}.crt
    update-ca-certificates
fi

if [ -f /usr/local/share/ca-certificates/${CN}.pem ]; then
    echo "[+] Removing ${CN}.pem certificate from ca-certificates"
    rm /usr/local/share/ca-certificates/${CN}.pem
    update-ca-certificates
fi

echo "[*] Certificate and key removed from server"

exit 0
