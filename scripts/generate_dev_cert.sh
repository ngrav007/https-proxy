#!/bin/bash

source /workspaces/Development/https-proxy/scripts/config/.config

source /workspaces/Development/https-proxy/scripts/generate_dev_key.sh  # generate key
source /workspaces/Development/https-proxy/scripts/generate_dev_ext.sh  # generate extension file
  
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
    echo "Usage: $0 <common name>"
    exit 1
fi
CN=$1

# Key File - will force overwrite
KEY=${KEYS_DIR}/${CN}.key
echo "[*] Generating key for ${CN} in ${KEYS_DIR}"
./generate_dev_key.sh ${KEY}

if [ ! -f ${KEY} ]; then
    echo "[!] Key file ${KEY} not found"
    exit 1
fi

# Extension file - will force overwrite
EXT_FILE=${EXT_DIR}/${CN}.ext
./generate_dev_ext.sh ${EXT_FILE}

if [ ! -f ${EXT_FILE} ]; then
    echo "[!] Extension file ${EXT_FILE} not found"
    exit 1
fi

# Certificate Signing Request
CSR=${CERTS_DIR}/${CN}.csr

# Certificate
CRT=${CERTS_DIR}/${CN}.crt

echo "[*] Generating certificate for ${CN} in ${CERTS_DIR}"
echo " -------------------------------------------------- "
echo " Root CA Certificate: ${ROOT_CA_CRT}"
echo "         Root CA Key: ${ROOT_CA_KEY}"
echo " -------------------------------------------------- "
echo "                  CN: ${CN}"
echo "                 Key: ${KEY}"
echo "     Signing Request: ${CSR}"
echo "      Extension File: ${EXT_FILE}"
echo "         Certificate: ${CRT}"
echo " -------------------------------------------------- "

# Generate key for new certificate
openssl genrsa -out ${KEY} 2048 

# Generate certificate signing request
openssl req -new -key ${KEY} -out ${CSR} -subj "${SUBJECT}${CN}"

# Generate certificate
openssl x509 -req -in ${CSR} -CA ${ROOT_CA_CRT} -CAkey ${ROOT_CA_KEY} -passin file:${ROOT_CA_PASSWD} -CAcreateserial -out ${CRT} -days 825 -sha256 -extfile ${EXT_FILE}

# TODO - do we do any of this below for dev certs?
# # Add certificate to ca-certificates
# echo "[*] Updating ca-certificates"
# cp "${CRT}" "/usr/share/ca-certificates/${CN}.crt"
# update-ca-certificates

# # Add key to trusted store
# echo "[*] Updating trusted store"
# cp "${KEY}" "/etc/ssl/private/${CN}.key"

# # Update permissions
# echo "[*] Updating permissions of key"
# chown root:root ${TRUSTED_PRIVATE_KEY_DIR}/${CN}.key
# chmod 0600 ${TRUSTED_PRIVATE_KEY_DIR}/${CN}.key

# # Verify certificate
# echo "[*] Verifying certificate"
# openssl verify -CAfile "${CRT}" "${CRT}"
# awk -v cmd='openssl x509 -noout -subject' '/BEGIN/{close(cmd)};{print | cmd}' < /etc/ssl/certs/ca-certificates.crt | grep eregion
