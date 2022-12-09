#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

# Check for sudo privileges
if [[ $EUID -ne 0 ]]; then
   echo "[!] This script must be run as root" 1>&2
   exit 1
fi

# Get Common Name - use ROOT_CA_CN if not provided
if [ -z "$1" ]; then
    CN=${ROOT_CA_CN}
else
    CN=$1
fi

# if [ -z $2 ]; then
#     echo "Usage: $0 <common name> <IP>"
# else
#     IP=$2
# fi

# Get Password
PASSWD_FILE=${PASSWD_DIR}/${CN}.passwd
if [ ! -f ${PASSWD_FILE} ]; then
    echo "[!] Password file ${PASSWD_FILE} not found"
    exit 1
fi

# Read password from file
PASSWD=$(cat ${PASSWD_FILE})

# Set key, certificate (PEM)
KEY=${KEYS_DIR}/${CN}.key
CRT=${CERTS_DIR}/${CN}.crt

echo "[+] Generating certificate for ${CN} in ${CERTDIR}"
echo "[*] Certificate: ${CRT}"
echo "[*]         Key: ${KEY}"

# If key exists, remove it
if [ -f "${KEY}" ]; then
    echo "[!] Removing existing key"
    rm "${KEY}"
    # TODO - should we also remove the key from the trusted store?
fi

# If certificate exists, remove it
if [ -f "${CRT}" ]; then
    echo "[!] Removing existing certificate"
    # Remove certificate from workspace
    rm "${CRT}"
    # TODO - should we remove certificate from ca-certificates?
    # rm "${CA_CERT_DIR}/${CN}.crt"
    # TODO - should we remove certificate from trusted store?
    # rm "${TRUSTED_CA_CERT_DIR}/${CN}.crt"
fi

# Generate certificate key include password - store in /workspaces/Development/https-proxy/etc/private
echo "[*] Generating certificate key"
openssl genrsa -des3 -passout "pass:${PASSWD}" -out "${KEY}" 2048 

# Generate certificate root certificate
echo "[*] Generating certificate signing request"
openssl req -x509 -new -nodes -key "${KEY}" -sha256 -days 2048 -out "${CRT}" -subj "${SUBJECT}${CN}" -passin "pass:${PASSWD}"
# openssl req -x509 -new -nodes -sha256 -days 3650 -key "${KEY}" -out ${CRT} -subj "${SUBJECT}${CN}" -addext "subjectAltName=DNS:${CN}.localhost" -passin "pass:${PASSWD}"

# Add certificate to ca-certificates, this will update the ca-certificates.crt file and add  the
# certificate to the trusted store
echo "[*] Updating ca-certificates"
sudo cp ${CRT} ${CA_CERTS_DIR}/${CN}.crt
sudo update-ca-certificates

# Verify certificate
echo "[*] Verifying certificate"
openssl verify -CAfile "${CRT}" "${CRT}"
awk -v cmd='openssl x509 -noout -subject' '/BEGIN/{close(cmd)};{print | cmd}' < /etc/ssl/certs/ca-certificates.crt | grep ${CN}
