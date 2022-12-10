#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

# External Scripts
GENERATE_EXT="/workspaces/Development/https-proxy/scripts/generate_ext.sh"

# Get Common Name
if [ -z "$1" ]; then
    echo "Usage: generate_cert.sh <common name>"
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
    echo "[!] Root CA certificate ${ROOTCA_CRT} not found"
    exit 1
fi

# Ensure Root CA Key exists
if [ ! -f "${ROOTCA_KEY}" ]; then
    echo "[!] Root CA key ${ROOTCA_KEY} not found"
    exit 1
fi

# Files
CSR="${LOCAL_CSRS}/${CN}.csr"
CRT="${LOCAL_CERTS}/${CN}.crt"
KEY="${LOCAL_KEYS}/${CN}.key"
EXT="${LOCAL_EXTS}/${CN}.ext"

# Generate Extension File
${GENERATE_EXT} ${CN}

# Print information
echo "[*] Generating certificate for '${CN}' in '${LOCAL_CERTS}'"
echo " -------------------------------------------------- "
echo " Root CA Certificate: ${ROOTCA_CRT}"
echo "         Root CA Key: ${ROOTCA_KEY}"
echo "         Root CA PWD: ${ROOTCA_PASSWD}"
echo " -------------------------------------------------- "
echo "                  CN: ${CN}"
echo "     Signing Request: ${CSR}"
echo "         Certificate: ${CRT}"
echo "                 Key: ${KEY}"
echo "      Extension File: ${EXT}"
echo " -------------------------------------------------- "

# Generate key for new certificate
echo "[*] Generating key for ${CN}"
openssl genrsa -out ${KEY} 2048 

# Generate certificate signing request
echo "[*] Generating certificate signing request for ${CN}"
openssl req -new -key ${KEY} -out ${CSR} -subj "${SUBJECT_DEV}${CN}"

# Generate certificate using our own root ca
echo "[*] Generating certificate for ${CN}"
openssl x509 -req                       \
        -in ${CSR}                      \
        -CA ${ROOTCA_CRT}               \
        -CAkey ${ROOTCA_KEY}            \
        -passin "file:${ROOTCA_PASSWD}" \
        -CAcreateserial                 \
        -out ${CRT} -days 365           \
        -sha256                         \
        -extfile ${EXT}

# Add certificate to local trust store
echo "[*] Adding certificate to local trust store"  
cp ${CRT} /usr/local/share/ca-certificates/${CN}.crt
update-ca-certificates
